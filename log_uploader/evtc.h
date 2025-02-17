#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

enum class TriggerID : uint16_t
{
	Invalid = 0,

	// World vs World
	WorldVsWorld = 1,

	// Raids
	// Spirit Vale
	ValeGuardian = 15438,
	Gorseval = 15429,
	SpiritRace = 15415,
	SabethaTheSaboteur = 15375,
	// Salvation Pass
	Slothasor = 16123,
	BanditTrio = 16088,
	MatthiasGabrel = 16115,
	// Stronghold of the Faithful
	SiegeTheStronghold = 16253,
	KeepConstruct = 16235,
	TwistedCastle = 16247,
	Xera = 16246,
	// Bastion of the Penitent
	CairnTheIndomitable = 17194,
	MursaatOverseer = 17172,
	Samarog = 17188,
	Deimos = 17154,
	// Hall of Chains
	SoullessHorror = 19767,
	RiverOfSouls = 19828,
	StatueOfIce = 19691, // broken king
	StatueOfDarkness = 19844, // eyes
	StatueOfDeath = 19536, // eater of souls
	Dhuum = 19450,
	// Mythwright Gambit
	ConjuredAmalgamate = 43974,
	TwinLargos = 21105,
	Qadim = 20934,
	// The Key of Ahdashim
	CardinalAdina = 22006,
	CardinalSabir = 21964,
	QadimThePeerless = 22000,
	// Mount Balrior
	DecimaTheStormsinger = 26774,
	GreerTheBlightbringer = 26725,
	UraTheSteamshrieker = 26712,

	// Fractals
	// Nightmare
	MAMA = 17021,
	SiaxTheCorrupted = 17028,
	EnsolyssOfTheEndlessTorment = 16948,
	// Shattered Observatory
	SkorvaldTheShattered = 17632,
	Artsariiv = 17949,
	Arkk = 17759,
	// Sunqua Peak
	AiKeeperOfThePeak = 23254,
	//Silent Surf
	Kanaxai = 25572,
	KanaxaiChallengeMode = 25577,
	// Lonely Tower
	Eparch = 26231,

	// Strike Missions
	// Core
	OldLionsCourt = 25413,
	OldLionsCourtChallengeMode = 25414,
	// The Icebrood Saga
	IcebroodConstruct = 22154,
	SuperKodanBrothers = 22343,
	FraenirOfJormag = 22492,
	Boneskinner = 22521,
	WhisperOfJormag = 22711,
	// End of Dragons
	AetherbladeHideout = 24033,
	XunlaiJadeJunkyard = 23957,
	KainengOverlook = 24485,
	KainengOverlookChallengeMode = 24266,
	HarvestTemple = 43488,
	// Secrets of the Obscure
	CosmicObservatory = 25705,
	TempleOfFebe = 25989,

	// Other
	// Convergences
	DemonKnight = 26142,
	Sorrow = 26143,
	Dreadwing = 26161,
	HellSister = 26146,
	Umbriel = 26196,

	// Special Forces Training Area
	StandardKittyGolem = 16199,
	MediumKittyGolem = 19645,
	LargeKittyGolem = 19676,

	//Open World
	SooWon = 35552,

	// Uncategorized
	Freezie = 21333,
	DregShark = 21181,
	HeartsAndMinds = 15884,
};

