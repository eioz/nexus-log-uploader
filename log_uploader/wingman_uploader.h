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

	// requests the missing statistics of a parsed log in the background
	void request_statistics(std::shared_ptr<Log> log);

	std::atomic<bool> servers_available = false;

private:
	void run() override;

	WingmanUpload upload(LogData& log_data);

	void update_statistics(std::shared_ptr<Log> log);

	std::string get_era(std::chrono::system_clock::time_point time);

	bool get_server_availability();

	std::queue<std::shared_ptr<Log>> statistics_queue;

	// kept until the addon is unloaded, only used by the upload thread
	nlohmann::json patches;
	std::map<std::string, nlohmann::json> bosses;
	std::map<std::string, std::string> ranks;
};

DECLARE_MODULE(WingmanUploader, wingman_uploader)
