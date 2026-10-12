#include "wingman_uploader.h"
#include "addon.h"
#include "settings.h"

#include <cpr/cpr.h>

#include <set>

IMPLEMENT_MODULE(WingmanUploader, wingman_uploader)

#define CPR_PARAMETERS cpr::Timeout(std::chrono::seconds(180)), cpr::ProgressCallback([this](auto...) { return initialized.load(); })
#define CPR_API_PARAMETERS cpr::Timeout(std::chrono::seconds(30)), cpr::ProgressCallback([this](auto...) { return initialized.load(); })

namespace
{
// wingman reports most errors in the response
nlohmann::json get_json(const cpr::Response& response)
{
	if (response.status_code != 200)
		throw std::runtime_error("Status " + std::to_string(response.status_code) + " on " + response.url.str());

	return nlohmann::json::parse(response.text);
}

// statistics without data are left out or null
std::optional<int> get_number(const nlohmann::json& json, const char* key)
{
	if (json.contains(key) && json.at(key).is_number())
		return json.at(key).get<int>();

	return std::nullopt;
}

// ranks from F to SSS, - without a rank
std::string get_rank(const nlohmann::json& json, const char* key)
{
	if (json.contains(key) && json.at(key).is_string() && json.at(key) != "-")
		return json.at(key).get<std::string>();

	return "";
}
} // namespace

void WingmanUploader::add_log(std::shared_ptr<Log> log)
{
	if (!initialized.load() || !addon::settings->get().wingman.enabled)
		return;

	std::unique_lock lock(log->mutex);

	// allow uploading for available and failed logs
	if (!log->parser_data.has_report() || (log->wingman_upload.status != UploadStatus::AVAILABLE && log->wingman_upload.status != UploadStatus::FAILED))
	{
		addon::log("Log unavailable for wingman upload: " + log->id, LOGLEVEL_WARNING);
		return;
	}

	log->wingman_upload.status = UploadStatus::QUEUED;

	log->update_view();

	{
		std::unique_lock upload_queue_lock(upload_queue_mutex);
		this->upload_queue.push(log);
		this->upload_cv.notify_one();
	}
}

void WingmanUploader::process_auto_upload(std::shared_ptr<Log> log)
{
	auto settings = addon::settings->get().wingman;

	if (!settings.auto_upload)
		return;

	std::shared_lock lock(log->mutex);

	auto log_trigger_id = log->trigger_id;

	if (log->parser_data.status != ParseStatus::PARSED || (settings.auto_upload_filter == AutoUploadFilter::SUCCESSFUL_ONLY && !log->parser_data.encounter.success))
		return;

	lock.unlock();

	bool is_in_filter = std::find(settings.auto_upload_encounters.begin(), settings.auto_upload_encounters.end(), log_trigger_id) != settings.auto_upload_encounters.end();

	if (is_in_filter)
		this->add_log(log);
}

void WingmanUploader::request_statistics(std::shared_ptr<Log> log)
{
	if (!initialized.load() || !addon::settings->get().wingman.enabled)
		return;

	std::unique_lock upload_queue_lock(upload_queue_mutex);
	this->statistics_queue.push(log);
	this->upload_cv.notify_one();
}

