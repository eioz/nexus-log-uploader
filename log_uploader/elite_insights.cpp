#include "api.h"
#include "elite_insights.h"
#include "settings.h"
#include "logger.h"

#include <cpr/cpr.h>
#include <miniz/miniz.h>

#include <fstream>

#define INSTALLATION_DIRECTORY "elite-insights"
#define OUTPUT_DIRECTORY "log-data"
#define EXECUTABLE_FILE "GuildWars2EliteInsights-CLI.exe"
#define SETTINGS_FILE "Settings" / "settings.conf"
#define VERSION_FILE ".version"

#define CPR_PARAMETERS cpr::Timeout{ std::chrono::seconds(30) }
#define GITHUB_RELEASES_URL std::string("https://api.github.com/repos/baaron4/GW2-Elite-Insights-Parser/releases/")
#define WINGMAN_VERSION_URL std::string("https://gw2wingman.nevermindcreations.de/api/EIversion")

ParserData EliteInsights::parse(const std::filesystem::path& evtc_file_path)
{
	ParserData data;

	data.status = ParseStatus::FAILED;

	if (!installed.load())
	{
		throw std::runtime_error("Elite Insights is not installed.");
	}

	if (!std::filesystem::exists(evtc_file_path))
	{
		throw std::runtime_error("EVTC file does not exist: " + evtc_file_path.string());
	}

	SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
	HANDLE read_pipe, write_pipe;

	if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0))
	{
		throw std::runtime_error("Failed to create read/write pipe");
	}

	STARTUPINFOA si = {};
	PROCESS_INFORMATION pi = {};
	si.cb = sizeof(si);
	si.dwFlags |= STARTF_USESTDHANDLES;
	si.hStdOutput = write_pipe;
	si.hStdError = write_pipe;

	if (!std::filesystem::exists(output_directory))
	{
		if (!std::filesystem::create_directories(output_directory))
		{
			CloseHandle(read_pipe);
			CloseHandle(write_pipe);
			throw std::runtime_error("Failed to create Elite Insights output directory: " + output_directory.string());
		}
	}

	auto command = "\"" + executable_file.string() + "\" -c \"" + settings_file.string() + "\" \"" + evtc_file_path.string() + "\"";

	if (!CreateProcessA(NULL, const_cast<char*>(command.c_str()), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
	{
		CloseHandle(read_pipe);
		CloseHandle(write_pipe);
		throw std::runtime_error("Failed to start Elite Insights");
	}

	CloseHandle(write_pipe);

	int PARSE_TIMEOUT = 180000;

	if (WaitForSingleObject(pi.hProcess, PARSE_TIMEOUT) == WAIT_TIMEOUT)
	{
		TerminateProcess(pi.hProcess, EXIT_FAILURE);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		CloseHandle(read_pipe);
		throw std::runtime_error("Elite Insights parser timeout. PID: " + std::to_string(pi.dwProcessId));
	}

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	CHAR buffer[4096];
	DWORD read;
	std::string output;

	while (ReadFile(read_pipe, buffer, sizeof(buffer) - 1, &read, NULL))
	{
		buffer[read] = '\0';
		output += buffer;
	}

	CloseHandle(read_pipe);

	std::smatch matches;

	std::regex json_regex(R"(Generated:\s*(.+\.json)\s*)");

	if (std::regex_search(output, matches, json_regex) && matches.size() > 1)
		data.json_file_path = std::filesystem::path(matches[1].str());

	std::regex html_regex(R"(Generated:\s*(.+\.html)\s*)");

	if (std::regex_search(output, matches, html_regex) && matches.size() > 1)
		data.html_file_path = std::filesystem::path(matches[1].str());

	std::regex success_regex(R"(Parsing Successful)");
	std::regex failure_regex(R"(Parsing Failure)");

	const auto valid_output = std::regex_search(output, success_regex) && !std::regex_search(output, failure_regex);

	if (valid_output && std::filesystem::exists(data.json_file_path) && std::filesystem::exists(data.html_file_path))
	{
		std::ifstream json_file(data.json_file_path);

		if (json_file.is_open())
		{
			nlohmann::json json = nlohmann::json::parse(json_file);

			auto trigger_id = 0;

			if (json.contains("triggerID"))
				trigger_id = json.at("triggerID").get<int>();

			if (json.contains("fightName"))
				data.encounter.name = json.at("fightName").get<std::string>();

			if (json.contains("recordedAccountBy"))
				data.encounter.account_name = json.at("recordedAccountBy").get<std::string>();

			if (json.contains("durationMS"))
				data.encounter.duration_ms = json.at("durationMS").get<int>();

			if (json.contains("success"))
				data.encounter.success = json.at("success").get<bool>();

			bool cm = false;

			if (json.contains("isCM"))
				cm = json.at("isCM").get<bool>();

			bool lcm = false;

			if (json.contains("isLegendaryCM"))
				lcm = json.at("isLegendaryCM").get<bool>();

			data.encounter.difficulty = cm ? EncounterDifficulty::CHALLENGE_MODE : lcm ? EncounterDifficulty::LEGENDARY_CHALLENGE_MODE : EncounterDifficulty::NORMAL_MODE;

			static const auto parse_time = [](const std::string& utc_time_str) -> std::chrono::system_clock::time_point
				{
					std::istringstream ss(utc_time_str);
					std::chrono::system_clock::time_point tp;
					ss >> std::chrono::parse("%F %T %z", tp);
					return tp;
				};

			if (json.contains("timeStartStd"))
				data.encounter.start_time = parse_time(json.at("timeStartStd").get<std::string>());

			if (json.contains("timeEndStd"))
				data.encounter.end_time = parse_time(json.at("timeEndStd").get<std::string>());

			if (trigger_id && json.contains("targets") && !json.at("targets").empty())
			{
				for (auto i = 0; i < json.at("targets").size(); i++)
				{
					auto& target = json.at("targets").at(i);

					if (target.contains("id"))
					{
						auto id = target.at("id").get<int>();

						if (id == trigger_id) // main boss
						{
							if (target.contains("healthPercentBurned"))
								data.encounter.health_percent_burned = target.at("healthPercentBurned").get<float>();

							data.encounter.has_boss = true;

							break;
						}
					}
				}
			}

			json_file.close();
			data.status = ParseStatus::PARSED;
			return data;
		}
		else
		{
			throw std::runtime_error("Failed to open json file: " + data.json_file_path.string());
		}
	}
	else
	{
		std::regex failure_regex_message(R"(Parsing Failure - .*?: .*?: (.+))");

		if (std::regex_search(output, matches, failure_regex_message) && matches.size() > 1)
			throw std::runtime_error("Parsing failed: " + matches[1].str());

		return data;
	}
}


void EliteInsights::install()
{
	installation_directory = addon::directory / INSTALLATION_DIRECTORY;
	output_directory = addon::directory / OUTPUT_DIRECTORY;

	executable_file = installation_directory / EXECUTABLE_FILE;
	settings_file = installation_directory / SETTINGS_FILE;
	version_file = installation_directory / VERSION_FILE;

	if (installation_directory.empty() || output_directory.empty())
		throw std::runtime_error("Installation or output directory is empty");

	if (!std::filesystem::exists(installation_directory))
		if (!std::filesystem::create_directory(installation_directory))
		{
			throw std::runtime_error("Failed to create installation directory: " + installation_directory.string());
		}

	auto settings = GET_SETTING(parser);

	auto local_version = get_local_version();
	auto is_installed = [&] { return local_version.is_valid() && std::filesystem::exists(executable_file) && std::filesystem::exists(settings_file); };

	auto installed = is_installed();

	if (!settings.auto_update && installed)
	{
		LOG("Parser auto update disabled.", ELogLevel::ELogLevel_DEBUG);
		this->installed.store(installed);
		return;
	}

	auto latest_version = get_latest_version(settings.update_channel);

	if (latest_version.is_valid() && !latest_version.download_url.empty())
	{
		if (latest_version > local_version || !installed)
		{
			if (installed)
				LOG("Updating Elite Insights: " + local_version.get_tag() + " -> " + latest_version.get_tag(), ELogLevel::ELogLevel_DEBUG);
			else
				LOG("Installing Elite Insights " + latest_version.get_tag(), ELogLevel::ELogLevel_DEBUG);

			const auto download = cpr::Get(cpr::Url{ latest_version.download_url }, CPR_PARAMETERS);

			if (download.status_code != 200)
				throw std::runtime_error("Invalid HTTP status code: " + download.status_code == 0 ? "timeout" : std::to_string(download.status_code));

			mz_zip_archive zip_archive{};
			mz_zip_zero_struct(&zip_archive);
			if (!mz_zip_reader_init_mem(&zip_archive, reinterpret_cast<const unsigned char*>(download.text.c_str()), download.text.size(), 0))
				throw std::runtime_error("Failed to initialize zip archive");

			if (std::filesystem::exists(installation_directory))
				std::filesystem::remove_all(installation_directory);

			for (auto i = 0; i < static_cast<int>(mz_zip_reader_get_num_files(&zip_archive)); ++i)
			{
				mz_zip_archive_file_stat file_stat{};
				if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat))
				{
					mz_zip_reader_end(&zip_archive);
					throw std::runtime_error("Failed to get file stat from zip archive: " + std::to_string(i));
				}

				if (file_stat.m_is_directory)
					continue;

				const auto output_path = installation_directory / file_stat.m_filename;

				if (output_path.has_parent_path())
					std::filesystem::create_directories(output_path.parent_path());

				size_t uncompressed_size = 0;
				auto uncompressed_data = mz_zip_reader_extract_to_heap(&zip_archive, i, &uncompressed_size, 0);

				if (!uncompressed_data)
				{
					mz_zip_reader_end(&zip_archive);
					throw std::runtime_error("Failed to extract file from zip archive: " + output_path.string());
				}

				std::ofstream out_file(output_path, std::ios::binary);

				if (out_file)
				{
					out_file.write(reinterpret_cast<const char*>(uncompressed_data), uncompressed_size);
					out_file.close();
				}
				else
				{
					mz_free(uncompressed_data);
					mz_zip_reader_end(&zip_archive);
					throw std::runtime_error("Failed to write file: " + output_path.string());
				}
				mz_free(uncompressed_data);
			}

			mz_zip_reader_end(&zip_archive);

			std::ofstream version_file_stream(version_file, std::ios::out);

			if (version_file_stream)
			{
				version_file_stream << latest_version.tag_name;
				version_file_stream.close();
			}
			else
				throw std::runtime_error("Failed to write version file: " + version_file.string());

			std::ofstream settings_file_stream(settings_file, std::ios::out);

			if (settings_file_stream)
			{
				std::stringstream custom_settings;
				custom_settings << R"(# Custom settings
SaveOutJSON=true
IndentJSON=false
SaveOutHTML=true
SaveOutTrace=false
SaveAtOut=false
ParseCombatReplay=true
SingleThreaded=false
OutLocation=)" << std::regex_replace(output_directory.string(), std::regex(R"(\\)"), R"(\\)");

				settings_file_stream << custom_settings.str();
				settings_file_stream.close();
			}
			else
				throw std::runtime_error("Failed to write settings file: " + settings_file.string());

			local_version = get_local_version();

			if (is_installed())
			{
				LOG("Installed Elite Insights " + local_version.get_tag(), ELogLevel::ELogLevel_INFO);
				this->installed.store(true);
				return;
			}
			else
				throw std::runtime_error("Post install validation failed");
		}
		else
		{
			LOG("Elite Insights is up to date: " + local_version.get_tag(), ELogLevel::ELogLevel_DEBUG);
			this->installed.store(true);
			return;
		}
	}
	else
	{
		throw std::runtime_error("Failed to get latest version");
	}
}


