#pragma once

#include "log.h"
#include "module.h"

#include <chrono>
#include <filesystem>
#include <mutex>

class EVTCParser
{
public:
	EVTCParserData parse(const std::filesystem::path& evtc_file_path);

private:
	std::mutex parser_mutex;
};

DECLARE_MODULE(EVTCParser, evtc_parser)