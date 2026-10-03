#pragma once

#include "resource.h"

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
	Berg = 16088,
	Zane = 16137,
	Narella = 16125,
	MatthiasGabrel = 16115,
	// Stronghold of the Faithful
	McLeodTheSilent = 16253,
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
	BrokenKing = 19691,
	EaterOfSouls = 19536,
	EyeOfJudgment = 19651,
	EyeOfFate = 19844,
	Dhuum = 19450,
	// Mythwright Gambit
	ConjuredAmalgamate = 43974,
	Nikare = 21105,
	Kenut = 21089,
	Qadim = 20934,
	// The Key of Ahdashim
	CardinalAdina = 22006,
	CardinalSabir = 21964,
	QadimThePeerless = 22000,
	// Mount Balrior
	GreerTheBlightbringer = 26725,
	DecimaTheStormsinger = 26774,
	GodsquallDecima = 26867,
	UraTheSteamshrieker = 26712,
	// Core
	PrototypeVermilion = 25413,
	PrototypeVermilionCM = 25414,
	PrototypeArsenite = 25415,
	PrototypeIndigo = 25419,
	// The Icebrood Saga
	IcebroodConstruct = 22154,
	VoiceOfTheFallen = 22343,
	ClawOfTheFallen = 22481,
	VoiceAndClaw = 22315,
	FraenirOfJormag = 22492,
	IcebroodConstructSanctum = 22436,
	Boneskinner = 22521,
	WhisperOfJormag = 22711,
	// End of Dragons
	MaiTrin = 24033,
	EchoOfScarletBriar = 24768,
	Ankka = 23957,
	MinisterLi = 24485,
	MinisterLiCM = 24266,
	TheRitualist = 23618,
	TheMindblade = 24254,
	TheEnforcer = 24261,
	TheSniper = 23612,
	TheMechRider = 24660,
	TheDragonvoid = 43488,
	VoidAmalgamate = 23956,
	// Secrets of the Obscure
	Dagda = 25705,
	Cerus = 25989,
	// Visions of Eternity
	KelaSeneschalOfWaves = 27124,

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
	// Silent Surf
	Kanaxai = 25572,
	KanaxaiChallengeMode = 25577,
	// Lonely Tower
	Eparch = 26231,

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

	// Open World
	SooWon = 35552,

	// Uncategorized
	Freezie = 21333,
	DregShark = 21181,
	HeartsAndMinds = 15884,
};

struct EncounterDefinition
{
	std::string name;
	std::vector<TriggerID> triggers;
	unsigned icon = 0; // the resource of the encounter icon
};

struct EncounterInstance
{
	std::string name;
	std::vector<EncounterDefinition> encounters;
};

struct EncounterCategory
{
	std::string name;
	std::vector<EncounterInstance> instances;
};

