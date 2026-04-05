#pragma once

#include "module.h"
#include <Nexus.h>
#include <mutex>

class Logger
{
public:
	void log(ELogLevel level, const char* message);

	void log(ELogLevel level, const std::string& message) { log(level, message.c_str()); }

private:
	std::mutex log_mutex;
};

DECLARE_MODULE(Logger, logger)

#define LOG(message, log_level) addon::logger->log(log_level, message)
