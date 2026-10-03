#pragma once

#include "evtc.h"

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <shared_mutex>

// time points are stored as milliseconds since epoch
NLOHMANN_JSON_NAMESPACE_BEGIN
template <>
struct adl_serializer<std::chrono::system_clock::time_point>
{
	static void to_json(json& j, const std::chrono::system_clock::time_point& time) { j = std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()).count(); }

	static void from_json(const json& j, std::chrono::system_clock::time_point& time) { time = std::chrono::system_clock::time_point(std::chrono::milliseconds(j.get<int64_t>())); }
};
NLOHMANN_JSON_NAMESPACE_END

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

	std::string url;
	std::string id;
	std::string user_token;

	void open();

	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(DpsReportUpload, status, url, id, error_message)
};

class WingmanUpload : public Upload
{
public:
	UploadStatus status = UploadStatus::AVAILABLE;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(WingmanUpload, status, error_message)
};

class Encounter
{
public:
	std::string name;
	std::string account_name;
	int duration_ms = 0;
	bool success = false;
	bool has_boss = false;
	float health_percent_burned = 0.f;
	EncounterDifficulty difficulty = EncounterDifficulty::NORMAL_MODE;

	std::chrono::system_clock::time_point start_time{};
	std::chrono::system_clock::time_point end_time{};

	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Encounter, name, account_name, duration_ms, success, has_boss, health_percent_burned, difficulty, start_time, end_time)
};

class PlayerSummary
{
public:
	std::string name;
	std::string account;
	std::string profession;
	int group = 0;
	int boss_dps = 0;
	int cleave_dps = 0;
	int downs = 0;
	int deaths = 0;
	double breakbar_damage = 0.0;
	std::map<int, float> boon_uptimes; // average stacks for might and stability

	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(PlayerSummary, name, account, profession, group, boss_dps, cleave_dps, downs, deaths, breakbar_damage, boon_uptimes)
};

class EVTCParserData
{
public:
	TriggerID trigger_id = TriggerID::Invalid;
	std::filesystem::path evtc_file_path;
	std::chrono::system_clock::time_point evtc_file_time;

	bool is_valid() const { return trigger_id != TriggerID::Invalid && !evtc_file_path.empty(); }
};

class ParserData
{
public:
	std::filesystem::path html_file_path;
	std::filesystem::path json_file_path;

	std::optional<std::string> error_message;

	Encounter encounter = Encounter();
	std::vector<PlayerSummary> players;

	bool is_success() const { return status == ParseStatus::PARSED && encounter.success; }

	// archived sessions keep parsed logs without their reports
	bool has_report() const { return status == ParseStatus::PARSED && !html_file_path.empty(); }

	ParseStatus status = ParseStatus::UNPARSED;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(ParserData, html_file_path, json_file_path, error_message, encounter, players, status)
};

using EncounterLogID = std::string;

class LogData : public EVTCParserData
{
public:
	EncounterLogID id; // unique identifier based on evtc file path arcdps.cbtlogs/{ encounter_id }

	ParserData parser_data = ParserData();
	DpsReportUpload dps_report_upload = DpsReportUpload();
	WingmanUpload wingman_upload = WingmanUpload();

	bool is_busy() const
	{
		auto uploading = [](UploadStatus status) { return status == UploadStatus::QUEUED || status == UploadStatus::UPLOADING; };

		return parser_data.status == ParseStatus::QUEUED || parser_data.status == ParseStatus::PARSING || uploading(dps_report_upload.status) || uploading(wingman_upload.status);
	}

	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(LogData, id, trigger_id, evtc_file_path, evtc_file_time, parser_data, dps_report_upload, wingman_upload)
};

class Log : public LogData
{
public:
	Log(EVTCParserData data);
	Log(LogData data) : LogData(data) {}

	~Log();

	LogData get_data() { return *this; }

	// also marks the log for saving with its session
	void update_view()
	{
		view_updated_required.store(true);
		save_required.store(true);
	}

	std::atomic<bool> view_updated_required = true;
	std::atomic<bool> save_required = false;

	mutable std::shared_mutex mutex;
};
