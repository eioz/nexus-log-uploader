#include "wingman_uploader.h"
#include "addon.h"
#include "settings.h"

#include <cpr/cpr.h>

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

		upload_cv.wait(upload_queue_lock, [&]() { return !upload_queue.empty() || !statistics_queue.empty() || !initialized.load(); });

		if (!initialized.load())
			break;

		if (!statistics_queue.empty())
		{
			auto log = statistics_queue.front();
			statistics_queue.pop();

			upload_queue_lock.unlock();

			update_statistics(log);
			continue;
		}

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

	auto& encounter = data.parser_data.encounter;

	// wingman only has statistics of bosses
	if (data.parser_data.status != ParseStatus::PARSED || !encounter.has_boss)
		return;

	auto statistics = data.wingman_statistics;
	auto url = data.wingman_upload.url;
	auto boss_id = std::to_string(static_cast<int>(data.trigger_id));

	// missing statistics are requested again with the next update, the first error stops this one
	try
	{
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
			auto key = boss_id;

			if (encounter.difficulty == EncounterDifficulty::CHALLENGE_MODE)
				key = "-" + boss_id;
			else if (encounter.difficulty == EncounterDifficulty::LEGENDARY_CHALLENGE_MODE)
				key = "--" + boss_id;

			auto& boss = bosses[era + " " + key];

			if (boss.is_null())
				boss = get_json(cpr::Get(cpr::Url(WINGMAN_URL "/api/boss"), cpr::Parameters{ { "era", era }, { "bossID", key } }, CPR_API_PARAMETERS));

			statistics.era = era;
			statistics.median_duration_ms = get_number(boss, "duration_med");
			statistics.best_duration_ms = get_number(boss, "duration_top");
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
				auto rank = response.status_code == 404 ? nlohmann::json() : get_json(response).value("globalRank", nlohmann::json());

				ranks[player.account] = rank.is_string() && rank != "-" ? rank.get<std::string>() : "";
			}

			statistics.ranks[player.account] = ranks[player.account];
		}

		if (url.empty() && (data.wingman_upload.status == UploadStatus::UPLOADED || data.wingman_upload.status == UploadStatus::SKIPPED))
		{
			cpr::Multipart multipart = { { "file", data.evtc_file_path.filename().string() }, { "filesize", std::to_string(std::filesystem::file_size(data.evtc_file_path)) }, { "bossID", boss_id } };

			auto json = get_json(cpr::Post(cpr::Url(WINGMAN_URL "/checkUploadSuccessfulWithLog"), multipart, CPR_API_PARAMETERS));

			if (json.value("success", false))
				url = WINGMAN_URL "/log/" + json.at("log").at("html").get<std::string>();
		}

		if (!url.empty() && encounter.success)
		{
			auto log_id = url.substr(url.rfind('/') + 1);

			for (const auto& player : data.parser_data.players)
			{
				if (statistics.dps_percentiles.contains(player.account))
					continue;

				nlohmann::json body = { { "logs", nlohmann::json::array({ log_id }) }, { "playerAccount", player.account } };

				auto json = get_json(cpr::Post(cpr::Url(WINGMAN_URL "/api/ratePerformanceOfPlayerInLogs"), cpr::Header{ { "Content-Type", "application/json" } }, cpr::Body(body.dump()), CPR_API_PARAMETERS));

				// patches before 26-01 are not rated
				auto percentile = get_number(json.at(0).value("percentiles", nlohmann::json()), "DamageProfession");

				if (percentile && *percentile >= 0)
					statistics.dps_percentiles[player.account] = *percentile;
			}
		}
	}
	catch (const std::exception& e)
	{
		if (initialized.load())
			addon::log("Failed to get Wingman statistics for " + data.id + ". Error: " + e.what(), LOGLEVEL_WARNING);
	}

	std::unique_lock lock(log->mutex);

	log->wingman_statistics = statistics;
	log->wingman_upload.url = url;
	log->update_view();
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
				throw std::runtime_error("Unable to get file attributes for " + file_path.string());

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

		cpr::Multipart multipart = { { "file", log_data.evtc_file_path.filename().string() }, { "timestamp", std::to_string(file_creation_time) }, { "filesize", std::to_string(file_size) }, { "account", log_data.parser_data.encounter.account_name },
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
		cpr::Multipart multipart = { { "file", cpr::File(log_data.evtc_file_path.string(), log_data.evtc_file_path.filename().string()) }, { "jsonfile", cpr::File(log_data.parser_data.json_file_path.string(), log_data.parser_data.json_file_path.filename().string()) },
			{ "htmlfile", cpr::File(log_data.parser_data.html_file_path.string(), log_data.parser_data.html_file_path.filename().string()) }, { "account", log_data.parser_data.encounter.account_name } };

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

		this->servers_available.store(servers_available_cached);

		return servers_available_cached;
	}

	return servers_available_cached;
}
