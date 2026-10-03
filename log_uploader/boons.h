#pragma once

#include <vector>

struct BoonDefinition
{
	int id;
	const char* name;
};

inline const std::vector<BoonDefinition> Boons = {
	{ 740, "Might" },
	{ 725, "Fury" },
	{ 1187, "Quickness" },
	{ 30328, "Alacrity" },
	{ 717, "Protection" },
	{ 718, "Regeneration" },
	{ 726, "Vigor" },
	{ 743, "Aegis" },
	{ 1122, "Stability" },
	{ 719, "Swiftness" },
	{ 26980, "Resistance" },
	{ 873, "Resolution" },
};
