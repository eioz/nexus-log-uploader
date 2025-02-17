#include "parser.h"
#include "log_manager.h"
#include "ui.h"
#include "wingman_uploader.h"
#include "dps_report_uploader.h"
#include "logger.h"

IMPLEMENT_MODULE(Parser, parser)

void Parser::initialize()
{
	loaded.store(true);

	parser_thread = std::thread(&Parser::run, this);
}

void Parser::release()
{
	loaded.store(false);

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

	if (log->parser_data.status != ParseStatus::UNPARSED)
	{
		LOG("Log unavailable for parsing: " + log->id, ELogLevel::ELogLevel_WARNING);
		return;
	}

	log->parser_data.status = ParseStatus::QUEUED;

	{
		std::unique_lock parser_queue_lock(parser_queue_mutex);
		parser_queue.push(log);
		parser_cv.notify_one();
	}
}

void Parser::run()
{
	std::unique_lock parser_lock(parser_mutex);

	LOG("Starting parser", ELogLevel_DEBUG);

	try
	{
		elite_insights.install();
	}
	catch (const std::exception& e)
	{
		LOG("Failed to install Elite Insights: " + std::string(e.what()), ELogLevel::ELogLevel_CRITICAL);
		return;
	}

	while (true)
	{
		parser_cv.wait(parser_lock, [this] { return !loaded.load() || !parser_queue.empty(); });

		if (!loaded.load())
			break;

		std::unique_lock parser_queue_lock(parser_queue_mutex);

		auto log = parser_queue.front();
		parser_queue.pop();

		parser_queue_lock.unlock();

		std::unique_lock log_lock(log->mutex);

		auto evtc_file_path = log->evtc_file_path;

		log->parser_data.status = ParseStatus::PARSING;

		log->update_view();

		log_lock.unlock();

		try
		{
			auto parser_data = elite_insights.parse(evtc_file_path);

			{
				std::unique_lock lock(log->mutex);
				log->parser_data = parser_data;
				log->update_view();
			}

			addon::dps_report_uploader->process_auto_upload(log);
			addon::wingman_uploader->process_auto_upload(log);
		}
		catch (const std::exception& e)
		{
			std::unique_lock lock(log->mutex);
			log->parser_data.status = ParseStatus::FAILED;
			log->parser_data.error_message = e.what();
			LOG("Failed to parse log with Elite Insights: " + log->id + " Exception: " + e.what(), ELogLevel::ELogLevel_WARNING);
			log->update_view();
		}
	}
}