inline const std::map<TriggerID, std::string> EncounterNames =
{
	// World vs World
	{TriggerID::WorldVsWorld, "World vs. World"},

	// Raids
	// Spirit Vale
	{TriggerID::ValeGuardian, "Vale Guardian"},
	{TriggerID::Gorseval, "Gorseval"},
	{TriggerID::SpiritRace, "Spirit Race"},
	{TriggerID::SabethaTheSaboteur, "Sabetha the Saboteur"},
	// Salvation Pass
	{TriggerID::Slothasor, "Slothasor"},
	{TriggerID::BanditTrio, "Bandit Trio"},
	{TriggerID::MatthiasGabrel, "Matthias Gabrel"},
	// Stronghold of the Faithful
	{TriggerID::SiegeTheStronghold, "Siege the Stronghold"},
	{TriggerID::KeepConstruct, "Keep Construct"},
	{TriggerID::TwistedCastle, "Twisted Castle"},
	{TriggerID::Xera, "Xera"},
	// Bastion of the Penitent
	{TriggerID::CairnTheIndomitable, "Cairn the Indomitable"},
	{TriggerID::MursaatOverseer, "Mursaat Overseer"},
	{TriggerID::Samarog, "Samarog"},
	{TriggerID::Deimos, "Deimos"},
	// Hall of Chains
	{TriggerID::SoullessHorror, "Soulless Horror"},
	{TriggerID::RiverOfSouls, "River of Souls"},
	{TriggerID::StatueOfIce, "Statue of Ice"},
	{TriggerID::StatueOfDarkness, "Statue of Darkness"},
	{TriggerID::StatueOfDeath, "Statue of Death"},
	{TriggerID::Dhuum, "Dhuum"},
	// Mythwright Gambit
	{TriggerID::ConjuredAmalgamate, "Conjured Amalgamate"},
	{TriggerID::TwinLargos, "Twin Largos"},
	{TriggerID::Qadim, "Qadim"},
	// The Key of Ahdashim
	{TriggerID::CardinalAdina, "Cardinal Adina"},
	{TriggerID::CardinalSabir, "Cardinal Sabir"},
	{TriggerID::QadimThePeerless, "Qadim the Peerless"},
	// Mount Balrior
	{TriggerID::DecimaTheStormsinger, "Decima, the Stormsinger"},
	{TriggerID::GreerTheBlightbringer, "Greer, the Blightbringer"},
	{TriggerID::UraTheSteamshrieker, "Ura, the Steamshrieker"},

	// Fractals
	// Nightmare
	{TriggerID::MAMA, "MAMA"},
	{TriggerID::SiaxTheCorrupted, "Siax the Corrupted"},
	{TriggerID::EnsolyssOfTheEndlessTorment, "Ensolyss of the Endless Torment"},
	// Shattered Observatory
	{TriggerID::SkorvaldTheShattered, "Skorvald the Shattered"},
	{TriggerID::Artsariiv, "Artsariiv"},
	{TriggerID::Arkk, "Arkk"},
	// Sunqua Peak
	{TriggerID::AiKeeperOfThePeak, "Ai, Keeper of the Peak"},
	// Silent Surf
	{TriggerID::Kanaxai, "Kanaxai"},
	{TriggerID::KanaxaiChallengeMode, "Kanaxai CM"},
	// Lonely Tower
	{TriggerID::Eparch, "Eparch"},

	// Strike Missions
	// Core
	{TriggerID::OldLionsCourt, "Old Lion's Court"},
	{TriggerID::OldLionsCourtChallengeMode, "Old Lion's Court CM"},
	// The Icebrood Saga
	{TriggerID::IcebroodConstruct, "Icebrood Construct"},
	{TriggerID::SuperKodanBrothers, "Super Kodan Brothers"},
	{TriggerID::FraenirOfJormag, "Fraenir of Jormag"},
	{TriggerID::Boneskinner, "Boneskinner"},
	{TriggerID::WhisperOfJormag, "Whisper of Jormag"},
	// End of Dragons
	{TriggerID::AetherbladeHideout, "Aetherblade Hideout"},
	{TriggerID::XunlaiJadeJunkyard, "Xunlai Jade Junkyard"},
	{TriggerID::KainengOverlook, "Kaineng Overlook"},
	{TriggerID::KainengOverlookChallengeMode, "Kaineng Overlook CM"},
	{TriggerID::HarvestTemple, "Harvest Temple"},
	// Secrets of the Obscure
	{TriggerID::CosmicObservatory, "Cosmic Observatory"},
	{TriggerID::TempleOfFebe, "Temple of Febe"},

	//Other
	// Convergences
	{TriggerID::DemonKnight, "Demon Knight"},
	{TriggerID::Sorrow, "Sorrow"},
	{TriggerID::Dreadwing, "Dreadwing"},
	{TriggerID::HellSister, "Hell Sister"},
	{TriggerID::Umbriel, "Umbriel, Halberd of House Aurkus"},

	// Special Forces Training Area
	{TriggerID::StandardKittyGolem, "Standard Kitty Golem"},
	{TriggerID::MediumKittyGolem, "Medium Kitty Golem"},
	{TriggerID::LargeKittyGolem, "Large Kitty Golem"},

	// Open World
	{TriggerID::SooWon, "Soo-Won"},

	// Uncategorized
	{TriggerID::Freezie, "Freezie"},
	{TriggerID::DregShark, "Dreg Shark"},
	{TriggerID::HeartsAndMinds, "Hearts and Minds"},
};

