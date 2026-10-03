#include "logs_table.h"
#include "addon.h"
#include "boons.h"
#include "dps_report_uploader.h"
#include "parser.h"
#include "professions.h"
#include "resource.h"
#include "session_manager.h"
#include "ui.h"
#include "ui_elements.h"
#include "wingman_uploader.h"

#undef min
#undef max

#define SUCCESS_COLOR ImVec4(0.45f, 0.8f, 0.45f, 1.f)
#define FAILURE_COLOR ImVec4(0.9f, 0.45f, 0.4f, 1.f)

namespace
{
// gray up to 30, then blue at 60 and purple at 100 like gw2 raidar
ImVec4 get_percentile_color(int percentile)
{
	static const ImVec4 gray(0.6f, 0.58f, 0.61f, 1.f);
	static const ImVec4 blue(0.36f, 0.56f, 0.94f, 1.f);
	static const ImVec4 purple(0.69f, 0.4f, 0.94f, 1.f);

	auto blend = [](const ImVec4& from, const ImVec4& to, float t) { return ImVec4(std::lerp(from.x, to.x, t), std::lerp(from.y, to.y, t), std::lerp(from.z, to.z, t), 1.f); };

	if (percentile <= 30)
		return gray;

	if (percentile <= 60)
		return blend(gray, blue, (percentile - 30) / 30.f);

	return blend(blue, purple, (std::min(percentile, 100) - 60) / 40.f);
}

// ranks from F to SSS use the percentile colors
ImVec4 get_rank_color(const std::string& rank)
{
	static const std::vector<std::string> ranks = { "F", "E", "D", "C", "C+", "B", "B+", "A", "A+", "S", "SS", "SSS" };

	auto it = std::find(ranks.begin(), ranks.end(), rank);

	if (it == ranks.end())
		return ImGui::GetStyleColorVec4(ImGuiCol_Text);

	return get_percentile_color(static_cast<int>(100 * (it - ranks.begin()) / (ranks.size() - 1)));
}

std::string format_duration(int duration_ms)
{
	return std::format("{}m {}s", duration_ms / 60000, duration_ms / 1000 % 60);
}

std::string get_session_name(const SessionInfo& session)
{
	std::chrono::zoned_time local_time = { std::chrono::current_zone(), std::chrono::floor<std::chrono::minutes>(session.start_time) };

	return std::format("{:%a %d %b %Y, %H:%M} ({})", local_time, session.log_count);
}

const char* get_difficulty_name(EncounterDifficulty difficulty)
{
	switch (difficulty)
	{
	case EncounterDifficulty::CHALLENGE_MODE:
		return "Challenge Mode";
	case EncounterDifficulty::LEGENDARY_CHALLENGE_MODE:
		return "Legendary Challenge Mode";
	default:
		return "Normal Mode";
	}
}

std::string format_dps(int dps)
{
	return dps >= 1000 ? std::format("{:.1f}k", dps / 1000.0) : std::to_string(dps);
}

void text_right(const std::string& text)
{
	auto offset = ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(text.c_str()).x;

	if (offset > 0)
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);

	ImGui::TextUnformatted(text.c_str());
}

void icon_centered(unsigned resource_id, float height, const char* fallback)
{
	auto* texture = addon::get_icon(resource_id);

	if (!texture || !texture->Resource)
	{
		ImGui::TextUnformatted(fallback);
		return;
	}

	auto width = height * texture->Width / texture->Height;
	auto offset = (ImGui::GetContentRegionAvail().x - width) * 0.5f;

	if (offset > 0)
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);

	ImGui::Image(texture->Resource, ImVec2(width, height));
}
} // namespace

