#include "addon.h"

#include <mutex>

namespace addon
{
std::filesystem::path directory;
AddonAPI_t* api = nullptr;
Mumble::Data* mumble = nullptr;

namespace
{
std::mutex log_mutex;

std::filesystem::path get_log_config_path()
{
	if (api)
		return std::filesystem::path(api->Paths_GetAddonDirectory("arcdps")) / "arcdps.ini";
	return {};
}
} // namespace

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

std::filesystem::path get_log_directory()
{
	auto ini_path = get_log_config_path();

	if (!std::filesystem::exists(ini_path))
	{
		addon::log("arcdps.ini not found at " + to_utf8(ini_path) + ", using default log path", LOGLEVEL_DEBUG);
		return {};
	}

	auto ini_wpath = ini_path.wstring();
	wchar_t buffer[MAX_PATH] = {};
	GetPrivateProfileStringW(L"session", L"boss_encounter_path", L"", buffer, MAX_PATH, ini_wpath.c_str());

	if (buffer[0] == L'\0')
		GetPrivateProfileStringW(L"main", L"boss_encounter_path", L"", buffer, MAX_PATH, ini_wpath.c_str());

	std::wstring boss_path(buffer);

	if (boss_path.empty())
	{
		addon::log("boss_encounter_path is empty in arcdps.ini, using default log path", LOGLEVEL_DEBUG);
		return {};
	}

	return std::filesystem::path(boss_path) / "arcdps.cbtlogs";
}

std::string to_utf8(const std::filesystem::path& path)
{
	auto text = path.u8string();
	return std::string(text.begin(), text.end());
}

std::string get_icon_texture(unsigned resource_id)
{
	return "LOG_UPLOADER_ICON_" + std::to_string(resource_id);
}

Texture_t* get_icon(unsigned resource_id)
{
	return api ? api->Textures_Get(get_icon_texture(resource_id).c_str()) : nullptr;
}
} // namespace addon
