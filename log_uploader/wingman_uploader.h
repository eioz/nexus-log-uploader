#pragma once

#include "uploader.h"

class WingmanUploader : public Uploader
{
public:
	void add_log(std::shared_ptr<Log> log) override;

	void process_auto_upload(std::shared_ptr<Log> log);

	std::atomic<bool> servers_available = false;
private:
	void run() override;

	WingmanUpload upload(LogData &log_data);

	bool get_server_availability();
};

DECLARE_MODULE(WingmanUploader, wingman_uploader)
