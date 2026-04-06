#pragma once

#include <Mumble.h>

#include <filesystem>
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

enum class Platform
{
	None,
	Nexus,
	Arcdps
};

namespace addon
{
extern std::filesystem::path directory;
extern Mumble::Data* mumble;
extern Platform platform;
extern HMODULE dll_module;

using LogFn = void (*)(LogLevel, const char*);
using IniPathFn = std::filesystem::path (*)();

void init_platform(Platform p, LogFn log_fn, IniPathFn ini_path_fn);
void log(LogLevel level, const char* message);
std::filesystem::path get_arcdps_ini_path();
std::filesystem::path get_log_directory();
} // namespace addon
