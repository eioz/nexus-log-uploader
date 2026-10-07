#pragma once

#include "uploader.h"

#include <nlohmann/json.hpp>

#include <map>

#define WINGMAN_URL "https://gw2wingman.nevermindcreations.de"

class WingmanUploader : public Uploader
{
public:
	void add_log(std::shared_ptr<Log> log) override;

	void process_auto_upload(std::shared_ptr<Log> log);

	// requests the missing link and statistics of a parsed log in the background
	void request_statistics(std::shared_ptr<Log> log);

	std::atomic<bool> servers_available = false;

private:
	void run() override;

	WingmanUpload upload(LogData& log_data);

	void update_statistics(std::shared_ptr<Log> log);
	void retry_link(std::shared_ptr<Log> log);

	std::string get_era(std::chrono::system_clock::time_point time);
	std::optional<int> get_boss_id(const LogData& data);

	bool get_server_availability();

	std::queue<std::shared_ptr<Log>> statistics_queue;
	std::multimap<std::chrono::steady_clock::time_point, std::shared_ptr<Log>> link_retries;

	// kept until the addon is unloaded, only used by the upload thread
	nlohmann::json patches;
	nlohmann::json boss_list; // with the target ids wingman counts as each boss
	std::map<std::string, nlohmann::json> bosses;
	std::map<std::string, std::string> ranks;
	std::map<std::shared_ptr<Log>, int> link_failures;
};

DECLARE_MODULE(WingmanUploader, wingman_uploader)
