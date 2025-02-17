#include "log.h"
#include "ui.h"

#include <ShlObj.h>

Log::Log(EVTCParserData data)
{
	trigger_id = data.trigger_id;
	evtc_file_path = data.evtc_file_path;
	evtc_file_time = data.evtc_file_time;

	static const auto extract_log_identifier = [](const std::filesystem::path& p) -> std::string
		{
			std::vector<std::filesystem::path> parts(p.begin(), p.end());
			if (auto it = std::find(parts.begin(), parts.end(), "arcdps.cbtlogs"); it != parts.end()) {
				for (auto boss = std::next(it); boss != std::prev(parts.end()); ++boss)
				{
					auto file = std::prev(parts.end());
					if (boss != file) return boss->string() + "/" + file->string();
				}
			}
			return p.string();
		};

	id = extract_log_identifier(evtc_file_path);
}

Log::~Log()
{

}

void DpsReportUpload::open() // move somewhere else?
{
	ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}