EliteInsightsVersion EliteInsights::get_local_version()
{
	auto version = EliteInsightsVersion();

	if (!std::filesystem::exists(version_file))
		return version;

	std::ifstream version_file_stream(version_file);

	if (version_file_stream)
	{
		std::string version_str;
		version_file_stream >> version_str;
		version_file_stream.close();

		return EliteInsightsVersion(version_str);
	}

	return version;
}

EliteInsightsVersion EliteInsights::get_latest_version(ParserUpdateChannel update_channel)
{
	EliteInsightsVersion version;

	auto get_github_version = [this](const std::string& url) -> EliteInsightsVersion
		{
			auto version = EliteInsightsVersion();

			try
			{
				auto version_response = cpr::Get(cpr::Url{ url }, CPR_PARAMETERS);

				if (version_response.status_code != 200)
					throw std::exception("Invalid http status code");

				const auto json_response = nlohmann::json::parse(version_response.text);

				if (!json_response.contains("tag_name") || !json_response.contains("assets") || !json_response["assets"].is_array() || json_response["assets"].empty())
					throw std::exception("Invalid json response");

				std::string download_url;

				for (const auto& asset : json_response["assets"])
				{
					if (asset.contains("name") && asset["name"].is_string() && asset["name"] == "GW2EICLI.zip" && asset.contains("browser_download_url") && asset["browser_download_url"].is_string())
					{
						download_url = asset["browser_download_url"].get<std::string>();
						break;
					}
				}
				if (download_url.empty())
					throw std::exception("Asset download url not found");

				version = EliteInsightsVersion(json_response["tag_name"].get<std::string>(), download_url);
			}
			catch (const std::exception& e)
			{
				LOG("Failed to parse release information: " + std::string(e.what()), ELogLevel_WARNING);
			}

			return version;
		};

	if (update_channel == ParserUpdateChannel::LATEST_WINGMAN)
	{
		const auto version_response = cpr::Get(cpr::Url{ WINGMAN_VERSION_URL }, CPR_PARAMETERS);

		if (version_response.status_code != 200)
		{
			LOG("Failed to fetch version tag from " + WINGMAN_VERSION_URL + ". (" + (version_response.status_code ? std::to_string(version_response.status_code) : "timeout") + ")", ELogLevel_WARNING);
			return version;
		}

		version = EliteInsightsVersion(version_response.text);
		version = get_github_version(GITHUB_RELEASES_URL + "tags/" + version.get_tag());
	}
	else
	{
		version = get_github_version(GITHUB_RELEASES_URL + "latest");
	}

	return version;
}
