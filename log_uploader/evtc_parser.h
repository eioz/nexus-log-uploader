#pragma once

#include "module.h"
#include "log.h"

#include <filesystem>
#include <chrono>
#include <mutex>

class EVTCParser
{
public:
	EVTCParserData parse(const std::filesystem::path& evtc_file_path);
private:
	std::mutex parser_mutex;
};

DECLARE_MODULE(EVTCParser, evtc_parser)