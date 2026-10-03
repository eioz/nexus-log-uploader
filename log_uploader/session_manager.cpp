#include "session_manager.h"
#include "addon.h"

#include <format>
#include <fstream>

IMPLEMENT_MODULE(SessionManager, session_manager)

#define SESSIONS_DIRECTORY "sessions"
#define INDEX_FILE "index.json"
#define SESSION_FILE "session.json"
#define REPORTS_DIRECTORY "reports"

// a new log starts a new session after this long without logs
#define SESSION_TIMEOUT std::chrono::hours(2)

namespace
{
struct SessionFile
{
	SessionInfo info;
	std::vector<LogData> logs;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(SessionFile, info, logs)
};

nlohmann::json read_json(const std::filesystem::path& path)
{
	std::ifstream file(path);

	if (!file.is_open())
		throw std::runtime_error("Failed to open " + path.string());

	return nlohmann::json::parse(file);
}

void write_json(const std::filesystem::path& path, const nlohmann::json& json)
{
	auto temporary_path = path;
	temporary_path += ".tmp";

	std::ofstream file(temporary_path, std::ios::binary | std::ios::trunc);
	file << json.dump();
	file.close();

	if (!file)
		throw std::runtime_error("Failed to write " + temporary_path.string());

	// the previous file stays intact until the new one is complete
	std::filesystem::rename(temporary_path, path);
}

SessionFile read_session(const std::filesystem::path& path)
{
	auto file = read_json(path).get<SessionFile>();

	// work that was still queued or running when the addon was unloaded
	for (auto& data : file.logs)
	{
		if (data.parser_data.status == ParseStatus::QUEUED || data.parser_data.status == ParseStatus::PARSING)
			data.parser_data.status = ParseStatus::UNPARSED;

		if (data.dps_report_upload.status == UploadStatus::QUEUED || data.dps_report_upload.status == UploadStatus::UPLOADING)
			data.dps_report_upload.status = UploadStatus::AVAILABLE;

		if (data.wingman_upload.status == UploadStatus::QUEUED || data.wingman_upload.status == UploadStatus::UPLOADING)
			data.wingman_upload.status = UploadStatus::AVAILABLE;
	}

	return file;
}
} // namespace

void SessionManager::initialize()
{
	sessions_directory = addon::directory / SESSIONS_DIRECTORY;

	try
	{
		std::filesystem::create_directories(sessions_directory);

		if (std::filesystem::exists(sessions_directory / INDEX_FILE))
			for (const auto& info : read_json(sessions_directory / INDEX_FILE).get<std::vector<SessionInfo>>())
				sessions[info.id] = info;
	}
	catch (const std::exception& e)
	{
		addon::log("Failed to read session index: " + std::string(e.what()), LOGLEVEL_WARNING);
	}

	// the index misses sessions if the addon was closed before it was saved
	try
	{
		for (const auto& entry : std::filesystem::directory_iterator(sessions_directory))
		{
			auto id = entry.path().filename().string();

			if (!entry.is_directory() || sessions.contains(id))
				continue;

			try
			{
				sessions[id] = read_session(entry.path() / SESSION_FILE).info;
			}
			catch (const std::exception& e)
			{
				addon::log("Failed to read session " + id + ": " + e.what(), LOGLEVEL_WARNING);
			}
		}

		std::erase_if(sessions, [this](const auto& session) { return !std::filesystem::exists(sessions_directory / session.first / SESSION_FILE); });
	}
	catch (const std::exception& e)
	{
		addon::log("Failed to read sessions: " + std::string(e.what()), LOGLEVEL_WARNING);
	}

	// continue the latest session after a restart or reload
	auto latest = std::max_element(sessions.begin(), sessions.end(), [](const auto& a, const auto& b) { return a.second.end_time < b.second.end_time; });

	if (latest != sessions.end() && std::chrono::system_clock::now() - latest->second.end_time < SESSION_TIMEOUT)
	{
		try
		{
			auto file = read_session(sessions_directory / latest->first / SESSION_FILE);

			for (const auto& data : file.logs)
				session_logs[latest->first].push_back(std::make_shared<Log>(data));

			sessions[latest->first] = file.info;
			current_session_id = latest->first;
		}
		catch (const std::exception& e)
		{
			addon::log("Failed to load session " + latest->first + ": " + e.what(), LOGLEVEL_WARNING);
		}
	}

	running = true;
	save_thread = std::thread(&SessionManager::run, this);
}

