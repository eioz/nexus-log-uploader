#include "addon.h"

#include "platform/platform.h"

#include <mutex>

namespace addon
{
std::filesystem::path directory;
AddonAPI_t* api = nullptr;
Mumble::Data* mumble = nullptr;

static std::mutex log_mutex;

void log(const std::string& message, LogLevel level)
{
	std::lock_guard lock(log_mutex);
	if (api)
		api->Log(static_cast<ELogLevel>(level), ADDON_LOG_CHANNEL, message.c_str());
}

void log(const char* message, LogLevel level)
{
	log(std::string(message), level);
}

static std::filesystem::path get_log_config_path()
{
	if (api)
		return std::filesystem::path(api->Paths_GetAddonDirectory("arcdps")) / "arcdps.ini";
	return {};
}

std::filesystem::path get_log_directory()
{
	auto ini_path = get_log_config_path();

	if (!std::filesystem::exists(ini_path))
	{
		addon::log("arcdps.ini not found at " + ini_path.string() + ", using default log path", LOGLEVEL_DEBUG);
		return {};
	}

	auto boss_path = addon::platform::read_ini_path_value(ini_path, "session", "boss_encounter_path");

	if (boss_path.empty())
		boss_path = addon::platform::read_ini_path_value(ini_path, "main", "boss_encounter_path");

	if (boss_path.empty())
	{
		addon::log("boss_encounter_path is empty in arcdps.ini, using default log path", LOGLEVEL_DEBUG);
		return {};
	}

	return boss_path / "arcdps.cbtlogs";
}
} // namespace addon
