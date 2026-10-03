#include "logs_table.h"
#include "addon.h"
#include "dps_report_uploader.h"
#include "parser.h"
#include "session_manager.h"
#include "ui.h"
#include "ui_elements.h"
#include "wingman_uploader.h"

#undef min
#undef max

namespace
{
std::string get_session_name(const SessionInfo& session)
{
	std::chrono::zoned_time local_time = { std::chrono::current_zone(), std::chrono::floor<std::chrono::minutes>(session.start_time) };

	return std::format("{:%a %d %b %Y, %H:%M} ({})", local_time, session.log_count);
}
} // namespace

void LogsTable::render()
{
	std::lock_guard lock(mutex);

	update_entries();

	auto display_settings = addon::settings->get().display.log_table;

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

		if (display_settings.dps_report_column)
			columns.push_back(Columns::DPS_REPORT);

		if (display_settings.wingman_column)
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

	draw_context_menu();

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

void LogsTable::draw_context_menu()
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

						if (e.data.wingman_upload.status == UploadStatus::AVAILABLE || e.data.wingman_upload.status == UploadStatus::FAILED)
						{
							action_logs[static_cast<size_t>(LogAction::UPLOAD_TO_WINGMAN)].push_back(e);
						}
					}
					else if (e.data.parser_data.status == ParseStatus::UNPARSED || e.data.parser_data.status == ParseStatus::PARSED || e.data.parser_data.status == ParseStatus::FAILED)
					{
						action_logs[static_cast<size_t>(LogAction::PARSE)].push_back(e);
					}

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
