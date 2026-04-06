#include "platform.h"

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

static AddonAPI_t* nexus_api = nullptr;

bool nexus_registered = false;

static void nexus_log(LogLevel level, const char* message)
{
	if (nexus_api)
		nexus_api->Log(static_cast<ELogLevel>(level), ADDON_LOG_CHANNEL, message);
}

static std::filesystem::path nexus_ini_path()
{
	if (nexus_api)
		return std::filesystem::path(nexus_api->Paths_GetAddonDirectory("arcdps")) / "arcdps.ini";
	return {};
}

void render() { addon::ui->render_windows(); }
void render_options() { addon::ui->render_options(); }

void load(AddonAPI_t* addon_api)
{
	nexus_api = addon_api;

	if (nexus_api == nullptr)
		return;

	addon::mumble = static_cast<Mumble::Data*>(nexus_api->DataLink_Get(DL_MUMBLE_LINK));
	addon::directory = std::filesystem::path(nexus_api->Paths_GetAddonDirectory(ADDON_DIRECTORY));

	if (addon::mumble == nullptr || addon::directory.empty())
		return;

	addon::init_platform(Platform::Nexus, nexus_log, nexus_ini_path);

	nexus_api->InputBinds_RegisterWithString(
	    HOTKEY_UI,
	    [](const char* key, bool is_released) {
		    if (strcmp(key, HOTKEY_UI) == 0 && !is_released)
			    addon::ui->logs_table.toggle_visibility();
	    },
	    "CTRL+L");

	if (HRSRC hres = FindResource(addon::dll_module, MAKEINTRESOURCE(IDR_LOG_ICON), RT_RCDATA))
		if (HGLOBAL hdata = LoadResource(addon::dll_module, hres))
			if (void* ptr = LockResource(hdata))
				nexus_api->Textures_LoadFromMemory(QUICK_ACCESS_TEXTURE, ptr, SizeofResource(addon::dll_module, hres), nullptr);
	nexus_api->QuickAccess_Add(QUICK_ACCESS_UI, QUICK_ACCESS_TEXTURE, QUICK_ACCESS_TEXTURE, HOTKEY_UI, "Log Uploader");

	if (!std::filesystem::exists(addon::directory))
		if (!std::filesystem::create_directory(addon::directory))
		{
			nexus_api->Log(static_cast<ELogLevel>(LOGLEVEL_CRITICAL), ADDON_LOG_CHANNEL, "Failed to create addon directory.");
			return;
		}

	ImGui::SetCurrentContext((ImGuiContext*)nexus_api->ImguiContext);
	ImGui::SetAllocatorFunctions((void* (*)(size_t, void*))nexus_api->ImguiMalloc, (void (*)(void*, void*))nexus_api->ImguiFree);

	addon::settings->initialize();

	nexus_api->GUI_Register(RT_Render, render);
	nexus_api->GUI_Register(RT_OptionsRender, render_options);

	addon::parser->initialize();
	addon::dps_report_uploader->initialize();
	addon::wingman_uploader->initialize();

	addon::directory_monitor->initialize();
}

void unload()
{
	addon::settings->release();

	nexus_api->InputBinds_Deregister(HOTKEY_UI);
	nexus_api->QuickAccess_Remove(QUICK_ACCESS_UI);

	nexus_api->GUI_Deregister(render);
	nexus_api->GUI_Deregister(render_options);

	addon::directory_monitor->release();
	addon::parser->release();
	addon::dps_report_uploader->release();
	addon::wingman_uploader->release();
}

AddonDefinition_t addon_definition;

extern "C" __declspec(dllexport) AddonDefinition_t* GetAddonDef()
{
	nexus_registered = true;

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
