#include "log_manager.h"
#include "dps_report_uploader.h"
#include "evtc_parser.h"
#include "parser.h"
#include "platform.h"
#include "ui.h"
#include "wingman_uploader.h"

#include <fstream>

IMPLEMENT_MODULE(LogManager, log_manager)

LogManager::~LogManager()
{
	try
	{
		std::unique_lock lock(logs_mutex);

		for (auto& log : logs)
		{
			std::unique_lock log_lock(log->mutex);

			if (log->parser_data.status == ParseStatus::PARSED)
			{
				std::error_code ec;

				if (std::filesystem::exists(log->parser_data.html_file_path, ec))
					std::filesystem::remove(log->parser_data.html_file_path, ec);

				if (std::filesystem::exists(log->parser_data.json_file_path, ec))
					std::filesystem::remove(log->parser_data.json_file_path, ec);
			}
		}

		logs.clear();
	}
	catch (...)
	{
	}
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

			if (addon::settings->get().parser.auto_parse)
				addon::parser->add_log(log);

			addon::ui->logs_table.add_log(log);

			addon::log("Log added: " + log->id, LOGLEVEL_INFO);
		}
		else
			throw std::runtime_error("Invalid evtc data");
	}
	catch (const std::exception& e)
	{
		addon::log("Failed to add log: Evtc parsing failed. File: \"" + evtc_file_path.string() + "\" Exception: " + e.what(), LOGLEVEL_WARNING);
	}
}