// clang-format off
inline const std::vector<EncounterCategory> EncounterCategories = {
  { "Raids", {
    { "Spirit Vale", {
      { "Vale Guardian", { TriggerID::ValeGuardian }, IDR_ENCOUNTER_VALE_GUARDIAN },
      { "Spirit Woods", { TriggerID::SpiritRace }, IDR_ENCOUNTER_SPIRIT_WOODS },
      { "Gorseval", { TriggerID::Gorseval }, IDR_ENCOUNTER_GORSEVAL },
      { "Sabetha the Saboteur", { TriggerID::SabethaTheSaboteur }, IDR_ENCOUNTER_SABETHA_THE_SABOTEUR },
    } },
    { "Salvation Pass", {
      { "Slothasor", { TriggerID::Slothasor }, IDR_ENCOUNTER_SLOTHASOR },
      { "Bandit Trio", { TriggerID::Berg, TriggerID::Zane, TriggerID::Narella }, IDR_ENCOUNTER_BANDIT_TRIO },
      { "Matthias Gabrel", { TriggerID::MatthiasGabrel }, IDR_ENCOUNTER_MATTHIAS_GABREL },
    } },
    { "Stronghold of the Faithful", {
      { "Siege the Stronghold", { TriggerID::McLeodTheSilent }, IDR_ENCOUNTER_SIEGE_THE_STRONGHOLD },
      { "Keep Construct", { TriggerID::KeepConstruct }, IDR_ENCOUNTER_KEEP_CONSTRUCT },
      { "Twisted Castle", { TriggerID::TwistedCastle }, IDR_ENCOUNTER_TWISTED_CASTLE },
      { "Xera", { TriggerID::Xera }, IDR_ENCOUNTER_XERA },
    } },
    { "Bastion of the Penitent", {
      { "Cairn the Indomitable", { TriggerID::CairnTheIndomitable }, IDR_ENCOUNTER_CAIRN_THE_INDOMITABLE },
      { "Mursaat Overseer", { TriggerID::MursaatOverseer }, IDR_ENCOUNTER_MURSAAT_OVERSEER },
      { "Samarog", { TriggerID::Samarog }, IDR_ENCOUNTER_SAMAROG },
      { "Deimos", { TriggerID::Deimos }, IDR_ENCOUNTER_DEIMOS },
    } },
    { "Hall of Chains", {
      { "Soulless Horror", { TriggerID::SoullessHorror }, IDR_ENCOUNTER_SOULLESS_HORROR },
      { "River of Souls", { TriggerID::RiverOfSouls }, IDR_ENCOUNTER_RIVER_OF_SOULS },
      { "Broken King", { TriggerID::BrokenKing }, IDR_ENCOUNTER_BROKEN_KING },
      { "Eater of Souls", { TriggerID::EaterOfSouls }, IDR_ENCOUNTER_EATER_OF_SOULS },
      { "Statue of Darkness", { TriggerID::EyeOfFate, TriggerID::EyeOfJudgment }, IDR_ENCOUNTER_STATUE_OF_DARKNESS },
      { "Dhuum", { TriggerID::Dhuum }, IDR_ENCOUNTER_DHUUM },
    } },
    { "Mythwright Gambit", {
      { "Conjured Amalgamate", { TriggerID::ConjuredAmalgamate }, IDR_ENCOUNTER_CONJURED_AMALGAMATE },
      { "Twin Largos", { TriggerID::Nikare, TriggerID::Kenut }, IDR_ENCOUNTER_TWIN_LARGOS },
      { "Qadim", { TriggerID::Qadim }, IDR_ENCOUNTER_QADIM },
    } },
    { "The Key of Ahdashim", {
      { "Cardinal Adina", { TriggerID::CardinalAdina }, IDR_ENCOUNTER_CARDINAL_ADINA },
      { "Cardinal Sabir", { TriggerID::CardinalSabir }, IDR_ENCOUNTER_CARDINAL_SABIR },
      { "Qadim the Peerless", { TriggerID::QadimThePeerless }, IDR_ENCOUNTER_QADIM_THE_PEERLESS },
    } },
    { "Mount Balrior", {
      { "Greer, the Blightbringer", { TriggerID::GreerTheBlightbringer }, IDR_ENCOUNTER_GREER_THE_BLIGHTBRINGER },
      { "Decima, the Stormsinger", { TriggerID::DecimaTheStormsinger, TriggerID::GodsquallDecima }, IDR_ENCOUNTER_DECIMA_THE_STORMSINGER },
      { "Ura, the Steamshrieker", { TriggerID::UraTheSteamshrieker }, IDR_ENCOUNTER_URA_THE_STEAMSHRIEKER },
    } },
  } },
  { "Raids (Strike Missions)", {
    { "Core", {
      { "Old Lion's Court", { TriggerID::PrototypeVermilion, TriggerID::PrototypeVermilionCM, TriggerID::PrototypeArsenite, TriggerID::PrototypeIndigo }, IDR_ENCOUNTER_OLD_LIONS_COURT },
    } },
    { "The Icebrood Saga", {
      { "Shiverpeaks Pass", { TriggerID::IcebroodConstruct }, IDR_ENCOUNTER_SHIVERPEAKS_PASS },
      { "Voice of the Fallen and Claw of the Fallen", { TriggerID::VoiceOfTheFallen, TriggerID::ClawOfTheFallen, TriggerID::VoiceAndClaw }, IDR_ENCOUNTER_VOICE_OF_THE_FALLEN_AND_CLAW_OF_THE_FALLEN },
      { "Fraenir of Jormag", { TriggerID::FraenirOfJormag, TriggerID::IcebroodConstructSanctum }, IDR_ENCOUNTER_FRAENIR_OF_JORMAG },
      { "Boneskinner", { TriggerID::Boneskinner }, IDR_ENCOUNTER_BONESKINNER },
      { "Whisper of Jormag", { TriggerID::WhisperOfJormag }, IDR_ENCOUNTER_WHISPER_OF_JORMAG },
    } },
    { "End of Dragons", {
      { "Aetherblade Hideout", { TriggerID::MaiTrin, TriggerID::EchoOfScarletBriar }, IDR_ENCOUNTER_AETHERBLADE_HIDEOUT },
      { "Xunlai Jade Junkyard", { TriggerID::Ankka }, IDR_ENCOUNTER_XUNLAI_JADE_JUNKYARD },
      { "Kaineng Overlook", { TriggerID::MinisterLi, TriggerID::MinisterLiCM, TriggerID::TheRitualist, TriggerID::TheMindblade, TriggerID::TheEnforcer, TriggerID::TheSniper, TriggerID::TheMechRider }, IDR_ENCOUNTER_KAINENG_OVERLOOK },
      { "Harvest Temple", { TriggerID::TheDragonvoid, TriggerID::VoidAmalgamate }, IDR_ENCOUNTER_HARVEST_TEMPLE },
    } },
    { "Secrets of the Obscure", {
      { "Cosmic Observatory", { TriggerID::Dagda }, IDR_ENCOUNTER_COSMIC_OBSERVATORY },
      { "Temple of Febe", { TriggerID::Cerus }, IDR_ENCOUNTER_TEMPLE_OF_FEBE },
    } },
    { "Visions of Eternity", {
      { "Guardian's Glade", { TriggerID::KelaSeneschalOfWaves }, IDR_ENCOUNTER_GUARDIANS_GLADE },
    } },
  } },
  { "Fractals", {
    { "Nightmare", {
      { "MAMA", { TriggerID::MAMA }, IDR_ENCOUNTER_MAMA },
      { "Siax the Corrupted", { TriggerID::SiaxTheCorrupted }, IDR_ENCOUNTER_SIAX_THE_CORRUPTED },
      { "Ensolyss of the Endless Torment", { TriggerID::EnsolyssOfTheEndlessTorment }, IDR_ENCOUNTER_ENSOLYSS_OF_THE_ENDLESS_TORMENT },
    } },
    { "Shattered Observatory", {
      { "Skorvald the Shattered", { TriggerID::SkorvaldTheShattered }, IDR_ENCOUNTER_SKORVALD_THE_SHATTERED },
      { "Artsariiv", { TriggerID::Artsariiv }, IDR_ENCOUNTER_ARTSARIIV },
      { "Arkk", { TriggerID::Arkk }, IDR_ENCOUNTER_ARKK },
    } },
    { "Sunqua Peak", {
      { "Ai, Keeper of the Peak", { TriggerID::AiKeeperOfThePeak }, IDR_ENCOUNTER_AI_KEEPER_OF_THE_PEAK },
    } },
    { "Silent Surf", {
      { "Kanaxai", { TriggerID::Kanaxai, TriggerID::KanaxaiChallengeMode }, IDR_ENCOUNTER_KANAXAI },
    } },
    { "Lonely Tower", {
      { "Eparch", { TriggerID::Eparch }, IDR_ENCOUNTER_EPARCH },
    } },
  } },
  { "Other", {
    { "Convergences", {
      { "Demon Knight", { TriggerID::DemonKnight }, IDR_ENCOUNTER_CONVERGENCE },
      { "Sorrow", { TriggerID::Sorrow }, IDR_ENCOUNTER_CONVERGENCE },
      { "Dreadwing", { TriggerID::Dreadwing }, IDR_ENCOUNTER_CONVERGENCE },
      { "Hell Sister", { TriggerID::HellSister }, IDR_ENCOUNTER_CONVERGENCE },
      { "Umbriel, Halberd of House Aurkus", { TriggerID::Umbriel }, IDR_ENCOUNTER_CONVERGENCE },
    } },
    { "Special Forces Training Area", {
      { "Standard Kitty Golem", { TriggerID::StandardKittyGolem }, IDR_ENCOUNTER_STANDARD_KITTY_GOLEM },
      { "Medium Kitty Golem", { TriggerID::MediumKittyGolem }, IDR_ENCOUNTER_MEDIUM_KITTY_GOLEM },
      { "Large Kitty Golem", { TriggerID::LargeKittyGolem }, IDR_ENCOUNTER_LARGE_KITTY_GOLEM },
    } },
    { "World vs World", {
      { "World vs. World", { TriggerID::WorldVsWorld }, IDR_ENCOUNTER_WORLD_VS_WORLD },
    } },
    { "Open World", {
      { "Soo-Won", { TriggerID::SooWon }, IDR_ENCOUNTER_SOO_WON },
    } },
    { "Uncategorized", {
      { "Freezie", { TriggerID::Freezie }, IDR_ENCOUNTER_FREEZIE },
      { "Dreg Shark", { TriggerID::DregShark } },
      { "Hearts and Minds", { TriggerID::HeartsAndMinds }, IDR_ENCOUNTER_HEARTS_AND_MINDS },
    } },
  } },
};
// clang-format on

inline const std::map<TriggerID, std::string> EncounterNames = []() {
	std::map<TriggerID, std::string> result;
	for (const auto& category : EncounterCategories)
		for (const auto& instance : category.instances)
			for (const auto& encounter : instance.encounters)
				for (const auto& trigger : encounter.triggers)
					result.emplace(trigger, encounter.name);
	return result;
}();

inline const std::map<TriggerID, unsigned> EncounterIcons = []() {
	std::map<TriggerID, unsigned> result;
	for (const auto& category : EncounterCategories)
		for (const auto& instance : category.instances)
			for (const auto& encounter : instance.encounters)
				for (const auto& trigger : encounter.triggers)
					if (encounter.icon)
						result.emplace(trigger, encounter.icon);
	return result;
}();
