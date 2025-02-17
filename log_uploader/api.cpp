#include "api.h"

namespace addon 
{
	std::filesystem::path directory = std::filesystem::path();
	AddonAPI* api = nullptr;
	Mumble::Data* mumble = nullptr;
	NexusLinkData* nexus = nullptr;
}
