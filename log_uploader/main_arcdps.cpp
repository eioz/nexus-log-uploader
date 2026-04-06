#include "arcdps.h"
#include "platform.h"

#include "directory_monitor.h"
#include "dps_report_uploader.h"
#include "log_manager.h"
#include "logger.h"
#include "parser.h"
#include "settings.h"
#include "ui.h"
#include "wingman_uploader.h"

#include <atomic>
#include <mutex>
#include <thread>

static HANDLE mumble_file_handle = nullptr;
static std::atomic<bool> initialized = false;
static std::mutex initialization_mutex;
static std::thread initialization_thread;

static HMODULE arcdps_dll = nullptr;

using ArcdpsLogFn = void (*)(char*);
using ArcdpsIniPathFn = wchar_t* (*)();

static ArcdpsLogFn arcdps_e3 = nullptr;
static ArcdpsLogFn arcdps_e8 = nullptr;
static ArcdpsIniPathFn arcdps_e0 = nullptr;

extern bool nexus_registered;

static const char* get_log_level_str(LogLevel level)
{
	switch (level)
	{
	case LOGLEVEL_CRITICAL:
		return "Critical";
	case LOGLEVEL_WARNING:
		return "Warning";
	case LOGLEVEL_INFO:
		return "Info";
	case LOGLEVEL_DEBUG:
		return "Debug";
	case LOGLEVEL_TRACE:
		return "Trace";
	default:
		return "Unknown";
	}
}

static void arcdps_log(LogLevel level, const char* message)
{
	auto formatted = std::string(ADDON_LOG_CHANNEL) + ": " + (level != LOGLEVEL_INFO ? (std::string(get_log_level_str(level)) + ": ") : "") + message;
	if (arcdps_e8)
		arcdps_e8(formatted.data());
	if (arcdps_e3)
		arcdps_e3(formatted.data());
}

static std::filesystem::path arcdps_ini_path()
{
	if (arcdps_e0)
		return std::filesystem::path(arcdps_e0());
	return {};
}

static bool open_mumble_link(const std::string& name)
{
	HANDLE handle = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(Mumble::Data), name.c_str());

	if (handle == nullptr)
		return false;

	auto* data = static_cast<Mumble::Data*>(MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Mumble::Data)));

	if (data == nullptr)
	{
		CloseHandle(handle);
		return false;
	}

	if (std::wstring(data->Name) != L"Guild Wars 2")
	{
		UnmapViewOfFile(data);
		CloseHandle(handle);
		return false;
	}

	mumble_file_handle = handle;
	addon::mumble = data;
	return true;
}

static void close_mumble_link()
{
	if (addon::mumble != nullptr)
	{
		UnmapViewOfFile(addon::mumble);
		addon::mumble = nullptr;
	}

	if (mumble_file_handle != nullptr)
	{
		CloseHandle(mumble_file_handle);
		mumble_file_handle = nullptr;
	}
}

static bool is_nexus_loaded()
{
	return nexus_registered || GetModuleHandleA("nexus_arcdps.dll") != nullptr;
}

static uintptr_t mod_wnd(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (!initialized.load())
		return uMsg;

	if (uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN)
	{
		if (wParam && !(lParam & (1 << 30)))
		{
			auto hotkey = GET_SETTING(display.hotkey);

			bool ctrl_match = !hotkey.ctrl || (GetKeyState(VK_CONTROL) & 0x8000);
			bool shift_match = !hotkey.shift || (GetKeyState(VK_SHIFT) & 0x8000);
			bool alt_match = !hotkey.alt || (GetKeyState(VK_MENU) & 0x8000);

			bool ctrl_extra = !hotkey.ctrl && (GetKeyState(VK_CONTROL) & 0x8000);
			bool shift_extra = !hotkey.shift && (GetKeyState(VK_SHIFT) & 0x8000);
			bool alt_extra = !hotkey.alt && (GetKeyState(VK_MENU) & 0x8000);

			if (hotkey.key == wParam && ctrl_match && shift_match && alt_match && !ctrl_extra && !shift_extra && !alt_extra)
			{
				addon::ui->logs_table.toggle_visibility();
				return 0;
			}
		}
	}

	return uMsg;
}

static uintptr_t mod_combat(cbtevent* ev, ag* src, ag* dst, const char* skillname, uint64_t id, uint64_t revision)
{
	if (!ev && src && dst && !src->elite && src->prof && dst->self && dst->name != nullptr)
	{
		if (!initialized.load())
			return 0;

		std::string account_name(dst->name);

		if (account_name.empty())
			return 0;

		if (account_name.front() == ':')
			account_name.erase(0, 1);

		static bool mumble_link_disabled = false;

		if (!mumble_link_disabled && addon::mumble == nullptr && !account_name.empty())
		{
			if (!open_mumble_link("MumbleLink") && !open_mumble_link("MumbleLink_" + account_name))
			{
				mumble_link_disabled = true;
				LOG("Mumble link disabled: failed to initialize under 'MumbleLink' or 'MumbleLink_" + account_name + "'", LOGLEVEL_CRITICAL);
			}
			else
				LOG("Mumble link initialized", LOGLEVEL_DEBUG);
		}
	}

	return 0;
}

