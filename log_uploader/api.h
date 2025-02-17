#pragma once

#include <Nexus.h>
#include <Mumble.h>

#include <filesystem>

#define ADDON_DIRECTORY "log-uploader"
#define ADDON_LOG_CHANNEL "Log Uploader"

namespace addon
{
	extern std::filesystem::path directory;
	extern AddonAPI* api;
	extern Mumble::Data* mumble;
	extern NexusLinkData* nexus;
}