void WingmanUploader::run()
{
	addon::log("Starting Wingman uploader", LOGLEVEL_DEBUG);

	while (true)
	{
		std::unique_lock upload_queue_lock(upload_queue_mutex);

		auto ready = [&]() { return !upload_queue.empty() || !statistics_queue.empty() || !initialized.load(); };

		if (link_retries.empty())
			upload_cv.wait(upload_queue_lock, ready);
		else
			upload_cv.wait_until(upload_queue_lock, link_retries.begin()->first, ready);

		if (!initialized.load())
			break;

		// due link lookups run with the statistics
		while (!link_retries.empty() && link_retries.begin()->first <= std::chrono::steady_clock::now())
		{
			statistics_queue.push(link_retries.begin()->second);
			link_retries.erase(link_retries.begin());
		}

		if (!statistics_queue.empty())
		{
			auto log = statistics_queue.front();
			statistics_queue.pop();

			upload_queue_lock.unlock();

			update_statistics(log);
			continue;
		}

		if (upload_queue.empty())
			continue;

		auto log = upload_queue.front();
		upload_queue.pop();

		upload_queue_lock.unlock();

		std::unique_lock lock(log->mutex);

		if (log->parser_data.status != ParseStatus::PARSED || log->wingman_upload.status != UploadStatus::QUEUED)
		{
			addon::log("Log unavailable for wingman upload: " + log->id, LOGLEVEL_WARNING);
			log->wingman_upload.status = UploadStatus::FAILED;
			continue;
		}

		log->wingman_upload.status = UploadStatus::UPLOADING;

		log->update_view();

		auto id = log->id;
		auto log_data = log->get_data();

		lock.unlock();

		WingmanUpload upload;

		try
		{
			upload = this->upload(log_data);
			addon::log("Wingman upload successful: " + id, LOGLEVEL_INFO);
		}
		catch (const std::exception& e)
		{
			// an upload cancelled by unloading is reset when its session is loaded again
			if (!initialized.load())
				break;

			upload.status = UploadStatus::FAILED;
			upload.error_message = e.what();
			addon::log("Wingman upload failed: " + id + ". Error: " + e.what(), LOGLEVEL_WARNING);
		}

		lock.lock();

		log->wingman_upload = upload;
		log->update_view();

		lock.unlock();

		// the report link and the player ratings need the uploaded log
		if (upload.status == UploadStatus::UPLOADED || upload.status == UploadStatus::SKIPPED)
			update_statistics(log);
	}

	addon::log("Wingman uploader stopped", LOGLEVEL_DEBUG);
}

