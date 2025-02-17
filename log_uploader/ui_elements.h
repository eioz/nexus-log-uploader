#pragma once

#include <imgui.h>

#include <string>

#include "log.h"

namespace ImGui
{
	class ID
	{
	public:
		ID(const char* id) { ImGui::PushID(id); }
		ID(std::string id) { ImGui::PushID(id.c_str()); }
		ID(void* id) { ImGui::PushID(id); }
		~ID() { ImGui::PopID(); }
	};

	bool ButtonDisabled(const char* label, bool disabled);

	void ButtonParser(std::shared_ptr<Log> log, LogData& log_data);
	bool ButtonUpload(UploadStatus upload_status, bool available);
	void ButtonDPSReportUpload(std::shared_ptr<Log> log, DpsReportUpload& upload_data);
	void ButtonWingmanUpload(std::shared_ptr<Log> log, WingmanUpload& upload_data, ParserData& parser_data);
	bool EncounterSelector(const char* label, EncounterSelection* value);
	void HoverTooltip(const char* text);
	void CenterNextTextItemHorizontally(const char* text);
}

inline bool operator!=(const ImVec2& lhs, const ImVec2& rhs)
{
	return lhs.x != rhs.x || lhs.y != rhs.y;
}

inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs)
{
	return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y);
}