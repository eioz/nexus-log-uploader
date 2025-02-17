#pragma once

#include "log.h"
#include "module.h"

#include <deque>
#include <memory>

class LogManager
{
public:
	LogManager() {}
	~LogManager();

	void add_log(std::filesystem::path evtc_file_path);

	std::deque<std::shared_ptr<Log>> logs;
	std::shared_mutex logs_mutex;

private:

};

DECLARE_MODULE(LogManager, log_manager)
