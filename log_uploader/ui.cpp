#include "ui.h"
#include "ui_elements.h"
#include "api.h"
#include "log_manager.h"

IMPLEMENT_MODULE(UI, ui)

#define SAVE_SETTING(Setting) \
addon::settings->write([this, &settings](auto& _settings) \
{ \
	_settings.Setting = settings.Setting; \
});

#define UI_OPTION(ImGuiFunction, Label, Setting) \
if (ImGuiFunction(Label, &(settings.Setting))) \
{ \
	SAVE_SETTING(Setting); \
}

#define UI_CHECKBOX(Label, Setting) \
if (ImGui::Checkbox(Label, &(settings.Setting))) \
{ \
	SAVE_SETTING(Setting); \
}

#define UI_CHECKBOX_T(Label, Setting, TooltipText) \
UI_CHECKBOX(Label, Setting); \
ImGui::HoverTooltip(TooltipText);

#define UI_COMBO(Label, Setting, Items) \
if(ImGui::Combo(Label, reinterpret_cast<int*>(&(settings.Setting)), Items)) \
{ \
	SAVE_SETTING(Setting); \
}

void UI::initialize()
{

}

void UI::render_windows()
{
	logs_table.render();
}

void UI::render_options()
{
	auto settings = addon::settings->read([this](auto& settings) { return settings; });

	if (ImGui::BeginTabBar("OptionsTabBar"))
	{
		if (ImGui::BeginTabItem("Display Options"))
		{
			draw_display_options(settings);
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Parser Options"))
		{
			draw_parser_options(settings);
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("DPS Report Options"))
		{
			draw_dps_report_options(settings);
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Wingman Options"))
		{
			draw_wingman_options(settings);
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}
}

void UI::render_context_menu()
{
	auto settings = addon::settings->read([this](auto& settings) { return settings; });

	if (ImGui::BeginMenu("Display")) { draw_display_options(settings); ImGui::EndMenu(); }
	if (ImGui::BeginMenu("Parser")) { draw_parser_options(settings); ImGui::EndMenu(); }
	ImGui::SetNextWindowSize(ImVec2(800, 0)); // ???
	if (ImGui::BeginMenu("dps.report")) { draw_dps_report_options(settings); ImGui::EndMenu(); }
	ImGui::SetNextWindowSize(ImVec2(800, 0));
	if (ImGui::BeginMenu("Wingman")) { draw_wingman_options(settings); ImGui::EndMenu(); }
}

void UI::draw_display_options(SettingsData& settings)
{
	ImGui::ID id("Display Settings");

	UI_CHECKBOX("Fixed size", display.log_table.fixed_size);
	if (!settings.display.log_table.fixed_size)
	{
		ImGui::SameLine();
		if (ImGui::Button("Set to current size"))
		{
			settings.display.log_table.fixed_size = true;
			settings.display.log_table.size = this->logs_table.window_size;
			SAVE_SETTING(display.log_table);
		}
	}

	if (settings.display.log_table.fixed_size)
	{
		if (ImGui::InputFloat("Width", &settings.display.log_table.size.x, 1.f, 10.f, "%.0f"))
			SAVE_SETTING(display.log_table.size.x);

		if (ImGui::InputFloat("Height", &settings.display.log_table.size.y, 1.f, 10.f, "%.0f"))
			SAVE_SETTING(display.log_table.size.y);
	}

	UI_CHECKBOX("Fixed Position", display.log_table.fixed_position);
	if (settings.display.log_table.fixed_position)
	{
		UI_COMBO("Alignment", display.log_table.alignment, "Top Left\0Top Right\0Bottom Left\0Bottom Right\0");

		if (ImGui::InputFloat("Horizontal offset", &settings.display.log_table.position.x, 1.f, 10.f, "%.0f"))
			SAVE_SETTING(display.log_table.position.x);

		if (ImGui::InputFloat("Vertical offset", &settings.display.log_table.position.y, 1.f, 10.f, "%.0f"))
			SAVE_SETTING(display.log_table.position.y);
	}
	UI_CHECKBOX("Clip to screen", display.log_table.clip_to_screen);
	UI_CHECKBOX("Title Bar", display.log_table.title_bar);
	UI_CHECKBOX("Background", display.log_table.background);
	UI_CHECKBOX("Alternate row backgrounds", display.log_table.alternate_row_backgrounds);
	UI_CHECKBOX("Hide in combat", display.log_table.hide_in_combat);
	ImGui::Spacing();
	ImGui::Text("Columns");
	UI_CHECKBOX("Parser", display.log_table.parser_column);
	UI_CHECKBOX("DPS Report", display.log_table.dps_report_column);
	UI_CHECKBOX("Wingman", display.log_table.wingman_column);
}

void UI::draw_dps_report_options(SettingsData& settings)
{
	ImGui::ID id("DPS Report Settings");

	UI_CHECKBOX_T("Auto upload", dps_report.auto_upload, "Automatically upload new logs based on selected encounters and filter options");

	UI_COMBO("Auto upload result filter", dps_report.auto_upload_filter, "None\0Successful only\0");
	UI_CHECKBOX_T("Auto upload copy url to clipboard", dps_report.auto_upload_copy_url_to_clipboard, "Automatically copy the dps.report url to clipboard after upload");

	// User Token
	{
		char user_token[32 + 1] = {};
		strncpy_s(user_token, settings.dps_report.user_token.c_str(), sizeof(user_token) - 1);
		static bool toggle_password = false;

		if (ImGui::InputText("User token", user_token, sizeof(user_token), !toggle_password ? ImGuiInputTextFlags_Password : 0))
		{
			std::string token = user_token;

			if (token.length() == 0 || token.length() == 32)
			{
				settings.dps_report.user_token = token;
				SAVE_SETTING(dps_report.user_token);
			}
		}
		ImGui::HoverTooltip("Your personal dps.report user token. Will be automatically aquired on first upload if not specified.");
		ImGui::SameLine();
		ImGui::Checkbox("##Show user token", &toggle_password);
		ImGui::HoverTooltip("Reveal user token");
	}

	UI_CHECKBOX_T("Anonymize", dps_report.anonymize, "Player names will be anonymized.");
	UI_CHECKBOX_T("Detailed WvW", dps_report.detailed_wvw, "Enable detailed WvW reports.");

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	UI_OPTION(ImGui::EncounterSelector, "Auto Upload Encounter Selection", dps_report.auto_upload_encounters);
}

void UI::draw_wingman_options(SettingsData& settings)
{
	ImGui::ID id("Wingman Settings");

	UI_CHECKBOX_T("Auto upload", wingman.auto_upload, "Automatically upload new logs based on selected encounters and filter options");
	UI_COMBO("Auto upload result filter", wingman.auto_upload_filter, "None\0Successful only\0");

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	UI_OPTION(ImGui::EncounterSelector, "Auto Upload Encounter Selection", wingman.auto_upload_encounters);
}

void UI::draw_parser_options(SettingsData& settings)
{
	ImGui::ID id("Parser Settings");

	UI_CHECKBOX_T("Auto parse", parser.auto_parse, "Automatically parse new (z)evtc files");
	UI_CHECKBOX_T("Auto update", parser.auto_update, "Automatically check for new Elite Insights version on startup and install if available");
	UI_COMBO("Update channel", parser.update_channel, "Latest\0Latest (Wingman)\0");
}
