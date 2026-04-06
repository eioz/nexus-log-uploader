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
      { "Vale Guardian", { TriggerID::ValeGuardian } },
      { "Spirit Woods", { TriggerID::SpiritRace } },
      { "Gorseval", { TriggerID::Gorseval } },
      { "Sabetha the Saboteur", { TriggerID::SabethaTheSaboteur } },
    } },
    { "Salvation Pass", {
      { "Slothasor", { TriggerID::Slothasor } },
      { "Bandit Trio", { TriggerID::Berg, TriggerID::Zane, TriggerID::Narella } },
      { "Matthias Gabrel", { TriggerID::MatthiasGabrel } },
    } },
    { "Stronghold of the Faithful", {
      { "Siege the Stronghold", { TriggerID::McLeodTheSilent } },
      { "Keep Construct", { TriggerID::KeepConstruct } },
      { "Twisted Castle", { TriggerID::TwistedCastle } },
      { "Xera", { TriggerID::Xera } },
    } },
    { "Bastion of the Penitent", {
      { "Cairn the Indomitable", { TriggerID::CairnTheIndomitable } },
      { "Mursaat Overseer", { TriggerID::MursaatOverseer } },
      { "Samarog", { TriggerID::Samarog } },
      { "Deimos", { TriggerID::Deimos } },
    } },
    { "Hall of Chains", {
      { "Soulless Horror", { TriggerID::SoullessHorror } },
      { "River of Souls", { TriggerID::RiverOfSouls } },
      { "Broken King", { TriggerID::BrokenKing } },
      { "Eater of Souls", { TriggerID::EaterOfSouls } },
      { "Statue of Darkness", { TriggerID::EyeOfFate, TriggerID::EyeOfJudgment } },
      { "Dhuum", { TriggerID::Dhuum } },
    } },
    { "Mythwright Gambit", {
      { "Conjured Amalgamate", { TriggerID::ConjuredAmalgamate } },
      { "Twin Largos", { TriggerID::Nikare, TriggerID::Kenut } },
      { "Qadim", { TriggerID::Qadim } },
    } },
    { "The Key of Ahdashim", {
      { "Cardinal Adina", { TriggerID::CardinalAdina } },
      { "Cardinal Sabir", { TriggerID::CardinalSabir } },
      { "Qadim the Peerless", { TriggerID::QadimThePeerless } },
    } },
    { "Mount Balrior", {
      { "Greer, the Blightbringer", { TriggerID::GreerTheBlightbringer } },
      { "Decima, the Stormsinger", { TriggerID::DecimaTheStormsinger, TriggerID::GodsquallDecima } },
      { "Ura, the Steamshrieker", { TriggerID::UraTheSteamshrieker } },
    } },
  } },
  { "Raids (Strike Missions)", {
    { "Core", {
      { "Old Lion's Court", { TriggerID::PrototypeVermilion, TriggerID::PrototypeVermilionCM, TriggerID::PrototypeArsenite, TriggerID::PrototypeIndigo } },
    } },
    { "The Icebrood Saga", {
      { "Shiverpeaks Pass", { TriggerID::IcebroodConstruct } },
      { "Voice of the Fallen and Claw of the Fallen", { TriggerID::VoiceOfTheFallen, TriggerID::ClawOfTheFallen, TriggerID::VoiceAndClaw } },
      { "Fraenir of Jormag", { TriggerID::FraenirOfJormag, TriggerID::IcebroodConstructSanctum } },
      { "Boneskinner", { TriggerID::Boneskinner } },
      { "Whisper of Jormag", { TriggerID::WhisperOfJormag } },
    } },
    { "End of Dragons", {
      { "Aetherblade Hideout", { TriggerID::MaiTrin, TriggerID::EchoOfScarletBriar } },
      { "Xunlai Jade Junkyard", { TriggerID::Ankka } },
      { "Kaineng Overlook", { TriggerID::MinisterLi, TriggerID::MinisterLiCM, TriggerID::TheRitualist, TriggerID::TheMindblade, TriggerID::TheEnforcer, TriggerID::TheSniper, TriggerID::TheMechRider } },
      { "Harvest Temple", { TriggerID::TheDragonvoid, TriggerID::VoidAmalgamate } },
    } },
    { "Secrets of the Obscure", {
      { "Cosmic Observatory", { TriggerID::Dagda } },
      { "Temple of Febe", { TriggerID::Cerus } },
    } },
    { "Visions of Eternity", {
      { "Guardian's Glade", { TriggerID::KelaSeneschalOfWaves } },
    } },
  } },
  { "Fractals", {
    { "Nightmare", {
      { "MAMA", { TriggerID::MAMA } },
      { "Siax the Corrupted", { TriggerID::SiaxTheCorrupted } },
      { "Ensolyss of the Endless Torment", { TriggerID::EnsolyssOfTheEndlessTorment } },
    } },
    { "Shattered Observatory", {
      { "Skorvald the Shattered", { TriggerID::SkorvaldTheShattered } },
      { "Artsariiv", { TriggerID::Artsariiv } },
      { "Arkk", { TriggerID::Arkk } },
    } },
    { "Sunqua Peak", {
      { "Ai, Keeper of the Peak", { TriggerID::AiKeeperOfThePeak } },
    } },
    { "Silent Surf", {
      { "Kanaxai", { TriggerID::Kanaxai, TriggerID::KanaxaiChallengeMode } },
    } },
    { "Lonely Tower", {
      { "Eparch", { TriggerID::Eparch } },
    } },
  } },
  { "Other", {
    { "Convergences", {
      { "Demon Knight", { TriggerID::DemonKnight } },
      { "Sorrow", { TriggerID::Sorrow } },
      { "Dreadwing", { TriggerID::Dreadwing } },
      { "Hell Sister", { TriggerID::HellSister } },
      { "Umbriel, Halberd of House Aurkus", { TriggerID::Umbriel } },
    } },
    { "Special Forces Training Area", {
      { "Standard Kitty Golem", { TriggerID::StandardKittyGolem } },
      { "Medium Kitty Golem", { TriggerID::MediumKittyGolem } },
      { "Large Kitty Golem", { TriggerID::LargeKittyGolem } },
    } },
    { "World vs World", {
      { "World vs. World", { TriggerID::WorldVsWorld } },
    } },
    { "Open World", {
      { "Soo-Won", { TriggerID::SooWon } },
    } },
    { "Uncategorized", {
      { "Freezie", { TriggerID::Freezie } },
      { "Dreg Shark", { TriggerID::DregShark } },
      { "Hearts and Minds", { TriggerID::HeartsAndMinds } },
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
