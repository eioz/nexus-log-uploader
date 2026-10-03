#include "parser.h"
#include "addon.h"
#include "dps_report_uploader.h"
#include "log_manager.h"
#include "session_manager.h"
#include "ui.h"
#include "wingman_uploader.h"

IMPLEMENT_MODULE(Parser, parser)

void Parser::initialize()
{
	loaded.store(true);

	parser_thread = std::thread(&Parser::run, this);
}

void Parser::release()
{
	loaded.store(false);

	elite_insights.cancel();

	clear_parser_queue();

	parser_cv.notify_all();

	if (parser_thread.joinable())
		parser_thread.join();
}

void Parser::add_log(std::shared_ptr<Log> log)
{
	if (!loaded.load())
		return;

	std::unique_lock lock(log->mutex);

	// failed logs and logs of archived sessions can be parsed again
	auto parsable = log->parser_data.status == ParseStatus::UNPARSED || log->parser_data.status == ParseStatus::FAILED || (log->parser_data.status == ParseStatus::PARSED && !log->parser_data.has_report());

	if (!parsable)
	{
		addon::log("Log unavailable for parsing: " + log->id, LOGLEVEL_WARNING);
		return;
	}

	log->parser_data.status = ParseStatus::QUEUED;
	log->parser_data.error_message.reset();

	{
		std::unique_lock parser_queue_lock(parser_queue_mutex);
		parser_queue.push(log);
		parser_cv.notify_one();
	}
}

void Parser::run()
{
	addon::log("Starting parser", LOGLEVEL_DEBUG);

	try
	{
		elite_insights.install();
	}
	catch (const std::exception& e)
	{
		if (loaded.load())
			addon::log("Failed to install Elite Insights: " + std::string(e.what()), LOGLEVEL_CRITICAL);

		loaded.store(false);
		return;
	}

	while (true)
	{
		std::unique_lock parser_queue_lock(parser_queue_mutex);

		parser_cv.wait(parser_queue_lock, [this] { return !loaded.load() || !parser_queue.empty(); });

		if (!loaded.load())
			break;

		auto log = parser_queue.front();
		parser_queue.pop();

		parser_queue_lock.unlock();

		std::unique_lock log_lock(log->mutex);

		auto evtc_file_path = log->evtc_file_path;
		auto parsed_before = !log->parser_data.encounter.name.empty();

		log->parser_data.status = ParseStatus::PARSING;

		log->update_view();

		log_lock.unlock();

		try
		{
			auto parser_data = elite_insights.parse(evtc_file_path, addon::session_manager->get_reports_directory(log));

			{
				std::unique_lock lock(log->mutex);
				log->parser_data = parser_data;
				log->update_view();
			}

			addon::wingman_uploader->request_statistics(log);

			// logs of archived sessions keep their summary, auto upload only handles a log's first parse
			if (!parsed_before)
			{
				addon::dps_report_uploader->process_auto_upload(log);
				addon::wingman_uploader->process_auto_upload(log);
			}
		}
		catch (const std::exception& e)
		{
			std::unique_lock lock(log->mutex);
			// logs of archived sessions keep their summary
			log->parser_data.status = log->parser_data.encounter.name.empty() ? ParseStatus::FAILED : ParseStatus::PARSED;
			log->parser_data.error_message = e.what();
			addon::log("Failed to parse log with Elite Insights: " + log->id + " Exception: " + e.what(), LOGLEVEL_WARNING);
			log->update_view();
		}
	}
}
