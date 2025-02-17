#pragma once

#include "evtc.h"

#include <shared_mutex>
#include <filesystem>
#include <chrono>
#include <atomic>

enum class ParseStatus
{
	UNPARSED = 0,
	QUEUED = 1,
	PARSING = 2,
	PARSED = 3,
	FAILED = 4
};

enum class EncounterDifficulty
{
	NORMAL_MODE = 0,
	CHALLENGE_MODE = 1,
	LEGENDARY_CHALLENGE_MODE = 2,
	EMBOLDENED_MODE = 3,
	EMBOLDENED_MODE_2 = 4,
	EMBOLDENED_MODE_3 = 5,
	EMBOLDENED_MODE_4 = 6,
	EMBOLDENED_MODE_5 = 7
};

class Upload
{
public:
	std::optional<std::string> error_message;
};

enum class UploadStatus
{
	UNAVAILABLE,
	AVAILABLE,
	QUEUED,
	UPLOADING,
	UPLOADED,
	SKIPPED,
	FAILED
};

class DpsReportUpload : public Upload
{
public:
	UploadStatus status = UploadStatus::AVAILABLE;

	std::string url = "";
	std::string id;
	std::string user_token;

	void open();
};

class WingmanUpload : public Upload
{
public:
	UploadStatus status = UploadStatus::AVAILABLE;
};

class Encounter
{
public:
	std::string name = "";
	std::string account_name = "";
	int duration_ms = 0;
	bool success = false;
	bool has_boss = false;
	float health_percent_burned = 0.f;
	EncounterDifficulty difficulty = EncounterDifficulty::NORMAL_MODE;

	std::chrono::system_clock::time_point start_time{};
	std::chrono::system_clock::time_point end_time{};
};

class EVTCParserData
{
public:
	TriggerID trigger_id = TriggerID::Invalid;
	std::filesystem::path evtc_file_path;
	std::chrono::system_clock::time_point evtc_file_time;

	auto is_valid() const -> bool
	{
		return trigger_id != TriggerID::Invalid && !evtc_file_path.empty();
	}
};

class ParserData
{
public:
	std::filesystem::path html_file_path;
	std::filesystem::path json_file_path;

	std::optional<std::string> error_message;

	Encounter encounter = Encounter();

	auto is_success() const -> bool
	{
		return status == ParseStatus::PARSED && encounter.success;
	}

	ParseStatus status = ParseStatus::UNPARSED;
};

using EncounterLogID = std::string;

class LogData : public EVTCParserData
{
public:
	EncounterLogID id = ""; //unique identifier based on evtc file path arcdps.cbtlogs/{ encounter_id }

	ParserData parser_data = ParserData();
	DpsReportUpload dps_report_upload = DpsReportUpload();
	WingmanUpload wingman_upload = WingmanUpload();
};

class Log : public LogData
{
public:
	Log(EVTCParserData data);
	Log(LogData data) : LogData(data) {}

	~Log();

	auto get_data() -> LogData
	{
		return *this;
	}

	auto update_view() -> void
	{
		view_updated_required.store(true);
	}

	std::atomic<bool> view_updated_required = true;

	mutable std::shared_mutex mutex;
};
