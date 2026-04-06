#include "logger.h"

IMPLEMENT_MODULE(Logger, logger)

void Logger::log(LogLevel level, const char* message)
{
	std::lock_guard<std::mutex> lock(log_mutex);
	addon::log(level, message);
}
