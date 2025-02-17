#include "settings.h"
#include "logger.h"
#include "api.h"
#include "logger.h"

#include <fstream>
#include <nlohmann/json.hpp>

IMPLEMENT_MODULE(Settings, settings)

void Settings::initialize()
{
	std::lock_guard lock(mutex);

	file_path = addon::directory / "settings.json";

	if (!std::filesystem::exists(file_path) || !load())
		save();
}

bool Settings::load()
{
	try
	{
		if (!std::filesystem::exists(file_path))
			throw std::exception("Settings file does not exist");

		std::ifstream file(file_path);

		if (!file.is_open())
			throw std::exception("Failed to open settings file");

		nlohmann::json json;
		file >> json;
		file.close();

		nlohmann::json target_json = data;

		target_json.merge_patch(json);

		auto settings = target_json.get<SettingsData>();

		settings.verify();

		data = std::move(settings);

		return true;
	}
	catch (const std::exception& e)
	{
		LOG("Failed to load settings file: " + file_path.string() + " Exception: " + e.what(), ELogLevel::ELogLevel_WARNING);
		return false;
	}

	return false;
}

bool Settings::save()
{
	try
	{
		if (!std::filesystem::exists(file_path.parent_path()))
			if (!std::filesystem::create_directories(file_path.parent_path()))
				throw std::exception("Failed to create directory");

		std::ofstream file(file_path);

		if (!file.is_open())
			throw std::exception("Failed to open settings file");

		nlohmann::json json = data;

		file << json.dump(2);
		file.close();

		return true;
	}
	catch (const std::exception& e)
	{
		LOG("Failed to save settings file: " + file_path.string() + " Exception: " + e.what(), ELogLevel::ELogLevel_WARNING);
		return false;
	}

	return false;
}

void SettingsData::verify()
{
	// todo ?
}