void WingmanUploader::update_statistics(std::shared_ptr<Log> log)
{
	LogData data;

	{
		std::shared_lock lock(log->mutex);
		data = log->get_data();
	}

	if (data.parser_data.status != ParseStatus::PARSED)
		return;

	auto& encounter = data.parser_data.encounter;
	auto statistics = data.wingman_statistics;
	auto url = data.wingman_upload.url;

	// the report link comes first, failed statistics requests leave it alone
	if (url.empty() && (data.wingman_upload.status == UploadStatus::UPLOADED || data.wingman_upload.status == UploadStatus::SKIPPED))
	{
		try
		{
			cpr::Multipart multipart = { { "file", addon::to_utf8(data.evtc_file_path.filename()) }, { "filesize", std::to_string(std::filesystem::file_size(data.evtc_file_path)) }, { "bossID", std::to_string(static_cast<int>(data.trigger_id)) } };

			auto json = get_json(cpr::Post(cpr::Url(WINGMAN_URL "/checkUploadSuccessfulWithLog"), multipart, CPR_API_PARAMETERS));

			if (json.value("success", false))
				url = WINGMAN_URL "/log/" + json.at("log").at("html").get<std::string>();
		}
		catch (const std::exception& e)
		{
			if (initialized.load())
				addon::log("Failed to get the Wingman link for " + data.id + ". Error: " + e.what(), LOGLEVEL_WARNING);
		}

		if (url.empty())
			retry_link(log);

		std::unique_lock lock(log->mutex);

		log->wingman_upload.url = url;
		log->update_view();
	}

	// the first error stops this update, the logs table requests incomplete statistics again
	try
	{
		auto wingman_boss_id = get_boss_id(data);

		// wingman only has statistics of the bosses it knows
		if (!wingman_boss_id)
			return;

		auto boss_id = std::to_string(*wingman_boss_id);

		// challenge modes have their own statistics
		auto key = boss_id;

		if (encounter.difficulty == EncounterDifficulty::CHALLENGE_MODE)
			key = "-" + boss_id;
		else if (encounter.difficulty == EncounterDifficulty::LEGENDARY_CHALLENGE_MODE)
			key = "--" + boss_id;

		// legendary challenge modes have no kill percentiles
		if (!statistics.kill_percentile && encounter.success && encounter.difficulty != EncounterDifficulty::LEGENDARY_CHALLENGE_MODE)
		{
			auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(encounter.start_time.time_since_epoch()).count();

			cpr::Parameters parameters = { { "bossID", boss_id }, { "isCM", encounter.difficulty == EncounterDifficulty::CHALLENGE_MODE ? "True" : "False" }, { "duration", std::to_string(encounter.duration_ms) }, { "timestamp", std::to_string(timestamp) } };

			// patches before 26-01 have no percentiles, very slow kills get -1
			auto percentile = get_number(get_json(cpr::Get(cpr::Url(WINGMAN_URL "/api/getPercentileByMetadata"), parameters, CPR_API_PARAMETERS)), "percentile");

			if (percentile)
				statistics.kill_percentile = std::clamp(*percentile, 0, 100);
		}

		if (statistics.era.empty())
		{
			auto era = get_era(encounter.start_time);
			auto& boss = bosses[era + " " + key];

			if (boss.is_null())
				boss = get_json(cpr::Get(cpr::Url(WINGMAN_URL "/api/boss"), cpr::Parameters{ { "era", era }, { "bossID", key } }, CPR_API_PARAMETERS));

			std::string best_group;

			for (const auto& group : boss.value("group_top", nlohmann::json::array()))
				if (group.is_string())
					best_group += (best_group.empty() ? "" : ", ") + group.get<std::string>();

			statistics.era = era;
			statistics.median_duration_ms = get_number(boss, "duration_med");
			statistics.best_duration_ms = get_number(boss, "duration_top");
			statistics.best_group = best_group;
		}

		if (!statistics.personal_best_ms && encounter.success && !statistics.era.empty() && !encounter.account_name.empty())
		{
			auto json = get_json(cpr::Get(cpr::Url(WINGMAN_URL "/api/getPlayerProfessionStats"), cpr::Parameters{ { "account", encounter.account_name }, { "era", statistics.era } }, CPR_API_PARAMETERS));
			auto pointer = nlohmann::json::json_pointer("/topBossTimes/" + statistics.era + "/" + key);

			// without an earlier kill on wingman there is no personal best
			if (json.contains(pointer))
				statistics.personal_best_ms = get_number(json.at(pointer), "durationMS");
		}

		for (const auto& player : data.parser_data.players)
		{
			if (statistics.ranks.contains(player.account))
				continue;

			if (!ranks.contains(player.account))
			{
				auto account = cpr::util::urlEncode(player.account);
				auto response = cpr::Get(cpr::Url(WINGMAN_URL "/api/playerRanks/current/" + std::string(account.begin(), account.end())), CPR_API_PARAMETERS);

				// players have no rank until someone presses rerank on their wingman profile
				ranks[player.account] = response.status_code == 404 ? "" : get_rank(get_json(response), "globalRank");
			}

			statistics.ranks[player.account] = ranks[player.account];
		}

		auto unrated = std::any_of(data.parser_data.players.begin(), data.parser_data.players.end(), [&](const auto& player) { return !statistics.dps_percentiles.contains(player.account); });

		if (!url.empty() && encounter.success && unrated)
		{
			auto log_id = url.substr(url.rfind('/') + 1);

			// wingman decides who played support, private logs have no metadata
			std::set<std::string> supports;
			auto metadata = get_json(cpr::Get(cpr::Url(WINGMAN_URL "/api/getMetadata/" + log_id), CPR_API_PARAMETERS));

			if (metadata.contains("players"))
				for (const auto& player : metadata.at("players"))
					if (player.value("isSupport", 0) == 1)
						supports.insert(player.value("account", ""));

			for (const auto& player : data.parser_data.players)
			{
				if (statistics.dps_percentiles.contains(player.account))
					continue;

				nlohmann::json body = { { "logs", nlohmann::json::array({ log_id }) }, { "playerAccount", player.account } };

				auto json = get_json(cpr::Post(cpr::Url(WINGMAN_URL "/api/ratePerformanceOfPlayerInLogs"), cpr::Header{ { "Content-Type", "application/json" } }, cpr::Body(body.dump()), CPR_API_PARAMETERS));

				// patches before 26-01 are not rated
				auto percentiles = json.at(0).value("percentiles", nlohmann::json());
				auto dps_percentile = get_number(percentiles, "DamageProfession");
				auto support_percentile = get_number(percentiles, "SupportProfession");

				if (dps_percentile && *dps_percentile >= 0)
					statistics.dps_percentiles[player.account] = *dps_percentile;

				if (supports.contains(player.account) && support_percentile && *support_percentile >= 0)
					statistics.support_percentiles[player.account] = *support_percentile;
			}
		}

		statistics.complete = true;
	}
	catch (const std::exception& e)
	{
		statistics.complete = false;

		if (initialized.load())
			addon::log("Failed to get Wingman statistics for " + data.id + ". Error: " + e.what(), LOGLEVEL_WARNING);
	}

	std::unique_lock lock(log->mutex);

	log->wingman_statistics = statistics;
	log->update_view();
}

