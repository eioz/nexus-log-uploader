#include "platform.h"

#include <mutex>

namespace addon
{
std::filesystem::path directory;
Mumble::Data* mumble = nullptr;
Platform platform = Platform::None;
HMODULE dll_module = nullptr;

static std::mutex log_mutex;
static LogFn active_log_fn = nullptr;
static IniPathFn active_ini_path_fn = nullptr;

void init_platform(Platform p, LogFn log_fn, IniPathFn ini_path_fn)
{
	platform = p;
	active_log_fn = log_fn;
	active_ini_path_fn = ini_path_fn;
}

void log(const std::string& message, LogLevel level)
{
	std::lock_guard lock(log_mutex);
	if (active_log_fn)
		active_log_fn(level, message.c_str());
}

void log(const char* message, LogLevel level)
{
	log(std::string(message), level);
}

std::filesystem::path get_arcdps_ini_path()
{
	if (active_ini_path_fn)
		return active_ini_path_fn();
	return {};
}

std::filesystem::path get_log_directory()
{
	auto ini_path = get_arcdps_ini_path();

	if (!std::filesystem::exists(ini_path))
	{
		addon::log("arcdps.ini not found at " + ini_path.string() + ", using default log path", LOGLEVEL_DEBUG);
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
} // namespace addon

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID /*lpReserved*/)
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
		addon::dll_module = hModule;
	return TRUE;
}
