#include "addon.h"

#include "directory_monitor.h"
#include "dps_report_uploader.h"
#include "log_manager.h"
#include "parser.h"
#include "resource.h"
#include "settings.h"
#include "ui.h"
#include "wingman_uploader.h"

#include <Nexus.h>

#define HOTKEY_UI "Open Log Uploader"
#define QUICK_ACCESS_UI "LOG_UPLOADER"
#define QUICK_ACCESS_TEXTURE "LOG_UPLOADER_ICON"

static HMODULE addon_module = nullptr;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID /*lpReserved*/)
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
		addon_module = hModule;
	return TRUE;
}

void render() { addon::ui->render_windows(); }
void render_options() { addon::ui->render_options(); }

void load(AddonAPI_t* addon_api)
{
	addon::api = addon_api;

	if (addon::api == nullptr)
		return;

	addon::mumble = static_cast<Mumble::Data*>(addon::api->DataLink_Get(DL_MUMBLE_LINK));
	addon::directory = std::filesystem::path(addon::api->Paths_GetAddonDirectory(ADDON_DIRECTORY));

	if (addon::mumble == nullptr || addon::directory.empty())
		return;

	addon::api->InputBinds_RegisterWithString(
	    HOTKEY_UI,
	    [](const char* key, bool is_released) {
		    if (strcmp(key, HOTKEY_UI) == 0 && !is_released)
			    addon::ui->logs_table.toggle_visibility();
	    },
	    "CTRL+L");

	if (HRSRC hres = FindResource(addon_module, MAKEINTRESOURCE(IDR_LOG_ICON), RT_RCDATA))
		if (HGLOBAL hdata = LoadResource(addon_module, hres))
			if (void* ptr = LockResource(hdata))
				addon::api->Textures_LoadFromMemory(QUICK_ACCESS_TEXTURE, ptr, SizeofResource(addon_module, hres), nullptr);
	addon::api->QuickAccess_Add(QUICK_ACCESS_UI, QUICK_ACCESS_TEXTURE, QUICK_ACCESS_TEXTURE, HOTKEY_UI, "Log Uploader");

	if (!std::filesystem::exists(addon::directory))
		if (!std::filesystem::create_directory(addon::directory))
		{
			addon::log("Failed to create addon directory.", LOGLEVEL_CRITICAL);
			return;
		}

	ImGui::SetCurrentContext((ImGuiContext*)addon::api->ImguiContext);
	ImGui::SetAllocatorFunctions((void* (*)(size_t, void*))addon::api->ImguiMalloc, (void (*)(void*, void*))addon::api->ImguiFree);

	addon::settings->initialize();

	addon::api->GUI_Register(RT_Render, render);
	addon::api->GUI_Register(RT_OptionsRender, render_options);

	addon::parser->initialize();
	addon::dps_report_uploader->initialize();
	addon::wingman_uploader->initialize();

	addon::directory_monitor->initialize();
}

void unload()
{
	addon::settings->release();

	addon::api->InputBinds_Deregister(HOTKEY_UI);
	addon::api->QuickAccess_Remove(QUICK_ACCESS_UI);

	addon::api->GUI_Deregister(render);
	addon::api->GUI_Deregister(render_options);

	addon::directory_monitor->release();
	addon::parser->release();
	addon::dps_report_uploader->release();
	addon::wingman_uploader->release();
}

AddonDefinition_t addon_definition;

extern "C" __declspec(dllexport) AddonDefinition_t* GetAddonDef()
{
	addon_definition.Signature = -69;
	addon_definition.APIVersion = NEXUS_API_VERSION;
	addon_definition.Name = ADDON_NAME;
	addon_definition.Version.Major = ADDON_VERSION_MAJOR;
	addon_definition.Version.Minor = ADDON_VERSION_MINOR;
	addon_definition.Version.Build = ADDON_VERSION_BUILD;
	addon_definition.Version.Revision = ADDON_VERSION_REVISION;
	addon_definition.Author = "eioz";
	addon_definition.Description = "Automatically parse new logs locally and upload them to dps.report or Wingman.";
	addon_definition.Load = load;
	addon_definition.Unload = unload;
	addon_definition.Flags = AF_None;

#ifdef _DEBUG
	addon_definition.Provider = UP_None;
#else
	addon_definition.Provider = UP_GitHub;
	addon_definition.UpdateLink = "https://github.com/eioz/nexus-log-uploader";
#endif

	return &addon_definition;
}