void SessionManager::release()
{
	{
		std::lock_guard lock(mutex);
		running = false;
	}

	save_cv.notify_all();

	if (save_thread.joinable())
		save_thread.join();

	save();
}

void SessionManager::add_log(std::shared_ptr<Log> log)
{
	std::lock_guard lock(mutex);

	auto time = log->evtc_file_time;

	if (current_session_id.empty() || time - sessions[current_session_id].end_time > SESSION_TIMEOUT)
		current_session_id = create_session(time);

	auto& info = sessions[current_session_id];
	auto& logs = session_logs[current_session_id];

	logs.push_front(log);

	info.log_count = static_cast<int>(logs.size());

	if (time > info.end_time)
		info.end_time = time;

	changed_sessions.insert(current_session_id);
}

std::deque<std::shared_ptr<Log>> SessionManager::get_logs()
{
	std::lock_guard lock(mutex);

	auto it = session_logs.find(current_session_id);

	return it != session_logs.end() ? it->second : std::deque<std::shared_ptr<Log>>();
}

std::filesystem::path SessionManager::get_reports_directory(std::shared_ptr<Log> log)
{
	std::lock_guard lock(mutex);

	for (const auto& [id, logs] : session_logs)
		if (std::find(logs.begin(), logs.end(), log) != logs.end())
			return sessions_directory / id / REPORTS_DIRECTORY;

	throw std::runtime_error("Log does not belong to a session");
}

void SessionManager::run()
{
	std::unique_lock lock(mutex);

	while (running)
	{
		save_cv.wait_for(lock, std::chrono::seconds(1));

		lock.unlock();
		save();
		lock.lock();
	}
}

void SessionManager::save()
{
	std::vector<std::pair<SessionInfo, std::deque<std::shared_ptr<Log>>>> changed;
	std::vector<SessionInfo> index;

	{
		std::lock_guard lock(mutex);

		for (const auto& [id, logs] : session_logs)
			for (const auto& log : logs)
				if (log->save_required.exchange(false))
					changed_sessions.insert(id);

		if (changed_sessions.empty())
			return;

		for (const auto& id : changed_sessions)
			changed.emplace_back(sessions[id], session_logs[id]);

		for (const auto& [id, info] : sessions)
			index.push_back(info);

		changed_sessions.clear();
	}

	for (const auto& [info, logs] : changed)
	{
		SessionFile file;
		file.info = info;

		for (const auto& log : logs)
		{
			std::shared_lock log_lock(log->mutex);
			file.logs.push_back(log->get_data());
		}

		try
		{
			std::filesystem::create_directories(sessions_directory / info.id);
			write_json(sessions_directory / info.id / SESSION_FILE, file);
		}
		catch (const std::exception& e)
		{
			addon::log("Failed to save session " + info.id + ": " + e.what(), LOGLEVEL_WARNING);
		}
	}

	try
	{
		write_json(sessions_directory / INDEX_FILE, index);
	}
	catch (const std::exception& e)
	{
		addon::log("Failed to save session index: " + std::string(e.what()), LOGLEVEL_WARNING);
	}
}

std::string SessionManager::create_session(std::chrono::system_clock::time_point time)
{
	std::chrono::zoned_time local_time = { std::chrono::current_zone(), std::chrono::floor<std::chrono::seconds>(time) };

	auto id = std::format("{:%Y-%m-%d_%H-%M-%S}", local_time);

	for (auto suffix = 2; sessions.contains(id); ++suffix)
		id = std::format("{:%Y-%m-%d_%H-%M-%S}_{}", local_time, suffix);

	auto& info = sessions[id];
	info.id = id;
	info.start_time = time;
	info.end_time = time;

	return id;
}
