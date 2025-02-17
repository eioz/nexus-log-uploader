#include "dps_report_uploader.h"
#include "logger.h"
#include "settings.h"

#include <cpr/cpr.h>

IMPLEMENT_MODULE(DPSReportUploader, dps_report_uploader)

#define CPR_PARAMETERS cpr::Timeout{ std::chrono::seconds(60) }
#define UPLOAD_CONTENT_URL "https://dps.report/uploadContent"

void DPSReportUploader::add_log(std::shared_ptr<Log> log)
{
	if (!initialized.load())
		return;

	std::unique_lock lock(log->mutex);

	// allow uploading for available and failed logs
	if (log->dps_report_upload.status != UploadStatus::AVAILABLE && log->dps_report_upload.status != UploadStatus::FAILED)
	{
		LOG("Log unavailable for dps.report upload: " + log->id, ELogLevel::ELogLevel_WARNING);
		return;
	}

	log->dps_report_upload.status = UploadStatus::QUEUED;
	log->update_view();

	{
		std::unique_lock upload_queue_lock(upload_queue_mutex);
		this->upload_queue.push(log);
		this->upload_cv.notify_one();
	}
}

void DPSReportUploader::process_auto_upload(std::shared_ptr<Log> log)
{
	auto settings = GET_SETTING(dps_report);

	if (!settings.auto_upload)
		return;

	std::shared_lock lock(log->mutex);

	if (log->dps_report_upload.status != UploadStatus::AVAILABLE)
		return;

	if (settings.auto_upload_filter == AutoUploadFilter::SUCCESSFUL_ONLY && !log->parser_data.is_success())
		return;

	auto log_trigger_id = log->trigger_id;
	lock.unlock();

	bool is_in_filter = std::find(settings.auto_upload_encounters.begin(), settings.auto_upload_encounters.end(), log_trigger_id) != settings.auto_upload_encounters.end();

	if (is_in_filter)
		this->add_log(log);
}

void DPSReportUploader::run()
{
	std::unique_lock upload_lock(upload_mutex);
	LOG("Starting dps.report uploader", ELogLevel::ELogLevel_DEBUG);

	while (initialized.load())
	{
		std::unique_lock upload_queue_lock(upload_queue_mutex);
		upload_cv.wait(upload_queue_lock, [&]() { return !upload_queue.empty() || !initialized.load(); });

		if (!initialized.load())
			break;

		auto log = upload_queue.front();
		upload_queue.pop();
		upload_queue_lock.unlock();

		std::unique_lock lock(log->mutex);
		if (log->dps_report_upload.status != UploadStatus::QUEUED)
		{
			LOG("Log unavailable for dps.report upload: " + log->id, ELogLevel::ELogLevel_WARNING);
			continue;
		}

		log->dps_report_upload.status = UploadStatus::UPLOADING;
		log->update_view();

		auto id = log->id;
		auto evtc_file_path = log->evtc_file_path;
		lock.unlock();

		DpsReportUpload upload;
		try
		{
			upload = this->upload(evtc_file_path);
			LOG("Uploaded " + id + " to dps.report: " + upload.url, ELogLevel::ELogLevel_INFO);

			if (GET_SETTING(dps_report.user_token).empty() && !upload.user_token.empty())
			{
				SET_SETTING(dps_report.user_token, upload.user_token);
				LOG("dps.report user token acquired: " + upload.user_token, ELogLevel::ELogLevel_INFO);
			}

			if (GET_SETTING(dps_report.auto_upload_copy_url_to_clipboard))
				ImGui::SetClipboardText(upload.url.c_str());
		}
		catch (const std::exception& e)
		{
			upload.status = UploadStatus::FAILED;
			upload.error_message = e.what();
			LOG("dps.report upload failed for " + id + ". Error: " + e.what(), ELogLevel::ELogLevel_WARNING);
		}

		lock.lock();
		log->dps_report_upload = upload;
		log->update_view();
	}

	LOG("Stopping dps.report uploader", ELogLevel::ELogLevel_DEBUG);
}

DpsReportUpload DPSReportUploader::upload(std::filesystem::path evtc_file_path)
{
	cpr::Url url(UPLOAD_CONTENT_URL);
	cpr::Parameters parameters{};
	cpr::Multipart multipart{
		{ "file", cpr::File(evtc_file_path.string(), evtc_file_path.filename().string()) },
		{ "json", "1" }
	};

	auto settings = GET_SETTING(dps_report);

	if (!settings.user_token.empty() && settings.user_token.length() == 32)
		parameters.Add({ "userToken", settings.user_token });
	if (settings.anonymize)
		parameters.Add({ "anonymous", "true" });
	if (settings.detailed_wvw)
		parameters.Add({ "detailedwvw", "true" });

	auto response = cpr::Post(url, parameters, multipart, CPR_PARAMETERS);
	DpsReportUpload upload;

	if (response.status_code == 200)
	{
		try
		{
			nlohmann::json json = nlohmann::json::parse(response.text);

			if (json.contains("error") && !json.at("error").is_null())
			{
				if (json.at("error").is_string())
					throw std::runtime_error(json.at("error").get<std::string>());
				else
					throw std::runtime_error("Json contains errors");
			}

			upload.id = json.value("id", std::string());
			upload.user_token = json.value("userToken", std::string());
			upload.url = json.value("permalink", std::string());
			upload.status = UploadStatus::UPLOADED;
		}
		catch (const nlohmann::json::exception& e)
		{
			throw std::runtime_error("Failed to parse response: " + std::string(e.what()));
		}
	}
	else if (response.status_code >= 400 && response.status_code < 500)
	{
		if (!response.text.empty())
		{
			try
			{
				auto json = nlohmann::json::parse(response.text);
				if (json.contains("error") && json.at("error").is_string())
					throw std::runtime_error("Error: " + json.at("error").get<std::string>());
			}
			catch (const nlohmann::json::exception& e)
			{
				throw std::runtime_error("Failed to parse error message: " + std::string(e.what()));
			}
		}
		throw std::runtime_error("Client error: " + std::to_string(response.status_code));
	}
	else
	{
		throw std::runtime_error("Server error: " + std::to_string(response.status_code));
	}

	return upload;
}