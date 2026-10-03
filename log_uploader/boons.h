#pragma once

#include "resource.h"

#include <vector>

struct BoonDefinition
{
	int id;
	const char* name;
	unsigned icon;
};

inline const std::vector<BoonDefinition> Boons = {
	{ 740, "Might", IDR_BOON_MIGHT },
	{ 725, "Fury", IDR_BOON_FURY },
	{ 1187, "Quickness", IDR_BOON_QUICKNESS },
	{ 30328, "Alacrity", IDR_BOON_ALACRITY },
	{ 717, "Protection", IDR_BOON_PROTECTION },
	{ 718, "Regeneration", IDR_BOON_REGENERATION },
	{ 726, "Vigor", IDR_BOON_VIGOR },
	{ 743, "Aegis", IDR_BOON_AEGIS },
	{ 1122, "Stability", IDR_BOON_STABILITY },
	{ 719, "Swiftness", IDR_BOON_SWIFTNESS },
	{ 26980, "Resistance", IDR_BOON_RESISTANCE },
	{ 873, "Resolution", IDR_BOON_RESOLUTION },
};
