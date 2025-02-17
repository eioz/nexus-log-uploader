#pragma once

#include "module.h"
#include "evtc.h"

#include <imgui.h>
#include <nlohmann/json.hpp>

#include <shared_mutex>

inline void to_json(nlohmann::json& j, const ImVec2& v)
{
	j = nlohmann::json{ {"x", v.x}, {"y", v.y} };
};

inline void from_json(const nlohmann::json& j, ImVec2& v)
{
	j.at("x").get_to(v.x);
	j.at("y").get_to(v.y);
};


enum class ParserUpdateChannel
{
	LATEST,
	LATEST_WINGMAN
};

enum class AutoUploadFilter
{
	NONE,
	SUCCESSFUL_ONLY
};

enum class WindowAlignment
{
	TOP_LEFT,
	TOP_RIGHT,
	BOTTOM_LEFT,
	BOTTOM_RIGHT
};

using EncounterSelection = std::vector<TriggerID>;

struct SettingsData
{
	struct DPSReport
	{
		bool auto_upload = false;
		bool auto_upload_copy_url_to_clipboard = false;

		std::string user_token = "";

		bool anonymize = false;
		bool detailed_wvw = false;

		AutoUploadFilter auto_upload_filter = AutoUploadFilter::NONE;

		EncounterSelection auto_upload_encounters;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(DPSReport, auto_upload, auto_upload_copy_url_to_clipboard, user_token, anonymize, detailed_wvw, auto_upload_filter, auto_upload_encounters)

	} dps_report;

	struct Wingman
	{
		bool auto_upload = false;

		AutoUploadFilter auto_upload_filter = AutoUploadFilter::NONE;

		EncounterSelection auto_upload_encounters;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(Wingman, auto_upload, auto_upload_filter, auto_upload_encounters)

	} wingman;

	struct Parser
	{
		bool auto_update = true;

		ParserUpdateChannel update_channel = ParserUpdateChannel::LATEST_WINGMAN;

		bool auto_parse = true;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(Parser, auto_update, update_channel, auto_parse)

	} parser;

	struct Display
	{
		struct LogTable
		{
			bool fixed_size = false;
			bool fixed_position = false;
			bool clip_to_screen = true;
			bool alternate_row_backgrounds = false;

			WindowAlignment alignment = WindowAlignment::TOP_LEFT;
			ImVec2 position = ImVec2(0, 0);
			ImVec2 size = ImVec2(800, 350);

			bool title_bar = true;
			bool background = true;
			
			bool parser_column = true;
			bool dps_report_column = true;
			bool wingman_column = true;

			bool hide_in_combat = false;

			NLOHMANN_DEFINE_TYPE_INTRUSIVE(LogTable, fixed_size, fixed_position, clip_to_screen, alternate_row_backgrounds, alignment, position, size, title_bar, background, parser_column, dps_report_column, wingman_column, hide_in_combat)

		} log_table;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(Display, log_table)
	} display;

	void verify();

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(SettingsData, dps_report, wingman, parser, display)
};

class Settings
{
public:
	void initialize();
	auto release() -> void
	{
		std::lock_guard lock(mutex);
		save();
	}

	template<typename Func>
	auto read(Func&& func) const -> decltype(func(std::declval<const SettingsData&>()))
	{
		std::shared_lock lock(mutex);
		return func(data);
	}

	template<typename Func>
	auto write(Func&& func) -> decltype(func(std::declval<SettingsData&>()))
	{
		std::unique_lock lock(mutex);
		return func(data);
	}

private:
	bool load();
	bool save();

	std::filesystem::path file_path;
	mutable std::shared_mutex mutex;

	SettingsData data;
};

DECLARE_MODULE(Settings, settings)

#define GET_SETTING(path) \
    ([]() -> decltype(auto) { \
        auto d = addon::settings->read([](const auto& s) -> decltype(auto) { return s.path; }); \
        return d; \
    }())

#define SET_SETTING(path, value) \
    do { \
        addon::settings->write([&](auto& d) { d.path = value; }); \
    } while (0)
