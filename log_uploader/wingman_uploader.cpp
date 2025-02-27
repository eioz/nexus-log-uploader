#include "settings.h"
#include "wingman_uploader.h"
#include "logger.h"

#include <cpr/cpr.h>

IMPLEMENT_MODULE(WingmanUploader, wingman_uploader)

#define CPR_PARAMETERS cpr::Timeout{ std::chrono::seconds(180) }
#define TEST_CONNECTION_URL "https://gw2wingman.nevermindcreations.de/testConnection"
#define CHECK_UPLOAD_URL "https://gw2wingman.nevermindcreations.de/checkUpload"
#define UPLOAD_PROCESSED_URL "https://gw2wingman.nevermindcreations.de/uploadProcessed"

void WingmanUploader::add_log(std::shared_ptr<Log> log)
{
	if (!initialized.load())
		return;

	std::unique_lock lock(log->mutex);

	// allow uploading for available and failed logs
	if (log->parser_data.status != ParseStatus::PARSED || (log->wingman_upload.status != UploadStatus::AVAILABLE && log->wingman_upload.status != UploadStatus::FAILED))
	{
		LOG("Log unavailable for wingman upload: " + log->id, ELogLevel::ELogLevel_WARNING);
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
	auto settings = GET_SETTING(wingman);

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

void WingmanUploader::run()
{
	std::unique_lock upload_lock(upload_mutex);

	LOG("Starting Wingman uploader", ELogLevel_DEBUG);

	while (true)
	{
		std::unique_lock upload_queue_lock(upload_queue_mutex);

		upload_cv.wait(upload_queue_lock, [&]() { return !upload_queue.empty() || !initialized.load(); });

		if (!initialized.load())
			break;

		auto log = upload_queue.front();
		upload_queue.pop();

		upload_queue_lock.unlock();

		std::unique_lock lock(log->mutex);

		if (log->parser_data.status != ParseStatus::PARSED || log->wingman_upload.status != UploadStatus::QUEUED)
		{
			LOG("Log unavailable for wingman upload: " + log->id, ELogLevel::ELogLevel_WARNING);
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
			LOG("Wingman upload successful: " + id, ELogLevel::ELogLevel_INFO);
		}
		catch (const std::exception& e)
		{
			upload.status = UploadStatus::FAILED;
			upload.error_message = e.what();
			LOG("Wingman upload failed: " + id + ". Error: " + e.what(), ELogLevel::ELogLevel_WARNING);
		}

		lock.lock();

		log->wingman_upload = upload;
		log->update_view();
	}

	LOG("Wingman uploader stopped", ELogLevel_DEBUG);
}

WingmanUpload WingmanUploader::upload(LogData& log_data)
{
	auto settings = GET_SETTING(wingman);

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

		cpr::Multipart multipart =
		{
			{ "file", log_data.evtc_file_path.filename().string() },
			{ "timestamp", std::to_string(file_creation_time) },
			{ "filesize", std::to_string(file_size) },
			{ "account", log_data.parser_data.encounter.account_name },
			{ "triggerID", std::to_string(static_cast<int>(log_data.trigger_id))}
		};

		auto response = cpr::Post(cpr::Url(CHECK_UPLOAD_URL), CPR_PARAMETERS, multipart);

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
		cpr::Multipart multipart =
		{
			{ "file", cpr::File(log_data.evtc_file_path.string(), log_data.evtc_file_path.filename().string())},
			{ "jsonfile", cpr::File(log_data.parser_data.json_file_path.string(), log_data.parser_data.json_file_path.filename().string()) },
			{ "htmlfile", cpr::File(log_data.parser_data.html_file_path.string(), log_data.parser_data.html_file_path.filename().string()) },
			{ "account", log_data.parser_data.encounter.account_name }
		};

		auto response = cpr::Post(cpr::Url(UPLOAD_PROCESSED_URL), CPR_PARAMETERS, multipart);

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

	static auto servers_available = false;

	if (!servers_available || current_time - last_check_time > check_interval)
	{
		last_check_time = current_time;

		auto response = cpr::Get(cpr::Url(TEST_CONNECTION_URL), CPR_PARAMETERS);

		servers_available = (response.status_code == 200 && response.text == "True");

		this->servers_available.store(servers_available);

		return servers_available;
	}

	return servers_available;
}
