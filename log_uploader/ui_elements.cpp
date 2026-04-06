#include "ui_elements.h"
#include "dps_report_uploader.h"
#include "parser.h"
#include "wingman_uploader.h"

#include <imgui_internal.h>

#include <algorithm>
#include <unordered_map>

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

	static const auto get_text = [](ParseStatus parse_status) -> const char* {
		switch (parse_status)
		{
		case ParseStatus::UNPARSED:
			return "Parse";
		case ParseStatus::QUEUED:
			return "Queued";
		case ParseStatus::PARSING:
			return "Parsing";
		case ParseStatus::PARSED:
			return "Open";
		case ParseStatus::FAILED:
			return "Failed";
		default:
			return "Unknown";
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

	if (log_data.parser_data.error_message.has_value())
		HoverTooltip(log_data.parser_data.error_message.value().c_str());
}

bool ImGui::ButtonUpload(UploadStatus upload_status, bool available)
{
	static const auto get_text = [](UploadStatus status) -> const char* {
		switch (status)
		{
		case UploadStatus::UNAVAILABLE:
			return "Unavailable";
		case UploadStatus::AVAILABLE:
			return "Upload";
		case UploadStatus::QUEUED:
			return "Queued";
		case UploadStatus::UPLOADING:
			return "Uploading";
		case UploadStatus::UPLOADED:
			return "Uploaded";
		case UploadStatus::SKIPPED:
			return "Skipped";
		case UploadStatus::FAILED:
			return "Failed";
		default:
			return "Unknown";
		}
	};

	return ButtonDisabled(get_text(upload_status), !available);
}

void ImGui::ButtonDPSReportUpload(std::shared_ptr<Log> log, DpsReportUpload& upload_data)
{
	ID id("DPS Report Button");

	auto available = upload_data.status == UploadStatus::AVAILABLE || upload_data.status == UploadStatus::UPLOADED;

	static const auto get_text = [](UploadStatus status) -> const char* {
		switch (status)
		{
		case UploadStatus::UNAVAILABLE:
			return "Unavailable";
		case UploadStatus::AVAILABLE:
			return "Upload";
		case UploadStatus::QUEUED:
			return "Queued";
		case UploadStatus::UPLOADING:
			return "Uploading";
		case UploadStatus::UPLOADED:
			return "Open";
		case UploadStatus::SKIPPED:
			return "Skipped";
		case UploadStatus::FAILED:
			return "Failed";
		default:
			return "Unknown";
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

	struct SelectorState
	{
		char search_buffer[64] = {};
		char previous_search_buffer[64] = {};
		bool expand_on_search_update = false;
		bool expand_all = false;
		bool collapse_all = false;
	};

	static std::unordered_map<ImGuiID, SelectorState> selector_states;
	auto state_id = GetID(label);
	auto& state = selector_states[state_id];

	Text(label);
	Spacing();

	if (InputText("Search", state.search_buffer, IM_ARRAYSIZE(state.search_buffer)))
	{
		if (strcmp(state.search_buffer, state.previous_search_buffer) != 0)
		{
			state.expand_on_search_update = true;
			strncpy_s(state.previous_search_buffer, state.search_buffer, sizeof(state.previous_search_buffer));
		}
	}

	std::string search_text_lower = state.search_buffer;
	std::transform(search_text_lower.begin(), search_text_lower.end(), search_text_lower.begin(), [](unsigned char c) { return std::tolower(c); });
	Spacing();
	if (Button("Select All"))
	{
		for (const auto& category : EncounterCategories)
		{
			for (const auto& instance : category.instances)
			{
				for (const auto& encounter : instance.encounters)
				{
					std::string searchable_text = encounter.name + " " + category.name + " " + instance.name;
					for (const auto& trigger : encounter.triggers)
						searchable_text += " " + std::to_string(static_cast<int>(trigger));
					std::transform(searchable_text.begin(), searchable_text.end(), searchable_text.begin(), [](unsigned char c) { return std::tolower(c); });

					if (search_text_lower.empty() || searchable_text.find(search_text_lower) != std::string::npos)
						for (const auto& trigger : encounter.triggers)
							if (std::find(value->begin(), value->end(), trigger) == value->end())
								value->push_back(trigger);
				}
			}
		}
		r = true;
	}
	SameLine();
	if (Button("Deselect All"))
	{
		for (const auto& category : EncounterCategories)
		{
			for (const auto& instance : category.instances)
			{
				for (const auto& encounter : instance.encounters)
				{
					std::string searchable_text = encounter.name + " " + category.name + " " + instance.name;
					for (const auto& trigger : encounter.triggers)
						searchable_text += " " + std::to_string(static_cast<int>(trigger));
					std::transform(searchable_text.begin(), searchable_text.end(), searchable_text.begin(), [](unsigned char c) { return std::tolower(c); });

					if (search_text_lower.empty() || searchable_text.find(search_text_lower) != std::string::npos)
						for (const auto& trigger : encounter.triggers)
							value->erase(std::remove(value->begin(), value->end(), trigger), value->end());
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
		state.expand_all = true;
		state.collapse_all = false;
	}
	SameLine();
	if (Button("Collapse All"))
	{
		state.collapse_all = true;
		state.expand_all = false;
	}

	Spacing();

	for (const auto& category : EncounterCategories)
	{
		bool main_category_matches = false;

		std::vector<std::pair<std::string, std::vector<const EncounterDefinition*>>> filtered_instances;

		for (const auto& instance : category.instances)
		{
			std::vector<const EncounterDefinition*> filtered_encounters;

			for (const auto& encounter : instance.encounters)
			{
				std::string searchable_text = encounter.name + " " + category.name + " " + instance.name;
				for (const auto& trigger : encounter.triggers)
					searchable_text += " " + std::to_string(static_cast<int>(trigger));
				std::transform(searchable_text.begin(), searchable_text.end(), searchable_text.begin(), [](unsigned char c) { return std::tolower(c); });

				if (search_text_lower.empty() || searchable_text.find(search_text_lower) != std::string::npos)
					filtered_encounters.push_back(&encounter);
			}

			if (!filtered_encounters.empty())
			{
				filtered_instances.emplace_back(instance.name, filtered_encounters);
				main_category_matches = true;
			}
		}

		if (!main_category_matches)
			continue;

		bool main_category_open = state.expand_all || (state.expand_on_search_update && main_category_matches);

		if (main_category_open)
			SetNextItemOpen(true, ImGuiCond_Always);
		else if (state.collapse_all)
			SetNextItemOpen(false, ImGuiCond_Always);

		if (CollapsingHeader(category.name.c_str(), ImGuiTreeNodeFlags_AllowItemOverlap))
		{
			Indent();

			for (const auto& [instance_name, filtered_encounters] : filtered_instances)
			{
				bool instance_open = state.expand_all || (state.expand_on_search_update && main_category_matches);

				if (instance_open)
					SetNextItemOpen(true, ImGuiCond_Always);
				else if (state.collapse_all)
					SetNextItemOpen(false, ImGuiCond_Always);

				if (TreeNode(instance_name.c_str()))
				{
					for (const auto* encounter : filtered_encounters)
					{
						std::string checkbox_label = encounter->name + " (" + std::to_string(static_cast<int>(encounter->triggers.front())) + ")";

						bool selected = std::all_of(encounter->triggers.begin(), encounter->triggers.end(),
						    [&](const auto& t) { return std::find(value->begin(), value->end(), t) != value->end(); });

						if (Checkbox(checkbox_label.c_str(), &selected))
						{
							for (const auto trigger : encounter->triggers)
							{
								if (selected)
								{
									if (std::find(value->begin(), value->end(), trigger) == value->end())
										value->push_back(trigger);
								}
								else
									value->erase(std::remove(value->begin(), value->end(), trigger), value->end());
							}

							r = true;
						}
					}
					TreePop();
				}
			}

			Unindent();
		}
	}

	state.expand_all = false;
	state.collapse_all = false;
	state.expand_on_search_update = false;

	return r;
}

void ImGui::HoverTooltip(const char* text)
{
	if (IsItemHovered())
		SetTooltip("%s", text);
}

void ImGui::CenterNextTextItemHorizontally(const char* text)
{
	const auto offset = (ImGui::GetColumnWidth() - ImGui::CalcTextSize(text).x) * .5f;
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (offset > 0 ? offset : 0));
}
