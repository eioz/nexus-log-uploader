#pragma once

#include "module.h"
#include <Nexus.h>
#include <mutex>

class Logger
{
public:
	void log(ELogLevel level, const char* message);

	auto log(std::string message, ELogLevel level)
	{
		log(level, message.c_str());
	}

private:
	std::mutex log_mutex;  // maybe not needed

};

DECLARE_MODULE(Logger, logger)

#define LOG(message, log_level) addon::logger->log(message, log_level)
