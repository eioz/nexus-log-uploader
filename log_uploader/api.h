#pragma once

#include <Mumble.h>
#include <Nexus.h>

#include <filesystem>

#define ADDON_DIRECTORY "log-uploader"
#define ADDON_LOG_CHANNEL "Log Uploader"

namespace addon
{
extern std::filesystem::path directory;
extern AddonAPI_t* api;
extern Mumble::Data* mumble;
extern NexusLinkData_t* nexus;
} // namespace addon
