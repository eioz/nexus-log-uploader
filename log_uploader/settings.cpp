#include "settings.h"
#include "platform.h"

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
			throw std::runtime_error("Settings file does not exist");

		std::ifstream file(file_path);

		if (!file.is_open())
			throw std::runtime_error("Failed to open settings file");

		nlohmann::json json;
		file >> json;
		file.close();

		nlohmann::json target_json = data;

		target_json.merge_patch(json);

		data = target_json.get<SettingsData>();

		return true;
	}
	catch (const std::exception& e)
	{
		addon::log("Failed to load settings file: " + file_path.string() + " Exception: " + e.what(), LOGLEVEL_WARNING);
		return false;
	}
}

bool Settings::save()
{
	try
	{
		if (!std::filesystem::exists(file_path.parent_path()))
			if (!std::filesystem::create_directories(file_path.parent_path()))
				throw std::runtime_error("Failed to create directory");

		std::ofstream file(file_path);

		if (!file.is_open())
			throw std::runtime_error("Failed to open settings file");

		nlohmann::json json = data;

		file << json.dump(2);
		file.close();

		return true;
	}
	catch (const std::exception& e)
	{
		addon::log("Failed to save settings file: " + file_path.string() + " Exception: " + e.what(), LOGLEVEL_WARNING);
		return false;
	}
}