void LogsTable::render()
{
	std::lock_guard lock(mutex);

	update_entries();

	auto settings = addon::settings->get();
	auto& display_settings = settings.display.log_table;

	auto open = this->open;

	if (display_settings.hide_in_combat)
	{
		if (addon::mumble != nullptr)
			in_combat = addon::mumble->Context.IsInCombat;

		if (in_combat)
			open = in_combat_override;
		else
			in_combat_override = false;
	}

	if (!open)
		return;

	auto window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNavFocus;

	if (!display_settings.title_bar)
		window_flags |= ImGuiWindowFlags_NoTitleBar;

	if (!display_settings.background)
	{
		window_flags |= ImGuiWindowFlags_NoBackground;
		ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_TableBorderLight, ImVec4(0, 0, 0, 0));
	}

	if (display_settings.fixed_size)
		window_flags |= ImGuiWindowFlags_NoResize;

	if (display_settings.fixed_position)
		window_flags |= ImGuiWindowFlags_NoMove;

	if (ImGui::Begin("Log Uploader", &this->open, window_flags))
	{
		{
			auto screen_size = ImGui::GetIO().DisplaySize;
			auto window_size = display_settings.fixed_size ? display_settings.size : ImGui::GetWindowSize();

			if (display_settings.fixed_position && !(ImGui::IsMouseDragging(0) || ImGui::IsItemActive()))
			{
				auto get_aligned_position = [](WindowAlignment alignment, ImVec2& windowSize, ImVec2& screenSize) -> ImVec2 {
					switch (alignment)
					{
					case WindowAlignment::TOP_LEFT:
						return ImVec2(0, 0);
					case WindowAlignment::TOP_RIGHT:
						return ImVec2(screenSize.x - windowSize.x, 0);
					case WindowAlignment::BOTTOM_LEFT:
						return ImVec2(0, screenSize.y - windowSize.y);
					case WindowAlignment::BOTTOM_RIGHT:
						return ImVec2(screenSize.x - windowSize.x, screenSize.y - windowSize.y);
					default:
						return ImVec2(0, 0);
					}
				};

				auto window_position = get_aligned_position(display_settings.alignment, window_size, screen_size);

				window_position.x += display_settings.position.x;
				window_position.y += display_settings.position.y;

				ImGui::SetWindowPos(window_position);
			}

			if (display_settings.clip_to_screen)
			{
				auto window_position = ImGui::GetWindowPos();

				auto target_position = ImVec2(std::clamp(window_position.x, 0.0f, std::max(screen_size.x - window_size.x, 0.f)), std::clamp(window_position.y, 0.0f, std::max(screen_size.y - window_size.y, 0.f)));

				if (target_position != window_position)
					ImGui::SetWindowPos(target_position);
			}

			this->window_size = window_size;

			if (display_settings.fixed_size)
				ImGui::SetWindowSize(window_size);
		}

		if (!viewed_session_id.empty())
		{
			auto session = addon::session_manager->get_session(viewed_session_id);

			if (session)
			{
				if (ImGui::SmallButton("Back"))
					viewed_session_id.clear();

				ImGui::SameLine();
				ImGui::Text("%s%s", session->archived ? "Archived: " : "", get_session_name(*session).c_str());
			}
			else
				viewed_session_id.clear();
		}

		auto table_flags = ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable;

		if (display_settings.alternate_row_backgrounds)
			table_flags |= ImGuiTableFlags_RowBg;

		static bool select_all_toggle = false;
		bool select_all = false;
		bool all_selected = true;

		std::vector<Columns> columns;

		columns.push_back(Columns::SELECT);
		columns.push_back(Columns::TIME);
		columns.push_back(Columns::NAME);
		columns.push_back(Columns::RESULT);
		columns.push_back(Columns::DURATION);

		if (display_settings.parser_column)
			columns.push_back(Columns::PARSER);

		if (display_settings.dps_report_column && settings.dps_report.enabled)
			columns.push_back(Columns::DPS_REPORT);

		if (display_settings.wingman_column && settings.wingman.enabled)
			columns.push_back(Columns::WINGMAN);

		if (ImGui::BeginTable("Logs Table", static_cast<int>(columns.size()), table_flags))
		{
			for (auto column : columns)
			{
				switch (column)
				{
				case Columns::SELECT:
					ImGui::TableSetupColumn("##Select All", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, ImGui::GetFrameHeight());
					break;
				case Columns::TIME:
					ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, std::max(ImGui::CalcTextSize("Time").x, ImGui::CalcTextSize("00:00").x));
					break;
				case Columns::NAME:
					ImGui::TableSetupColumn("Encounter", ImGuiTableColumnFlags_WidthStretch);
					break;
				case Columns::RESULT:
					ImGui::TableSetupColumn("Result", ImGuiTableColumnFlags_WidthFixed, 100.f);
					break;
				case Columns::DURATION:
					ImGui::TableSetupColumn("Duration", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, std::max(ImGui::CalcTextSize("Duration").x, ImGui::CalcTextSize("00m 00s 000ms").x));
					break;
				case Columns::PARSER:
					ImGui::TableSetupColumn("Report", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, std::max(ImGui::CalcTextSize("Unavailable").x, ImGui::CalcTextSize("dps.report").x));
					break;
				case Columns::DPS_REPORT:
					ImGui::TableSetupColumn("dps.report", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, std::max(ImGui::CalcTextSize("Unavailable").x, ImGui::CalcTextSize("dps.report").x));
					break;
				case Columns::WINGMAN:
					ImGui::TableSetupColumn("Wingman", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, std::max(ImGui::CalcTextSize("Unavailable").x, ImGui::CalcTextSize("dps.report").x));
					break;
				default:
					break;
				}
			}

			ImGui::TableNextRow(ImGuiTableRowFlags_Headers);

			{
				auto index = 0;

				for (auto column : columns)
				{
					ImGui::TableSetColumnIndex(index++);
					if (column == Columns::SELECT)
					{
						if (!entries.empty() && ImGui::Checkbox("##Select All", &select_all_toggle))
							select_all = true;
					}
					else
					{
						if (column == Columns::PARSER || column == Columns::DPS_REPORT || column == Columns::WINGMAN)
							ImGui::CenterNextTextItemHorizontally(ImGui::TableGetColumnName(index - 1));

						ImGui::TextUnformatted(ImGui::TableGetColumnName(index - 1));
					}
				}
			}

			for (auto& entry : entries)
			{
				ImGui::ID id(&entry.ptr);

				auto& view = entry.view;

				if (select_all)
					view.selected = select_all_toggle;

				if (!view.selected)
					all_selected = false;

				ImGui::TableNextRow();

				auto index = 0;

				for (auto column : columns)
				{
					ImGui::TableSetColumnIndex(index++);
					switch (column)
					{
					case Columns::SELECT:
						ImGui::Checkbox("##Select", &view.selected);
						break;
					case Columns::TIME:
						ImGui::TextUnformatted(view.time.c_str());
						if (ImGui::IsItemHovered())
						{
							entry.refresh_time_ago();
							ImGui::SetTooltip("%s", view.time_ago.c_str());
						}
						break;
					case Columns::NAME:
						ImGui::TextUnformatted(view.name.c_str());

						if (ImGui::IsItemHovered() && entry.data.parser_data.status == ParseStatus::PARSED)
						{
							if (tooltip_log != entry.ptr)
								tooltip_position = ImGui::GetMousePos();

							tooltip_log = entry.ptr;
							tooltip_time = ImGui::GetTime();
						}
						break;
					case Columns::RESULT:
						ImGui::TextUnformatted(view.result.c_str());
						break;
					case Columns::DURATION:
						ImGui::TextUnformatted(view.duration.c_str());
						break;
					case Columns::PARSER:
						ImGui::ButtonParser(entry.ptr, entry.data);
						break;
					case Columns::DPS_REPORT:
						ImGui::ButtonDPSReportUpload(entry.ptr, entry.data.dps_report_upload);
						break;
					case Columns::WINGMAN:
						ImGui::ButtonWingmanUpload(entry.ptr, entry.data.wingman_upload, entry.data.parser_data);
						break;
					default:
						break;
					}
				}
			}

			select_all_toggle = all_selected;

			ImGui::EndTable();
		}
	}

	draw_tooltip(settings);
	draw_context_menu(settings);

	if (!display_settings.background)
		ImGui::PopStyleColor(2);

	ImGui::End();
}

