#pragma once

#include "uploader.h"

class DPSReportUploader : public Uploader
{
public:
	void add_log(std::shared_ptr<Log> log) override;
	void process_auto_upload(std::shared_ptr<Log> log);

private:
	void run() override;

	DpsReportUpload upload(std::filesystem::path evtc_file_path);

};

DECLARE_MODULE(DPSReportUploader, dps_report_uploader)