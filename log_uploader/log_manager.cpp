#include "log_manager.h"
#include "evtc_parser.h"
#include "ui.h"
#include "parser.h"
#include "api.h"
#include "dps_report_uploader.h"
#include "wingman_uploader.h"
#include "logger.h"

#include <fstream>

IMPLEMENT_MODULE(LogManager, log_manager)

LogManager::~LogManager()
{
	std::unique_lock lock(logs_mutex);

	for (auto& log : logs)
	{
		std::unique_lock lock(log->mutex);

		if (log->parser_data.status == ParseStatus::PARSED)
		{
			LOG("Deleting " + log->parser_data.html_file_path.string(), ELogLevel::ELogLevel_DEBUG);

			if (std::filesystem::exists(log->parser_data.html_file_path))
				std::filesystem::remove(log->parser_data.html_file_path);

			LOG("Deleting " + log->parser_data.json_file_path.string(), ELogLevel::ELogLevel_DEBUG);

			if (std::filesystem::exists(log->parser_data.json_file_path))
				std::filesystem::remove(log->parser_data.json_file_path);
		}
	}

	logs.clear();
}

void LogManager::add_log(std::filesystem::path evtc_file_path)
{
	try
	{
		const auto data = addon::evtc_parser->parse(evtc_file_path);

		if (data.is_valid())
		{
			auto log = std::make_shared<Log>(data);

			{
				std::unique_lock lock(logs_mutex);
				logs.push_front(log);
			}

			addon::dps_report_uploader->process_auto_upload(log);

			if (GET_SETTING(parser.auto_parse))
				addon::parser->add_log(log);

			addon::ui->logs_table.add_log(log);

			LOG("Log added: " + log->id, ELogLevel::ELogLevel_INFO);
		}
		else
			throw std::exception("Invalid evtc data");
	}
	catch (const std::exception& e)
	{
		LOG("Failed to add log: Evtc parsing failed. File: \"" + evtc_file_path.string() + "\" Exception: " + e.what(), ELogLevel::ELogLevel_WARNING);
	}
}
