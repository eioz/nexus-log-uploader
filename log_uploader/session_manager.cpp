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

	// the directory name identifies the session
	file.info.id = path.parent_path().filename().string();

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

			if (!entry.is_directory() || sessions.contains(id) || !std::filesystem::exists(entry.path() / SESSION_FILE))
				continue;

			try
			{
				sessions[id] = read_session(entry.path() / SESSION_FILE).info;
				index_save_required = true;
			}
			catch (const std::exception& e)
			{
				addon::log("Failed to read session " + id + ": " + e.what(), LOGLEVEL_WARNING);
			}
		}

		if (std::erase_if(sessions, [this](const auto& session) { return !std::filesystem::exists(sessions_directory / session.first / SESSION_FILE); }) > 0)
			index_save_required = true;
	}
	catch (const std::exception& e)
	{
		addon::log("Failed to read sessions: " + std::string(e.what()), LOGLEVEL_WARNING);
	}

	// continue the latest session after a restart or reload
	auto latest = std::max_element(sessions.begin(), sessions.end(), [](const auto& a, const auto& b) { return a.second.end_time < b.second.end_time; });

	if (latest != sessions.end() && !latest->second.archived && std::chrono::system_clock::now() - latest->second.end_time < SESSION_TIMEOUT)
	{
		auto id = latest->first;

		load_session(id);

		if (session_logs.contains(id))
			current_session_id = id;
	}

	running = true;
	session_thread = std::thread(&SessionManager::run, this);
}

void SessionManager::release()
{
	{
		std::lock_guard lock(mutex);
		running = false;
	}

	session_cv.notify_all();

	if (session_thread.joinable())
		session_thread.join();

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
	index_save_required = true;
}

void SessionManager::start_new_session()
{
	std::lock_guard lock(mutex);
	current_session_id.clear();
}

bool SessionManager::set_archived(const std::string& session_id, bool archived)
{
	std::lock_guard lock(mutex);

	auto logs = session_logs.find(session_id);

	if (logs == session_logs.end())
		return false;

	if (archived)
	{
		for (const auto& log : logs->second)
		{
			std::shared_lock log_lock(log->mutex);

			if (log->is_busy())
				return false;
		}

		// the summaries stay, the reports can be parsed again from the original logs
		for (const auto& log : logs->second)
		{
			std::unique_lock log_lock(log->mutex);
			log->parser_data.html_file_path.clear();
			log->parser_data.json_file_path.clear();
			log->update_view();
		}

		remove_requests.push_back(sessions_directory / session_id / REPORTS_DIRECTORY);

		if (current_session_id == session_id)
			current_session_id.clear();
	}

	sessions[session_id].archived = archived;
	changed_sessions.insert(session_id);
	index_save_required = true;

	return true;
}

bool SessionManager::delete_session(const std::string& session_id)
{
	std::lock_guard lock(mutex);

	auto session = sessions.find(session_id);

	if (session == sessions.end() || !session->second.archived)
		return false;

	auto logs = session_logs.find(session_id);

	if (logs != session_logs.end())
	{
		for (const auto& log : logs->second)
		{
			std::shared_lock log_lock(log->mutex);

			if (log->is_busy())
				return false;
		}

		session_logs.erase(logs);
	}

	sessions.erase(session);
	changed_sessions.erase(session_id);
	remove_requests.push_back(sessions_directory / session_id);
	index_save_required = true;

	return true;
}

std::vector<SessionInfo> SessionManager::get_sessions()
{
	std::lock_guard lock(mutex);

	std::vector<SessionInfo> result;

	for (const auto& [id, info] : sessions)
		result.push_back(info);

	std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.start_time > b.start_time; });

	return result;
}

std::optional<SessionInfo> SessionManager::get_session(const std::string& session_id)
{
	std::lock_guard lock(mutex);

	auto it = sessions.find(session_id);

	if (it == sessions.end())
		return std::nullopt;

	return it->second;
}

std::deque<std::shared_ptr<Log>> SessionManager::get_logs(const std::string& session_id)
{
	std::lock_guard lock(mutex);

	auto id = session_id.empty() ? current_session_id : session_id;
	auto it = session_logs.find(id);

	if (it != session_logs.end())
		return it->second;

	// older sessions are loaded in the background when they are first viewed
	if (sessions.contains(id))
	{
		load_requests.insert(id);
		session_cv.notify_one();
	}

	return {};
}