void LogsTable::update_entries()
{
	auto logs = addon::session_manager->get_logs(viewed_session_id);

	auto changed = logs.size() != entries.size() || !std::equal(logs.begin(), logs.end(), entries.begin(), [](const auto& log, const auto& entry) { return log == entry.ptr; });

	if (changed)
	{
		std::deque<LogTableEntry> updated_entries;

		// existing entries keep their selection
		for (const auto& log : logs)
		{
			auto existing = std::find_if(entries.begin(), entries.end(), [&](const auto& entry) { return entry.ptr == log; });
			updated_entries.push_back(existing != entries.end() ? *existing : LogTableEntry(log));
		}

		entries = updated_entries;
	}

	for (auto& entry : entries)
		entry.update();
}

void LogsTable::draw_context_menu(const SettingsData& settings)
{
	if (!ImGui::BeginPopupContextWindow("Context Menu"))
		return;

	enum class LogSelection : int
	{
		LAST,
		SELECTED,
		ALL,
		COUNT
	};

	static std::deque<std::reference_wrapper<LogTableEntry>> last_log_deque;
	static std::deque<std::reference_wrapper<LogTableEntry>> selected_logs_deque;

	for (auto i = 0; i < static_cast<int>(LogSelection::COUNT); ++i)
	{
		auto selection = static_cast<LogSelection>(i);
		std::deque<std::reference_wrapper<LogTableEntry>>* current_logs = nullptr;
		std::string menu_label;

		switch (selection)
		{
		case LogSelection::LAST:
			if (!entries.empty())
			{
				last_log_deque.clear();
				last_log_deque.push_back(std::ref(entries.front()));
				current_logs = &last_log_deque;
			}
			menu_label = "Last log";
			break;

		case LogSelection::SELECTED:
			selected_logs_deque.clear();
			for (auto& e : entries)
				if (e.view.selected)
					selected_logs_deque.push_back(std::ref(e));
			current_logs = &selected_logs_deque;
			menu_label = "Selected logs";
			break;

		case LogSelection::ALL:
			menu_label = "All logs (" + std::to_string(entries.size()) + ")";
			break;

		default:
			break;
		}

		bool is_all = (selection == LogSelection::ALL);

		if (current_logs && !current_logs->empty() && selection != LogSelection::LAST)
			menu_label += " (" + std::to_string(current_logs->size()) + ")";

		if (ImGui::BeginMenu(menu_label.c_str()))
		{
			bool has_logs = is_all ? !entries.empty() : (current_logs && !current_logs->empty());

			if (has_logs)
			{
				enum class LogAction : int
				{
					PARSE,
					OPEN_REPORTS,
					UPLOAD_TO_DPS_REPORT,
					COPY_DPS_REPORT_URLS,
					UPLOAD_TO_WINGMAN,
					COUNT
				};

				std::array<std::deque<std::reference_wrapper<LogTableEntry>>, static_cast<size_t>(LogAction::COUNT)> action_logs;

				auto fill_action_logs = [&](LogTableEntry& e) {
					if (e.data.parser_data.has_report())
					{
						action_logs[static_cast<size_t>(LogAction::OPEN_REPORTS)].push_back(e);

						if (settings.wingman.enabled && (e.data.wingman_upload.status == UploadStatus::AVAILABLE || e.data.wingman_upload.status == UploadStatus::FAILED))
						{
							action_logs[static_cast<size_t>(LogAction::UPLOAD_TO_WINGMAN)].push_back(e);
						}
					}
					else if (e.data.parser_data.status == ParseStatus::UNPARSED || e.data.parser_data.status == ParseStatus::PARSED || e.data.parser_data.status == ParseStatus::FAILED)
					{
						action_logs[static_cast<size_t>(LogAction::PARSE)].push_back(e);
					}

					if (!settings.dps_report.enabled)
						return;

					if (e.data.dps_report_upload.status == UploadStatus::AVAILABLE || e.data.dps_report_upload.status == UploadStatus::FAILED)
					{
						action_logs[static_cast<size_t>(LogAction::UPLOAD_TO_DPS_REPORT)].push_back(e);
					}

					if (e.data.dps_report_upload.status == UploadStatus::UPLOADED && !e.data.dps_report_upload.url.empty())
					{
						action_logs[static_cast<size_t>(LogAction::COPY_DPS_REPORT_URLS)].push_back(e);
					}
				};

				if (is_all)
				{
					for (auto& e : entries)
						fill_action_logs(e);
				}
				else
				{
					for (auto& entry : *current_logs)
						fill_action_logs(entry.get());
				}

				bool any_option = false;

				{
					auto& logs_for_parse = action_logs[static_cast<size_t>(LogAction::PARSE)];

					if (!logs_for_parse.empty())
					{
						any_option = true;

						if (ImGui::MenuItem(("Parse (" + std::to_string(logs_for_parse.size()) + ")").c_str()))
						{
							for (auto& entry : logs_for_parse)
								addon::parser->add_log(entry.get().ptr);
						}
					}
				}

				{
					auto& logs_for_open = action_logs[static_cast<size_t>(LogAction::OPEN_REPORTS)];

					if (!logs_for_open.empty())
					{
						any_option = true;

						if (ImGui::MenuItem(("Open reports (" + std::to_string(logs_for_open.size()) + ")").c_str()))
						{
							for (auto& entry : logs_for_open)
								ShellExecuteW(nullptr, L"open", entry.get().data.parser_data.html_file_path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
						}
					}
				}

				{
					auto& logs_for_dps = action_logs[static_cast<size_t>(LogAction::UPLOAD_TO_DPS_REPORT)];

					if (!logs_for_dps.empty())
					{
						any_option = true;

						if (ImGui::MenuItem(("Upload to dps.report (" + std::to_string(logs_for_dps.size()) + ")").c_str()))
						{
							for (auto& entry : logs_for_dps)
								addon::dps_report_uploader->add_log(entry.get().ptr);
						}
					}
				}

				{
					auto& logs_for_copy = action_logs[static_cast<size_t>(LogAction::COPY_DPS_REPORT_URLS)];

					if (!logs_for_copy.empty())
					{
						any_option = true;

						if (ImGui::BeginMenu(("Copy dps.report URLs (" + std::to_string(logs_for_copy.size()) + ")").c_str()))
						{
							if (ImGui::MenuItem("As raw list"))
							{
								std::string urls;
								for (auto& entry : logs_for_copy)
									urls += entry.get().data.dps_report_upload.url + "\n";
								if (!urls.empty())
									ImGui::SetClipboardText(urls.c_str());
							}
							if (ImGui::MenuItem("As markdown links"))
							{
								std::stringstream ss;
								for (auto& entry : logs_for_copy)
								{
									auto& e = entry.get();
									if (e.data.parser_data.status == ParseStatus::PARSED)
									{
										ss << "[" << e.view.name << " (" << e.view.duration;
										if (!e.data.parser_data.encounter.success)
											ss << " | " << (100.f - e.data.parser_data.encounter.health_percent_burned) << "% left";
										ss << ")](" << e.data.dps_report_upload.url << ")\n";
									}
									else
									{
										ss << "[" << e.view.name << "](" << e.data.dps_report_upload.url << ")\n";
									}
								}
								ImGui::SetClipboardText(ss.str().c_str());
							}
							ImGui::EndMenu();
						}
					}
				}

				{
					auto& logs_for_wingman = action_logs[static_cast<size_t>(LogAction::UPLOAD_TO_WINGMAN)];

					if (!logs_for_wingman.empty())
					{
						any_option = true;

						if (ImGui::MenuItem(("Upload to Wingman (" + std::to_string(logs_for_wingman.size()) + ")").c_str()))
						{
							for (auto& entry : logs_for_wingman)
								addon::wingman_uploader->add_log(entry.get().ptr);
						}
					}
				}

				if (!any_option)
					ImGui::TextDisabled("No options available");
			}
			else
				ImGui::TextDisabled(selection == LogSelection::SELECTED ? "No logs selected" : "No logs available");

			ImGui::EndMenu();
		}
	}

	ImGui::Separator();
	draw_session_menu();
	ImGui::Separator();
	addon::ui->render_context_menu();
	ImGui::EndPopup();
}

void LogsTable::draw_session_menu()
{
	if (!ImGui::BeginMenu("Sessions"))
		return;

	if (ImGui::MenuItem("New session"))
	{
		addon::session_manager->start_new_session();
		viewed_session_id.clear();
	}

	ImGui::Separator();

	auto current_session_id = addon::session_manager->get_current_session_id();
	auto displayed_session_id = viewed_session_id.empty() ? current_session_id : viewed_session_id;
	auto sessions = addon::session_manager->get_sessions();

	auto draw_sessions = [&](bool archived) {
		for (const auto& session : sessions)
		{
			if (session.archived != archived)
				continue;

			ImGui::ID id(session.id);

			if (ImGui::MenuItem(get_session_name(session).c_str(), nullptr, session.id == displayed_session_id))
				viewed_session_id = session.id == current_session_id ? "" : session.id;
		}
	};

	draw_sessions(false);

	auto has_archived_sessions = std::any_of(sessions.begin(), sessions.end(), [](const auto& session) { return session.archived; });

	if (ImGui::BeginMenu("Archived", has_archived_sessions))
	{
		draw_sessions(true);
		ImGui::EndMenu();
	}

	auto displayed_session = std::find_if(sessions.begin(), sessions.end(), [&](const auto& session) { return session.id == displayed_session_id; });

	if (displayed_session != sessions.end())
	{
		auto busy = std::any_of(entries.begin(), entries.end(), [](const auto& entry) { return entry.data.is_busy(); });

		ImGui::Separator();
		ImGui::TextDisabled("%s", get_session_name(*displayed_session).c_str());

		if (!displayed_session->archived)
		{
			if (ImGui::BeginMenu("Archive", !busy))
			{
				ImGui::TextDisabled("Removes the Elite Insights reports");

				if (ImGui::MenuItem("Confirm") && addon::session_manager->set_archived(displayed_session->id, true))
					viewed_session_id = displayed_session->id;

				ImGui::EndMenu();
			}
		}
		else
		{
			if (ImGui::MenuItem("Unarchive"))
				addon::session_manager->set_archived(displayed_session->id, false);

			if (ImGui::BeginMenu("Delete", !busy))
			{
				ImGui::TextDisabled("Removes the session from the history");

				if (ImGui::MenuItem("Confirm") && addon::session_manager->delete_session(displayed_session->id))
					viewed_session_id.clear();

				ImGui::EndMenu();
			}
		}
	}

	ImGui::EndMenu();
}

void LogsTable::draw_tooltip(const SettingsData& settings)
{
	auto& display_settings = settings.display.log_table;

	// stays open while its log or the tooltip itself is hovered, so it can be scrolled
	if (!tooltip_log || ImGui::GetTime() - tooltip_time > 0.3 || ImGui::IsPopupOpen("Context Menu"))
	{
		tooltip_log.reset();
		return;
	}

	auto entry = std::find_if(entries.begin(), entries.end(), [this](const auto& entry) { return entry.ptr == tooltip_log; });

	if (entry == entries.end())
	{
		tooltip_log.reset();
		return;
	}

	auto& parser_data = entry->data.parser_data;
	auto& encounter = parser_data.encounter;
	auto screen_size = ImGui::GetIO().DisplaySize;
	auto font_size = ImGui::GetFontSize();

	// next to where the hover started and within the screen
	auto position = tooltip_position + ImVec2(font_size, font_size);
	position.x = std::clamp(position.x, 0.f, std::max(screen_size.x - tooltip_size.x, 0.f));
	position.y = std::clamp(position.y, 0.f, std::max(screen_size.y - tooltip_size.y, 0.f));

	ImGui::SetNextWindowPos(position);
	ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), screen_size);

	auto window_flags = ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

	if (ImGui::Begin("##Encounter Tooltip", nullptr, window_flags))
	{
		// also while its scrollbar is dragged
		if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
			tooltip_time = ImGui::GetTime();

		auto icon = EncounterIcons.find(entry->data.trigger_id);
		auto* portrait = icon != EncounterIcons.end() ? addon::get_icon(icon->second) : nullptr;

		if (portrait && portrait->Resource)
		{
			ImGui::Image(portrait->Resource, ImVec2(font_size * 3.f, font_size * 3.f));
			ImGui::SameLine();
		}

		ImGui::BeginGroup();

		auto* nexus_link = static_cast<NexusLinkData_t*>(addon::api->DataLink_Get(DL_NEXUS_LINK));

		if (nexus_link && nexus_link->FontBig)
			ImGui::PushFont(static_cast<ImFont*>(nexus_link->FontBig));

		ImGui::TextUnformatted(encounter.name.c_str());

		if (nexus_link && nexus_link->FontBig)
			ImGui::PopFont();

		std::string mode = get_difficulty_name(encounter.difficulty);
		auto instance = EncounterInstanceNames.find(entry->data.trigger_id);

		if (instance != EncounterInstanceNames.end())
			mode = instance->second + " | " + mode;

		ImGui::TextDisabled("%s", mode.c_str());

		std::chrono::zoned_time end_time = { std::chrono::current_zone(), std::chrono::floor<std::chrono::seconds>(encounter.end_time) };

		ImGui::TextColored(encounter.success ? SUCCESS_COLOR : FAILURE_COLOR, "%s", entry->view.result.c_str());
		ImGui::SameLine();
		ImGui::TextDisabled("| %s | %s", entry->view.duration.c_str(), std::format("{:%d %b %Y, %H:%M}", end_time).c_str());

		// saved statistics stay hidden while wingman is turned off
		static const WingmanStatistics no_statistics;
		auto& statistics = settings.wingman.enabled ? entry->data.wingman_statistics : no_statistics;
		std::string details;

		if (statistics.median_duration_ms && statistics.best_duration_ms)
			details = std::format("Median {} | Best {}", format_duration(*statistics.median_duration_ms), format_duration(*statistics.best_duration_ms));

		if (!statistics.era.empty())
			details += (details.empty() ? "Era " : " | Era ") + statistics.era;

		if (statistics.kill_percentile)
		{
			ImGui::TextColored(get_percentile_color(*statistics.kill_percentile), "Faster than %d%% of kills", *statistics.kill_percentile);

			if (!details.empty())
			{
				ImGui::SameLine();
				ImGui::TextDisabled("| %s", details.c_str());
			}
		}
		else if (!details.empty())
			ImGui::TextDisabled("%s", details.c_str());

		ImGui::EndGroup();
		ImGui::Spacing();

		// players grouped by subgroup, highest boss dps first
		std::vector<const PlayerSummary*> players;

		for (const auto& player : parser_data.players)
			players.push_back(&player);

		std::sort(players.begin(), players.end(), [](const auto* a, const auto* b) { return a->group != b->group ? a->group < b->group : a->boss_dps > b->boss_dps; });

		std::vector<const BoonDefinition*> boons;

		for (const auto& boon : Boons)
			if (std::find(display_settings.tooltip_boons.begin(), display_settings.tooltip_boons.end(), boon.id) != display_settings.tooltip_boons.end())
				boons.push_back(&boon);

		auto show_breakbar = std::any_of(players.begin(), players.end(), [](const auto* player) { return player->breakbar_damage > 0; });
		auto show_ranks = std::any_of(statistics.ranks.begin(), statistics.ranks.end(), [](const auto& rank) { return !rank.second.empty(); });
		auto show_percentiles = !statistics.dps_percentiles.empty();
		auto column_count = 3 + (show_ranks ? 1 : 0) + (show_percentiles ? 1 : 0) + (display_settings.tooltip_downs_deaths ? 2 : 0) + (show_breakbar ? 1 : 0) + static_cast<int>(boons.size());
		auto table_left = ImGui::GetCursorScreenPos().x;

		if (ImGui::BeginTable("Squad", column_count, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX | ImGuiTableFlags_RowBg))
		{
			// a regular row, so the labels count towards the column widths
			ImGui::TableNextRow();
			ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImGuiCol_TableHeaderBg));
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Player");

			if (show_ranks)
			{
				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Rank");
			}

			ImGui::TableNextColumn();
			text_right("Boss DPS");

			// the dps percentile has no label of its own
			if (show_percentiles)
				ImGui::TableNextColumn();

			ImGui::TableNextColumn();
			text_right("Cleave DPS");

			if (display_settings.tooltip_downs_deaths)
			{
				ImGui::TableNextColumn();
				icon_centered(IDR_COMBAT_DOWNED, font_size, "Downs");
				ImGui::TableNextColumn();
				icon_centered(IDR_COMBAT_DEAD, font_size, "Deaths");
			}

			if (show_breakbar)
			{
				ImGui::TableNextColumn();
				icon_centered(IDR_COMBAT_CC, font_size, "CC");
			}

			for (const auto* boon : boons)
			{
				ImGui::TableNextColumn();
				icon_centered(boon->icon, font_size, boon->name);
			}

			// measured in the last column, the table is not limited to the window width
			auto table_width = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x + ImGui::GetStyle().CellPadding.x - table_left;
			auto group = -1;

			for (const auto* player : players)
			{
				if (player->group != group)
				{
					group = player->group;

					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TextDisabled("Subgroup %d", group);
				}

				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				auto profession = ProfessionIcons.find(player->profession);
				auto* icon = profession != ProfessionIcons.end() ? addon::get_icon(profession->second) : nullptr;

				// the white icons follow the text color of the theme
				if (icon && icon->Resource)
					ImGui::Image(icon->Resource, ImVec2(font_size, font_size), ImVec2(0, 0), ImVec2(1, 1), ImGui::GetStyleColorVec4(ImGuiCol_Text));
				else
					ImGui::Dummy(ImVec2(font_size, font_size));

				ImGui::SameLine();
				ImGui::TextUnformatted(player->name.c_str());

				if (show_ranks)
				{
					ImGui::TableNextColumn();

					auto rank = statistics.ranks.find(player->account);

					if (rank != statistics.ranks.end() && !rank->second.empty())
						ImGui::TextColored(get_rank_color(rank->second), "%s", rank->second.c_str());
				}

				ImGui::TableNextColumn();
				text_right(format_dps(player->boss_dps));

				if (show_percentiles)
				{
					ImGui::TableNextColumn();

					auto percentile = statistics.dps_percentiles.find(player->account);

					if (percentile != statistics.dps_percentiles.end())
					{
						ImGui::PushStyleColor(ImGuiCol_Text, get_percentile_color(percentile->second));
						text_right(std::format("{}%", percentile->second));
						ImGui::PopStyleColor();
					}
				}

				ImGui::TableNextColumn();
				text_right(format_dps(player->cleave_dps));

				if (display_settings.tooltip_downs_deaths)
				{
					ImGui::TableNextColumn();
					text_right(std::to_string(player->downs));
					ImGui::TableNextColumn();
					text_right(std::to_string(player->deaths));
				}

				if (show_breakbar)
				{
					ImGui::TableNextColumn();
					text_right(std::format("{:.0f}", player->breakbar_damage));
				}

				for (const auto* boon : boons)
				{
					auto uptime = player->boon_uptimes.contains(boon->id) ? player->boon_uptimes.at(boon->id) : 0.f;

					ImGui::TableNextColumn();
					text_right(boon->stacks ? std::format("{:.1f}", uptime) : std::format("{:.0f}%", uptime));
				}
			}

			ImGui::EndTable();

			// tooltips are sized to their content, leave room for the scrollbar of large squads
			if (ImGui::GetScrollMaxY() > 0)
				ImGui::Dummy(ImVec2(table_width + ImGui::GetStyle().ScrollbarSize, 0));
		}

		tooltip_size = ImGui::GetWindowSize();
	}

	ImGui::End();
}

