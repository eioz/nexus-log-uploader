#pragma once

#include "module.h"
#include "log.h"
#include "elite_insights.h"

#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>

class Parser
{
public:
	void initialize();
	void release();

	void add_log(std::shared_ptr<Log> log);

private:
	std::mutex parser_mutex;
	std::condition_variable_any parser_cv;
	std::mutex parser_queue_mutex;
	std::queue<std::shared_ptr<Log>> parser_queue;
	std::thread parser_thread;

	auto clear_parser_queue() -> void
	{
		std::unique_lock lock(this->parser_queue_mutex);
		while (!this->parser_queue.empty())
			this->parser_queue.pop();
	}

	EliteInsights elite_insights;

	std::atomic<bool> loaded = false;

	void run();
};

DECLARE_MODULE(Parser, parser)