// wingman can take a moment to add an upload, a missing link is looked up three more times, two minutes apart
void WingmanUploader::retry_link(std::shared_ptr<Log> log)
{
	if (++link_failures[log] > 3)
		return;

	std::unique_lock upload_queue_lock(upload_queue_mutex);
	link_retries.emplace(std::chrono::steady_clock::now() + std::chrono::minutes(2), log);
}

// wingman counts some encounters as another boss and splits ai into its phases
std::optional<int> WingmanUploader::get_boss_id(const LogData& data)
{
	// elite insights names logs of a single phase after it
	if (data.trigger_id == TriggerID::AiKeeperOfThePeak)
	{
		auto& name = data.parser_data.encounter.name;

		if (name.starts_with("Elemental"))
			return 23254;

		if (name.starts_with("Dark"))
			return 232542;

		return 232543;
	}

	// wingman lists river of souls without target ids
	if (data.trigger_id == TriggerID::RiverOfSouls)
		return static_cast<int>(TriggerID::RiverOfSouls);

	if (boss_list.is_null())
		boss_list = get_json(cpr::Get(cpr::Url(WINGMAN_URL "/api/bosses"), CPR_API_PARAMETERS));

	auto trigger_id = static_cast<int>(data.trigger_id);

	for (const auto& [id, boss] : boss_list.items())
	{
		auto targets = boss.value("targetIDs", nlohmann::json::array());

		if (std::find(targets.begin(), targets.end(), trigger_id) != targets.end())
			return std::stoi(id);
	}

	return std::nullopt;
}

// the balance patch a log was recorded in
std::string WingmanUploader::get_era(std::chrono::system_clock::time_point time)
{
	if (patches.is_null())
		patches = get_json(cpr::Get(cpr::Url(WINGMAN_URL "/api/patches"), CPR_API_PARAMETERS)).at("patches");

	auto date = std::format("{:%F}", std::chrono::floor<std::chrono::days>(time));
	std::string era;
	std::string era_date;

	for (const auto& patch : patches)
	{
		auto from = patch.value("from", "");

		if (from <= date && from > era_date)
		{
			era = patch.value("id", "");
			era_date = from;
		}
	}

	if (era.empty())
		throw std::runtime_error("No patch for " + date);

	return era;
}