void LogTableEntry::update_view()
{
	if (data.parser_data.status == ParseStatus::PARSED)
	{
		auto& encounter = data.parser_data.encounter;

		auto time = std::chrono::clock_cast<std::chrono::system_clock>(encounter.end_time);
		std::chrono::zoned_time local_time = { std::chrono::current_zone(), time };

		view.time = std::format("{:%H:%M}", local_time);
		view.name = encounter.name;
		view.result = encounter.success ? "Success" : encounter.has_boss ? std::format("{:.2f}%", 100.f - encounter.health_percent_burned)
		                                                                 : "Failure";

		auto update_duration = [&]() {
			auto minutes = encounter.duration_ms / (60 * 1000);
			auto seconds = (encounter.duration_ms / 1000) % 60;
			auto milliseconds = encounter.duration_ms % 1000;

			std::ostringstream oss;
			if (minutes > 0)
				oss << std::setfill('0') << std::setw(1) << minutes << "m ";
			oss << std::setfill('0') << std::setw(1) << seconds << "s ";
			oss << std::setfill('0') << std::setw(1) << milliseconds << "ms";

			view.duration = oss.str();
		};

		update_duration();
	}
	else
	{
		auto time = std::chrono::clock_cast<std::chrono::system_clock>(data.evtc_file_time);
		std::chrono::zoned_time local_time = { std::chrono::current_zone(), time };

		view.time = std::format("{:%H:%M}", local_time);

		auto it = EncounterNames.find(data.trigger_id);

		if (it != EncounterNames.end())
			view.name = it->second;
		else
			view.name = "Undefined";
	}
}

