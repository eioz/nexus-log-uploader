#pragma once

#include "log.h"
#include "module.h"

class LogManager
{
public:
	void add_log(std::filesystem::path evtc_file_path);
};

DECLARE_MODULE(LogManager, log_manager)