inline const std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::vector<TriggerID>>>>> EncounterCategories = 
{
	{ "Raids", {
		{"Spirit Vale", {TriggerID::ValeGuardian, TriggerID::SpiritRace, TriggerID::Gorseval, TriggerID::SabethaTheSaboteur}},
		{"Salvation Pass", {TriggerID::Slothasor, TriggerID::BanditTrio, TriggerID::MatthiasGabrel}},
		{"Stronghold of the Faithful", {TriggerID::SiegeTheStronghold, TriggerID::KeepConstruct, TriggerID::TwistedCastle, TriggerID::Xera}},
		{"Bastion of the Penitent", {TriggerID::CairnTheIndomitable, TriggerID::MursaatOverseer, TriggerID::Samarog, TriggerID::Deimos}},
		{"Hall of Chains", {TriggerID::SoullessHorror, TriggerID::RiverOfSouls, TriggerID::StatueOfIce, TriggerID::StatueOfDarkness, TriggerID::StatueOfDeath, TriggerID::Dhuum}},
		{"Mythwright Gambit", {TriggerID::ConjuredAmalgamate, TriggerID::TwinLargos, TriggerID::Qadim}},
		{"The Key of Ahdashim", {TriggerID::CardinalAdina, TriggerID::CardinalSabir, TriggerID::QadimThePeerless}},
		{"Mount Balrior", {TriggerID::DecimaTheStormsinger, TriggerID::GreerTheBlightbringer, TriggerID::UraTheSteamshrieker}},
	}},
	{"Fractals", {
		{"Nightmare", {TriggerID::MAMA, TriggerID::SiaxTheCorrupted, TriggerID::EnsolyssOfTheEndlessTorment}},
		{"Shattered Observatory", {TriggerID::SkorvaldTheShattered, TriggerID::Artsariiv, TriggerID::Arkk}},
		{"Sunqua Peak", {TriggerID::AiKeeperOfThePeak}},
		{"Silent Surf", {TriggerID::Kanaxai, TriggerID::KanaxaiChallengeMode}},
		{"Lonely Tower", {TriggerID::Eparch}},
	}},
	{"Strike Missions", {
		{"Core", {TriggerID::OldLionsCourt, TriggerID::OldLionsCourtChallengeMode}},
		{"The Icebrood Saga", {TriggerID::IcebroodConstruct, TriggerID::SuperKodanBrothers, TriggerID::FraenirOfJormag, TriggerID::Boneskinner, TriggerID::WhisperOfJormag}},
		{"End of Dragons", {TriggerID::AetherbladeHideout, TriggerID::XunlaiJadeJunkyard, TriggerID::KainengOverlook, TriggerID::KainengOverlookChallengeMode, TriggerID::HarvestTemple}},
		{"Secrets of the Obscure", {TriggerID::CosmicObservatory, TriggerID::TempleOfFebe}},
	}},
	{"Other", {
		{"Convergences", {TriggerID::DemonKnight, TriggerID::Sorrow, TriggerID::Dreadwing, TriggerID::HellSister, TriggerID::Umbriel}},
		{"Special Forces Training Area", {TriggerID::StandardKittyGolem, TriggerID::MediumKittyGolem, TriggerID::LargeKittyGolem}},
		{"World vs World", {TriggerID::WorldVsWorld}},
		{"Uncategorized", {TriggerID::Freezie, TriggerID::DregShark, TriggerID::HeartsAndMinds}},
	}},
};
