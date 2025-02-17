#pragma once

#include "module.h"
#include "logs_table.h"
#include "settings.h"

class UI
{
public:
	void initialize();
	
	void render_windows();
	void render_options();
	void render_context_menu();

	LogsTable logs_table;

private:
	void draw_display_options(SettingsData& settings);
	void draw_dps_report_options(SettingsData& settings);
	void draw_wingman_options(SettingsData& settings);
	void draw_parser_options(SettingsData& settings);
};

DECLARE_MODULE(UI, ui)
