#include "logger.h"
#include "api.h"

IMPLEMENT_MODULE(Logger, logger)

void Logger::log(ELogLevel level, const char* message)
{
	std::lock_guard<std::mutex> lock(log_mutex);

	if (addon::api)
		addon::api->Log(level, ADDON_LOG_CHANNEL, message);
}
