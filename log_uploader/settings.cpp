#include "settings.h"
#include "addon.h"

#include <fstream>
#include <nlohmann/json.hpp>

IMPLEMENT_MODULE(Settings, settings)

void Settings::initialize()
{
	std::lock_guard lock(mutex);

	file_path = addon::directory / "settings.json";

	if (!std::filesystem::exists(file_path) || !load())
		save(data);

	running = true;
	save_thread = std::thread(&Settings::run, this);
}

void Settings::release()
{
	{
		std::lock_guard lock(mutex);
		running = false;
	}

	save_cv.notify_all();

	if (save_thread.joinable())
		save_thread.join();

	save(get());
}

void Settings::run()
{
	std::unique_lock lock(mutex);

	while (true)
	{
		save_cv.wait(lock, [this] { return !running || save_required; });

		if (!running)
			break;

		auto settings = data;
		save_required = false;

		// the render thread reads the settings every frame, the file is written without the lock
		lock.unlock();
		save(settings);
		lock.lock();
	}
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
		addon::log("Failed to load settings file: " + addon::to_utf8(file_path) + " Exception: " + e.what(), LOGLEVEL_WARNING);
		return false;
	}
}

bool Settings::save(const SettingsData& settings)
{
	try
	{
		if (!std::filesystem::exists(file_path.parent_path()))
			if (!std::filesystem::create_directories(file_path.parent_path()))
				throw std::runtime_error("Failed to create directory");

		auto temporary_path = file_path;
		temporary_path += ".tmp";

		std::ofstream file(temporary_path);

		if (!file.is_open())
			throw std::runtime_error("Failed to open settings file");

		nlohmann::json json = settings;

		file << json.dump(2);
		file.close();

		if (!file)
			throw std::runtime_error("Failed to write settings file");

		// a crash while saving leaves the previous file intact
		std::filesystem::rename(temporary_path, file_path);

		return true;
	}
	catch (const std::exception& e)
	{
		addon::log("Failed to save settings file: " + addon::to_utf8(file_path) + " Exception: " + e.what(), LOGLEVEL_WARNING);
		return false;
	}
}
