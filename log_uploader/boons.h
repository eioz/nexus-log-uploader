#pragma once

#include "resource.h"

#include <vector>

struct BoonDefinition
{
	int id;
	const char* name;
	unsigned icon;
	bool stacks; // uptime is the average number of stacks
};

inline const std::vector<BoonDefinition> Boons = {
	{ 740, "Might", IDR_BOON_MIGHT, true },
	{ 725, "Fury", IDR_BOON_FURY, false },
	{ 1187, "Quickness", IDR_BOON_QUICKNESS, false },
	{ 30328, "Alacrity", IDR_BOON_ALACRITY, false },
	{ 717, "Protection", IDR_BOON_PROTECTION, false },
	{ 718, "Regeneration", IDR_BOON_REGENERATION, false },
	{ 726, "Vigor", IDR_BOON_VIGOR, false },
	{ 743, "Aegis", IDR_BOON_AEGIS, false },
	{ 1122, "Stability", IDR_BOON_STABILITY, true },
	{ 719, "Swiftness", IDR_BOON_SWIFTNESS, false },
	{ 26980, "Resistance", IDR_BOON_RESISTANCE, false },
	{ 873, "Resolution", IDR_BOON_RESOLUTION, false },
};