void LogTableEntry::refresh_time_ago()
{
	auto get_time_ago = [](const std::chrono::system_clock::time_point& timepoint) {
		auto sys_time = std::chrono::clock_cast<std::chrono::system_clock>(timepoint);
		auto sys_time_trunc = std::chrono::floor<std::chrono::seconds>(sys_time);

		std::chrono::zoned_time local_time{ std::chrono::current_zone(), sys_time_trunc };

		std::string formatted_time = std::format("{:%d %B %Y, %H:%M:%S}", local_time);

		std::ostringstream timestamp_stream;

		timestamp_stream << formatted_time;

		auto now = std::chrono::system_clock::now();

		auto diff = now - timepoint;

		std::vector<std::pair<int64_t, std::string>> components = { { std::chrono::duration_cast<std::chrono::years>(diff).count(), "y" }, { std::chrono::duration_cast<std::chrono::months>(diff % std::chrono::years(1)).count(), "M" },
			{ std::chrono::duration_cast<std::chrono::days>(diff % std::chrono::months(1)).count(), "d" }, { std::chrono::duration_cast<std::chrono::hours>(diff % std::chrono::days(1)).count(), "h" }, { std::chrono::duration_cast<std::chrono::minutes>(diff % std::chrono::hours(1)).count(), "m" },
			{ std::chrono::duration_cast<std::chrono::seconds>(diff % std::chrono::minutes(1)).count(), "s" } };

		std::ostringstream ago_stream;
		bool first = true;
		for (const auto& [value, unit] : components)
		{
			if (value > 0)
			{
				ago_stream << value << unit + " ";
				first = false;
			}
		}

		if (first)
			ago_stream << "now";
		else
			ago_stream << "ago";

		return timestamp_stream.str() + " (" + ago_stream.str() + ")";
	};

	view.time_ago = get_time_ago(data.parser_data.status != ParseStatus::PARSED ? data.evtc_file_time : data.parser_data.encounter.end_time);
}
