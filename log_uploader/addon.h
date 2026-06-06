#pragma once

#include <Mumble.h>
#include <Nexus.h>

#include <filesystem>
#include <string>
#include <windows.h>

#define ADDON_VERSION_MAJOR 1
#define ADDON_VERSION_MINOR 2
#define ADDON_VERSION_BUILD 0
#define ADDON_VERSION_REVISION 0
#define ADDON_VERSION_STRING "1.2.0"

#define ADDON_NAME "Log Uploader"
#define ADDON_DIRECTORY "log-uploader"
#define ADDON_LOG_CHANNEL "Log Uploader"

enum LogLevel : int
{
	LOGLEVEL_CRITICAL = 1,
	LOGLEVEL_WARNING = 2,
	LOGLEVEL_INFO = 3,
	LOGLEVEL_DEBUG = 4,
	LOGLEVEL_TRACE = 5
};

namespace addon
{
extern std::filesystem::path directory;
extern AddonAPI_t* api;
extern Mumble::Data* mumble;

void log(const std::string& message, LogLevel level);
void log(const char* message, LogLevel level);
std::filesystem::path get_log_directory();
} // namespace addon
