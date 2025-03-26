#include "api.h"
#include "directory_monitor.h"
#include "ui.h"
#include "settings.h"
#include "parser.h"
#include "dps_report_uploader.h"
#include "wingman_uploader.h"
#include "log_manager.h"

#define HOTKEY_UI "Open Log Uploader"
#define QUICK_ACCESS_UI "LOG_UPLOADER"
#define QUICK_ACCESS_TEXTURE "LOG_UPLOADER_ICON"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH: break;
	case DLL_PROCESS_DETACH: break;
	case DLL_THREAD_ATTACH: break;
	case DLL_THREAD_DETACH: break;
	}
	return TRUE;
}

void render()
{
	addon::ui->render_windows();
}

void render_options()
{
	addon::ui->render_options();
}

bool initialized = false;

void load(AddonAPI* addon_api)
{
	addon::api = addon_api;
	addon::mumble = static_cast<Mumble::Data*>(addon::api->DataLink.Get("DL_MUMBLE_LINK"));
	addon::nexus = static_cast<NexusLinkData*>(addon::api->DataLink.Get("DL_NEXUS_LINK"));
	addon::directory = std::filesystem::path(addon::api->Paths.GetAddonDirectory(ADDON_DIRECTORY));

	if (addon::api == nullptr || addon::mumble == nullptr || addon::nexus == nullptr || addon::directory.empty())
		return;

	addon::api->InputBinds.RegisterWithString(HOTKEY_UI, [](const char* key, bool is_released) { if (strcmp(key, HOTKEY_UI) == 0 && !is_released) addon::ui->logs_table.toggle_visibility(); }, "CTRL+L");
	addon::api->QuickAccess.Add(QUICK_ACCESS_UI, QUICK_ACCESS_TEXTURE, QUICK_ACCESS_TEXTURE, HOTKEY_UI, "Log Uploader");
	addon::api->Textures.LoadFromURL(QUICK_ACCESS_TEXTURE, "https://raw.githubusercontent.com", "RaidcoreGG/Nexus/refs/heads/main/src/Resources/icons/log.png", nullptr);

	if (!std::filesystem::exists(addon::directory))
		if (!std::filesystem::create_directory(addon::directory))
		{
			addon::api->Log(ELogLevel::ELogLevel_CRITICAL, ADDON_LOG_CHANNEL, "Failed to create addon directory.");
			return;
		}

	ImGui::SetCurrentContext((ImGuiContext*)addon::api->ImguiContext);
	ImGui::SetAllocatorFunctions((void* (*)(size_t, void*))addon::api->ImguiMalloc, (void(*)(void*, void*))addon::api->ImguiFree);

	addon::settings->initialize();

	addon::api->Renderer.Register(ERenderType_Render, render);
	addon::api->Renderer.Register(ERenderType_OptionsRender, render_options);

	addon::parser->initialize();
	addon::dps_report_uploader->initialize();
	addon::wingman_uploader->initialize();

	addon::directory_monitor->initialize();
}

void unload()
{
	addon::settings->release();

	addon::api->InputBinds.Deregister(HOTKEY_UI);
	addon::api->QuickAccess.Remove(QUICK_ACCESS_UI);

	addon::api->Renderer.Deregister(render);
	addon::api->Renderer.Deregister(render_options);

	addon::directory_monitor->release();
	addon::parser->release();
	addon::dps_report_uploader->release();
	addon::wingman_uploader->release();
}

AddonDefinition addon_definition;

#define V_MAJOR 1
#define V_MINOR 0
#define V_BUILD 0
#define V_REVISION 2

extern "C" __declspec(dllexport) AddonDefinition* GetAddonDef()
{
	addon_definition.Signature = -69;
	addon_definition.APIVersion = NEXUS_API_VERSION;
	addon_definition.Name = "Log Uploader";
	addon_definition.Version.Major = V_MAJOR;
	addon_definition.Version.Minor = V_MINOR;
	addon_definition.Version.Build = V_BUILD;
	addon_definition.Version.Revision = V_REVISION;
	addon_definition.Author = "eioz";
	addon_definition.Description = "Automatically parse new logs locally and upload them to dps.report or Wingman.";
	addon_definition.Load = load;
	addon_definition.Unload = unload;
	addon_definition.Flags = EAddonFlags_None;

#ifdef _DEBUG
	addon_definition.Provider = EUpdateProvider_None;
#else
	addon_definition.Provider = EUpdateProvider_GitHub;
	addon_definition.UpdateLink = "https://github.com/eioz/nexus-log-uploader";
#endif // !_DEBUG

	return &addon_definition;
}