std::string SessionManager::get_current_session_id()
{
	std::lock_guard lock(mutex);
	return current_session_id;
}

std::filesystem::path SessionManager::get_reports_directory(std::shared_ptr<Log> log)
{
	std::unique_lock lock(mutex);

	std::filesystem::path directory;

	for (const auto& [id, logs] : session_logs)
		if (std::find(logs.begin(), logs.end(), log) != logs.end())
			directory = sessions_directory / id / REPORTS_DIRECTORY;

	if (directory.empty())
		throw std::runtime_error("Log does not belong to a session");

	auto removing = [&] { return std::find(remove_requests.begin(), remove_requests.end(), directory) != remove_requests.end(); };
	auto kept = [&] { return std::find(kept_reports.begin(), kept_reports.end(), directory) != kept_reports.end(); };

	// a parse right after archiving would lose its reports otherwise
	remove_cv.wait(lock, [&] { return !removing() || kept(); });

	if (removing())
		throw std::runtime_error("Failed to save the archived session");

	return directory;
}

void SessionManager::run()
{
	std::unique_lock lock(mutex);

	while (running)
	{
		session_cv.wait_for(lock, std::chrono::seconds(1), [this] { return !running || !load_requests.empty(); });

		auto requested_sessions = load_requests;
		load_requests.clear();

		lock.unlock();

		for (const auto& id : requested_sessions)
			load_session(id);

		save();

		lock.lock();
	}
}

void SessionManager::load_session(const std::string& id)
{
	try
	{
		auto file = read_session(sessions_directory / id / SESSION_FILE);

		std::deque<std::shared_ptr<Log>> logs;

		for (const auto& data : file.logs)
			logs.push_back(std::make_shared<Log>(data));

		std::lock_guard lock(mutex);

		if (sessions.contains(id) && !session_logs.contains(id))
		{
			sessions[id] = file.info;
			session_logs[id] = logs;
		}
	}
	catch (const std::exception& e)
	{
		addon::log("Failed to load session " + id + ": " + e.what(), LOGLEVEL_WARNING);

		// hide it instead of retrying, its files stay untouched
		std::lock_guard lock(mutex);
		sessions.erase(id);
	}
}

void SessionManager::save()
{
	std::vector<std::pair<SessionInfo, std::deque<std::shared_ptr<Log>>>> changed;
	std::vector<SessionInfo> index;
	std::vector<std::filesystem::path> removed_paths;
	std::vector<std::filesystem::path> kept_paths;
	auto save_index = false;

	{
		std::lock_guard lock(mutex);

		for (const auto& [id, logs] : session_logs)
			for (const auto& log : logs)
				if (log->save_required.exchange(false))
					changed_sessions.insert(id);

		for (const auto& id : changed_sessions)
			changed.emplace_back(sessions[id], session_logs[id]);

		if (index_save_required)
			for (const auto& [id, info] : sessions)
				index.push_back(info);

		save_index = index_save_required;
		removed_paths = remove_requests;

		changed_sessions.clear();
		index_save_required = false;
		kept_reports.clear();
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

			// the reports of an archived session stay until its file no longer refers to them
			kept_paths.push_back(sessions_directory / info.id / REPORTS_DIRECTORY);
		}
	}

	if (save_index)
	{
		try
		{
			write_json(sessions_directory / INDEX_FILE, index);
		}
		catch (const std::exception& e)
		{
			addon::log("Failed to save session index: " + std::string(e.what()), LOGLEVEL_WARNING);
		}
	}

	for (const auto& path : removed_paths)
	{
		if (std::find(kept_paths.begin(), kept_paths.end(), path) != kept_paths.end())
			continue;

		std::error_code ec;

		// the session file goes first so a partly removed session is not loaded again
		std::filesystem::remove(path / SESSION_FILE, ec);
		std::filesystem::remove_all(path, ec);

		if (ec)
			addon::log("Failed to remove " + path.string() + ": " + ec.message(), LOGLEVEL_WARNING);
	}

	if (removed_paths.empty())
		return;

	// kept reports and requests added meanwhile stay for the next save
	{
		std::lock_guard lock(mutex);

		for (const auto& path : removed_paths)
			if (std::find(kept_paths.begin(), kept_paths.end(), path) == kept_paths.end())
				remove_requests.erase(std::find(remove_requests.begin(), remove_requests.end(), path));

		kept_reports = kept_paths;
	}

	remove_cv.notify_all();
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
