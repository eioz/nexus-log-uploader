#pragma once

#include "log.h"
#include "settings.h"

#include <filesystem>
#include <regex>
#include <sstream>
#include <string>
#include <thread>

class EliteInsightsVersion
{
private:
	int v1 = 0, v2 = 0, v3 = 0, v4 = 0;
	bool valid = false;
public:
	std::string download_url;
	std::string tag_name;

	EliteInsightsVersion() : v1(0), v2(0), v3(0), v4(0), valid(false) {}
	EliteInsightsVersion(std::string tag_name, const std::string& download_url = "")
	{
		this->tag_name = tag_name;
		this->download_url = download_url;

		tag_name = tag_name.starts_with("v.") ? "v" + tag_name.substr(2) : tag_name; // fix for inconsistent tag names

		auto pos = tag_name.find("v");

		if (pos != std::string::npos)
		{
			std::string version_str = tag_name.substr(pos + 1);
			std::replace(version_str.begin(), version_str.end(), '.', ' ');
			std::istringstream iss(version_str);
			iss >> v1 >> v2 >> v3 >> v4;

			this->valid = !iss.fail() && std::regex_match(tag_name, std::regex("^v(\\d+)\\.(\\d+)\\.(\\d+)\\.(\\d+)$"));
		}
	}

	bool is_valid() const
	{
		return this->valid;
	}

	const std::string get_tag() const { return this->tag_name; }

	bool operator==(const EliteInsightsVersion& other) const
	{
		return v1 == other.v1 && v2 == other.v2 && v3 == other.v3 && v4 == other.v4;
	}

	bool operator!=(const EliteInsightsVersion& other) const
	{
		return !(*this == other);
	}

	bool operator<(const EliteInsightsVersion& other) const
	{
		return std::tie(v1, v2, v3, v4) < std::tie(other.v1, other.v2, other.v3, other.v4);
	}

	bool operator>(const EliteInsightsVersion& other) const
	{
		return other < *this;
	}

	bool operator<=(const EliteInsightsVersion& other) const
	{
		return !(other < *this);
	}

	bool operator>=(const EliteInsightsVersion& other) const
	{
		return !(*this < other);
	}
};

class EliteInsights
{
public:
	EliteInsights() = default;

	ParserData parse(const std::filesystem::path& evtc_file_path);
	void install();

private:
	std::filesystem::path installation_directory;
	std::filesystem::path output_directory;
	std::filesystem::path executable_file;
	std::filesystem::path settings_file;
	std::filesystem::path version_file;

	std::atomic<bool> installed = false;

	EliteInsightsVersion get_local_version();
	EliteInsightsVersion get_latest_version(ParserUpdateChannel update_channel);
};
