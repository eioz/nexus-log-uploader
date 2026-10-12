#include "log_manager.h"
#include "addon.h"
#include "dps_report_uploader.h"
#include "evtc_parser.h"
#include "parser.h"
#include "session_manager.h"
#include "wingman_uploader.h"

#include <fstream>

IMPLEMENT_MODULE(LogManager, log_manager)

void LogManager::add_log(std::filesystem::path evtc_file_path)
{
	try
	{
		const auto data = addon::evtc_parser->parse(evtc_file_path);

		if (data.is_valid())
		{
			auto log = std::make_shared<Log>(data);

			addon::session_manager->add_log(log);

			addon::dps_report_uploader->process_auto_upload(log);

			if (addon::settings->get().parser.auto_parse)
				addon::parser->add_log(log);

			addon::log("Log added: " + log->id, LOGLEVEL_INFO);
		}
		else
			throw std::runtime_error("Invalid evtc data");
	}
	catch (const std::exception& e)
	{
		addon::log("Failed to add log: Evtc parsing failed. File: \"" + addon::to_utf8(evtc_file_path) + "\" Exception: " + e.what(), LOGLEVEL_WARNING);
	}
}
