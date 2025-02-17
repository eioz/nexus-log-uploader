#include "dps_report_uploader.h"
#include "parser.h"
#include "ui_elements.h"
#include "wingman_uploader.h"

#include <imgui_internal.h>

#include <ShlObj.h>

bool ImGui::ButtonDisabled(const char* label, bool disabled)
{
	if (disabled)
	{
		PushStyleVar(ImGuiStyleVar_Alpha, GetStyle().Alpha * 0.65f);
		PushItemFlag(ImGuiItemFlags_Disabled, true);
	}

	SetNextItemWidth(GetColumnWidth());
	auto result = Button(label, ImVec2(GetColumnWidth(), 0.f));

	if (disabled)
	{
		PopItemFlag();
		PopStyleVar();

		return false;
	}

	return result;
}

void ImGui::ButtonParser(std::shared_ptr<Log> log, LogData& log_data)
{
	ID id("Parser Button");

	static const auto get_text = [](ParseStatus parse_status) -> const char*
		{
			switch (parse_status)
			{
			case ParseStatus::UNPARSED: return "Parse";
			case ParseStatus::QUEUED: return "Queued";
			case ParseStatus::PARSING: return "Parsing";
			case ParseStatus::PARSED: return "Open";
			case ParseStatus::FAILED: return "Failed";
			default: return "Unknown";
			}
		};

	auto available = log_data.parser_data.status == ParseStatus::PARSED || log_data.parser_data.status == ParseStatus::UNPARSED;

	if (ButtonDisabled(get_text(log_data.parser_data.status), !available))
	{
		if (log_data.parser_data.status == ParseStatus::UNPARSED)
			addon::parser->add_log(log);
		else if (log_data.parser_data.status == ParseStatus::PARSED)
			ShellExecute(nullptr, L"open", log_data.parser_data.html_file_path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
	}

	if(log_data.parser_data.error_message.has_value())
		HoverTooltip(log_data.parser_data.error_message.value().c_str());
}

bool ImGui::ButtonUpload(UploadStatus upload_status, bool available)
{
	static const auto get_text = [](UploadStatus status) -> const char*
		{
			switch (status)
			{
			case UploadStatus::UNAVAILABLE: return "Unavailable";
			case UploadStatus::AVAILABLE: return "Upload";
			case UploadStatus::QUEUED: return "Queued";
			case UploadStatus::UPLOADING: return "Uploading";
			case UploadStatus::UPLOADED: return "Uploaded";
			case UploadStatus::SKIPPED: return "Skipped";
			case UploadStatus::FAILED: return "Failed";
			default: return "Unknown";
			}
		};

	return ButtonDisabled(get_text(upload_status), !available);
}

void ImGui::ButtonDPSReportUpload(std::shared_ptr<Log> log, DpsReportUpload& upload_data)
{
	ID id("DPS Report Button");

	auto available = upload_data.status == UploadStatus::AVAILABLE || upload_data.status == UploadStatus::UPLOADED;

	static const auto get_text = [](UploadStatus status) -> const char*
		{
			switch (status)
			{
			case UploadStatus::UNAVAILABLE: return "Unavailable";
			case UploadStatus::AVAILABLE: return "Upload";
			case UploadStatus::QUEUED: return "Queued";
			case UploadStatus::UPLOADING: return "Uploading";
			case UploadStatus::UPLOADED: return "Open";
			case UploadStatus::SKIPPED: return "Skipped";
			case UploadStatus::FAILED: return "Failed";
			default: return "Unknown";
			}
		};

	if (ButtonDisabled(get_text(upload_data.status), !available))
	{
		if (upload_data.status == UploadStatus::AVAILABLE)
			addon::dps_report_uploader->add_log(log);
		else if (upload_data.status == UploadStatus::UPLOADED && !upload_data.url.empty())
			upload_data.open();
	}

	if (upload_data.error_message.has_value())
		HoverTooltip(upload_data.error_message.value().c_str());
}

void ImGui::ButtonWingmanUpload(std::shared_ptr<Log> log, WingmanUpload& upload_data, ParserData& parser_data)
{
	ID id("Wingman Button");

	auto available = upload_data.status == UploadStatus::AVAILABLE && parser_data.status == ParseStatus::PARSED;

	if (ButtonUpload(upload_data.status, available))
	{
		if (upload_data.status == UploadStatus::AVAILABLE)
			addon::wingman_uploader->add_log(log);
	}

	if (upload_data.error_message.has_value())
		HoverTooltip(upload_data.error_message.value().c_str());
}

bool ImGui::EncounterSelector(const char* label, EncounterSelection* value)
{
	ID id("Encounter Selector");

	auto r = false;

	static char search_buffer[64] = "";
	static char previous_search_buffer[64] = "";
	static bool expand_on_search_update = false;
	static bool expand_all = false;
	static bool collapse_all = false;

	Text(label);
	Spacing();

	if (InputText("Search", search_buffer, IM_ARRAYSIZE(search_buffer)))
	{
		if (strcmp(search_buffer, previous_search_buffer) != 0)
		{
			expand_on_search_update = true;
			strncpy_s(previous_search_buffer, search_buffer, sizeof(previous_search_buffer));
		}
	}

	std::string search_text_lower = search_buffer;
	std::transform(search_text_lower.begin(), search_text_lower.end(), search_text_lower.begin(), [](unsigned char c)
		{
			return std::tolower(c);
		});
	Spacing();
	if (Button("Select All"))
	{
		for (const auto& [main_category, sub_categories] : EncounterCategories)
		{
			for (const auto& [sub_category, triggers] : sub_categories)
			{
				for (const auto& trigger_id : triggers)
				{
					auto it = EncounterNames.find(trigger_id);
					if (it != EncounterNames.end())
					{
						std::string trigger_name_lower = it->second;
						std::string trigger_id_str = std::to_string(static_cast<int>(trigger_id));
						std::string searchable_text = trigger_name_lower + " " + trigger_id_str + " " + main_category + " " + sub_category;

						std::transform(searchable_text.begin(), searchable_text.end(), searchable_text.begin(), [](unsigned char c)
							{
								return std::tolower(c);
							});

						if (search_text_lower.empty() || searchable_text.find(search_text_lower) != std::string::npos)
							if (std::find(value->begin(), value->end(), trigger_id) == value->end())
								value->push_back(trigger_id);
					}
				}
			}
		}
		r = true;
	}
	SameLine();
	if (Button("Deselect All"))
	{
		for (const auto& [main_category, sub_categories] : EncounterCategories)
		{
			for (const auto& [sub_category, triggers] : sub_categories)
			{
				for (const auto& trigger_id : triggers)
				{
					auto it = EncounterNames.find(trigger_id);
					if (it != EncounterNames.end())
					{
						std::string trigger_name_lower = it->second;
						std::string trigger_id_str = std::to_string(static_cast<int>(trigger_id));
						std::string searchable_text = trigger_name_lower + " " + trigger_id_str + " " + main_category + " " + sub_category;

						std::transform(searchable_text.begin(), searchable_text.end(), searchable_text.begin(), [](unsigned char c)
							{
								return std::tolower(c);
							});

						if (search_text_lower.empty() || searchable_text.find(search_text_lower) != std::string::npos)
							value->erase(std::remove(value->begin(), value->end(), trigger_id), value->end());
					}
				}
			}
		}
		r = true;
	}
	SameLine();
	Spacing();
	SameLine();
	if (Button("Expand All"))
	{
		expand_all = true;
		collapse_all = false;
	}
	SameLine();
	if (Button("Collapse All"))
	{
		collapse_all = true;
		expand_all = false;
	}

	Spacing();

	for (const auto& [main_category, sub_categories] : EncounterCategories)
	{
		bool main_category_matches = false;

		std::vector<std::pair<std::string, std::vector<TriggerID>>> filtered_subcategories;

		for (const auto& [sub_category, triggers] : sub_categories)
		{
			std::vector<TriggerID> filtered_triggers;

			for (const auto& trigger_id : triggers)
			{
				auto it = EncounterNames.find(trigger_id);
				if (it != EncounterNames.end())
				{
					std::string trigger_name_lower = it->second;
					std::string trigger_id_str = std::to_string(static_cast<int>(trigger_id));
					std::string searchable_text = trigger_name_lower + " " + trigger_id_str + " " + main_category + " " + sub_category;

					std::transform(searchable_text.begin(), searchable_text.end(), searchable_text.begin(), [](unsigned char c)
						{
							return std::tolower(c);
						});

					if (search_text_lower.empty() || searchable_text.find(search_text_lower) != std::string::npos)
						filtered_triggers.push_back(trigger_id);
				}
			}

			if (!filtered_triggers.empty())
			{
				filtered_subcategories.emplace_back(sub_category, filtered_triggers);
				main_category_matches = true;
			}
		}

		if (!main_category_matches)
			continue;

		bool main_category_open = expand_all || (expand_on_search_update && main_category_matches);

		if (main_category_open)
			SetNextItemOpen(true, ImGuiCond_Always);
		else if (collapse_all)
			SetNextItemOpen(false, ImGuiCond_Always);

		if (CollapsingHeader(main_category.c_str(), ImGuiTreeNodeFlags_AllowItemOverlap))
		{
			Indent();

			for (const auto& [sub_category, filtered_triggers] : filtered_subcategories)
			{
				bool sub_category_open = expand_all || (expand_on_search_update && main_category_matches);

				if (sub_category_open)
					SetNextItemOpen(true, ImGuiCond_Always);
				else if (collapse_all)
					SetNextItemOpen(false, ImGuiCond_Always);

				if (TreeNode(sub_category.c_str()))
				{
					for (const auto& trigger_id : filtered_triggers)
					{
						auto it = EncounterNames.find(trigger_id);

						if (it == EncounterNames.end())
							continue;

						const std::string& name = it->second;
						std::string checkbox_label = name + " (" + std::to_string(static_cast<int>(trigger_id)) + ")";

						bool selected = std::find(value->begin(), value->end(), trigger_id) != value->end();
						if (Checkbox(checkbox_label.c_str(), &selected))
						{
							if (selected)
							{
								if (std::find(value->begin(), value->end(), trigger_id) == value->end())
									value->push_back(trigger_id);
							}
							else
								value->erase(std::remove(value->begin(), value->end(), trigger_id), value->end());

							r = true;
						}
					}
					TreePop();
				}
			}

			Unindent();
		}
	}

	expand_all = false;
	collapse_all = false;
	expand_on_search_update = false;

	return r;
}

void ImGui::HoverTooltip(const char* text)
{
	if (IsItemHovered())
		SetTooltip(text);
}

void ImGui::CenterNextTextItemHorizontally(const char* text)
{
	const auto offset = (ImGui::GetColumnWidth() - ImGui::CalcTextSize(text).x) * .5f;
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (offset > 0 ? offset : 0));
}