WingmanUpload WingmanUploader::upload(LogData& log_data)
{
	auto settings = addon::settings->get().wingman;

	WingmanUpload upload;
	upload.status = UploadStatus::FAILED;

	if (!std::filesystem::exists(log_data.evtc_file_path) || !std::filesystem::exists(log_data.parser_data.json_file_path) || !std::filesystem::exists(log_data.parser_data.html_file_path))
		throw std::runtime_error("Missing required files for upload (evtc, json, html)");

	if (!this->get_server_availability())
		throw std::runtime_error("Wingman servers unavailable");

	// check upload
	{
		auto get_file_creation_time = [](std::filesystem::path file_path) // this should get the same result as the elite insights wingman uploader
		{
			WIN32_FILE_ATTRIBUTE_DATA file_info;

			if (!GetFileAttributesExW(file_path.wstring().c_str(), GetFileExInfoStandard, &file_info))
				throw std::runtime_error("Unable to get file attributes for " + addon::to_utf8(file_path));

			FILETIME file_time = file_info.ftCreationTime;
			SYSTEMTIME system_time;
			SYSTEMTIME system_time_utc;

			FileTimeToSystemTime(&file_time, &system_time_utc);

			SystemTimeToTzSpecificLocalTime(NULL, &system_time_utc, &system_time);

			FILETIME local_file_time;
			SystemTimeToFileTime(&system_time, &local_file_time);

			ULARGE_INTEGER ull;
			ull.LowPart = local_file_time.dwLowDateTime;
			ull.HighPart = local_file_time.dwHighDateTime;

			constexpr uint64_t WINDOWS_TICK = 10000000ULL;
			constexpr uint64_t EPOCH_DIFFERENCE = 11644473600ULL;

			return (ull.QuadPart / WINDOWS_TICK) - EPOCH_DIFFERENCE;
		};

		auto file_size = std::filesystem::file_size(log_data.evtc_file_path);
		auto file_creation_time = get_file_creation_time(log_data.evtc_file_path);

		cpr::Multipart multipart = { { "file", addon::to_utf8(log_data.evtc_file_path.filename()) }, { "timestamp", std::to_string(file_creation_time) }, { "filesize", std::to_string(file_size) }, { "account", log_data.parser_data.encounter.account_name },
			{ "triggerID", std::to_string(static_cast<int>(log_data.trigger_id)) } };

		auto response = cpr::Post(cpr::Url(WINGMAN_URL "/checkUpload"), CPR_PARAMETERS, multipart);

		if (response.status_code != 200)
			throw std::runtime_error("Status " + std::to_string(response.status_code) + " on checkUpload");

		if (response.text == "Error")
			throw std::runtime_error("Error on checkUpload");

		if (response.text == "False")
		{
			upload.status = UploadStatus::SKIPPED;
			upload.error_message = "Log already exists";
			return upload;
		}

		if (response.text != "True")
			throw std::runtime_error("Unexpected response on checkUpload: " + response.text);
	}

	// upload processed
	{
		cpr::Multipart multipart = { { "file", cpr::File(addon::to_utf8(log_data.evtc_file_path), addon::to_utf8(log_data.evtc_file_path.filename())) }, { "jsonfile", cpr::File(addon::to_utf8(log_data.parser_data.json_file_path), addon::to_utf8(log_data.parser_data.json_file_path.filename())) },
			{ "htmlfile", cpr::File(addon::to_utf8(log_data.parser_data.html_file_path), addon::to_utf8(log_data.parser_data.html_file_path.filename())) }, { "account", log_data.parser_data.encounter.account_name } };

		auto response = cpr::Post(cpr::Url(WINGMAN_URL "/uploadProcessed"), CPR_PARAMETERS, multipart);

		if (response.status_code != 200)
			throw std::runtime_error("Status " + std::to_string(response.status_code) + " on uploadProcessed");

		if (response.text != "True")
			throw std::runtime_error("Unexpected response on uploadProcessed: " + response.text);

		upload.status = UploadStatus::UPLOADED;
	}

	return upload;
}

bool WingmanUploader::get_server_availability()
{
	static std::chrono::system_clock::time_point last_check_time = std::chrono::system_clock::now();
	static const std::chrono::seconds check_interval = std::chrono::seconds(180);
	auto current_time = std::chrono::system_clock::now();

	static auto servers_available_cached = false;

	if (!servers_available_cached || current_time - last_check_time > check_interval)
	{
		last_check_time = current_time;

		auto response = cpr::Get(cpr::Url(WINGMAN_URL "/testConnection"), CPR_PARAMETERS);

		servers_available_cached = (response.status_code == 200 && response.text == "True");

		return servers_available_cached;
	}

	return servers_available_cached;
}