static uintptr_t mod_imgui(uint32_t /*not_charsel_or_loading*/)
{
	if (!initialized.load())
		return 0;

	addon::ui->render_windows();

	return 0;
}

static uintptr_t mod_options_end()
{
	if (!initialized.load())
		return 0;

	addon::ui->render_options();

	return 0;
}

static uintptr_t mod_options_windows(const char* window_name)
{
	if (window_name)
		return 0;

	if (!initialized.load())
		return 0;

	bool visible = addon::ui->logs_table.is_visible();
	if (ImGui::Checkbox("Log Uploader", &visible))
		addon::ui->logs_table.set_visible(visible);

	return 0;
}

static arcdps_exports arc_exports;

static arcdps_exports* mod_init()
{
	memset(&arc_exports, 0, sizeof(arcdps_exports));
	arc_exports.sig = 0xE41A0F2;
	arc_exports.imguivers = IMGUI_VERSION_NUM;
	arc_exports.size = sizeof(arcdps_exports);
	arc_exports.out_name = ADDON_NAME;
	arc_exports.out_build = ADDON_VERSION_STRING;
	arc_exports.wnd_nofilter = mod_wnd;
	arc_exports.combat = mod_combat;
	arc_exports.imgui = mod_imgui;
	arc_exports.options_end = mod_options_end;
	arc_exports.options_windows = mod_options_windows;

	return &arc_exports;
}

static void mod_release()
{
	std::lock_guard lock(initialization_mutex);

	if (!initialized.exchange(false))
		return;

	if (initialization_thread.joinable())
		initialization_thread.join();

	addon::directory_monitor->release();
	addon::parser->release();
	addon::dps_report_uploader->release();
	addon::wingman_uploader->release();
	addon::settings->release();
	close_mumble_link();
}

extern "C" __declspec(dllexport) void* get_init_addr(char* arcversion, ImGuiContext* imguictx, void* id3dptr, HANDLE arcdll, void* mallocfn, void* freefn, uint32_t d3dversion)
{
	arcdps_dll = reinterpret_cast<HMODULE>(arcdll);

	arcdps_e0 = reinterpret_cast<ArcdpsIniPathFn>(GetProcAddress(arcdps_dll, "e0"));
	arcdps_e3 = reinterpret_cast<ArcdpsLogFn>(GetProcAddress(arcdps_dll, "e3"));
	arcdps_e8 = reinterpret_cast<ArcdpsLogFn>(GetProcAddress(arcdps_dll, "e8"));

	if (is_nexus_loaded())
	{
		arcdps_log(LOGLEVEL_INFO, "Nexus detected, deferring initialization to Nexus module.");
		return nullptr;
	}

	ImGui::SetCurrentContext(imguictx);
	ImGui::SetAllocatorFunctions(reinterpret_cast<void* (*)(size_t, void*)>(mallocfn), reinterpret_cast<void (*)(void*, void*)>(freefn));

	auto initialize = [&]() {
		std::lock_guard lock(initialization_mutex);

		if (initialized.load())
			return;

		wchar_t module_path[MAX_PATH] = {};
		if (GetModuleFileNameW(nullptr, module_path, MAX_PATH) == 0)
			return;

		addon::directory = std::filesystem::path(module_path).parent_path() / "addons" / ADDON_DIRECTORY;

		if (!std::filesystem::exists(addon::directory))
			if (!std::filesystem::create_directories(addon::directory))
				return;

		addon::init_platform(Platform::Arcdps, arcdps_log, arcdps_ini_path);

		open_mumble_link("MumbleLink");

		addon::settings->initialize();

		initialization_thread = std::thread([]() {
			addon::parser->initialize();
			addon::dps_report_uploader->initialize();
			addon::wingman_uploader->initialize();
			addon::directory_monitor->initialize();

			LOG("Initialized", LOGLEVEL_INFO);
		});

		initialized.store(true);
	};

	initialize();

	return reinterpret_cast<void*>(mod_init);
}

extern "C" __declspec(dllexport) void* get_release_addr()
{
	return reinterpret_cast<void*>(mod_release);
}

extern "C" __declspec(dllexport) wchar_t* get_update_url()
{
	if (is_nexus_loaded())
		return nullptr;
	static wchar_t url[] = L"https://github.com/eioz/nexus-log-uploader/releases/latest/download/log_uploader.dll";
	return url;
}
