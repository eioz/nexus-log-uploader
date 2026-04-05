#include "api.h"

namespace addon
{
std::filesystem::path directory = std::filesystem::path();
AddonAPI_t* api = nullptr;
Mumble::Data* mumble = nullptr;
NexusLinkData_t* nexus = nullptr;
} // namespace addon
