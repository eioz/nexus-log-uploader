#include "elite_insights.h"
#include "addon.h"
#include "boons.h"
#include "settings.h"
#include "wingman_uploader.h"

#include <cpr/cpr.h>
#include <miniz/miniz.h>

#include <algorithm>
#include <fstream>

#define INSTALLATION_DIRECTORY "elite-insights"
#define EXECUTABLE_FILE "GuildWars2EliteInsights-CLI.exe"
#define SETTINGS_FILE "Settings" / "settings.conf"
#define VERSION_FILE ".version"

#define CPR_PARAMETERS cpr::Timeout(std::chrono::seconds(30)), cpr::ProgressCallback([this](auto...) { return !cancelled.load(); })
#define GITHUB_RELEASES_URL std::string("https://api.github.com/repos/baaron4/GW2-Elite-Insights-Parser/releases/")

namespace {
struct HandleGuard
{
	HANDLE handle = INVALID_HANDLE_VALUE;
	HandleGuard() = default;
	explicit HandleGuard(HANDLE h) : handle(h) {}
	~HandleGuard() { close(); }
	HandleGuard(const HandleGuard&) = delete;
	HandleGuard& operator=(const HandleGuard&) = delete;
	void close()
	{
		if (handle != INVALID_HANDLE_VALUE && handle != nullptr)
		{
			CloseHandle(handle);
			handle = INVALID_HANDLE_VALUE;
		}
	}
};

std::vector<PlayerSummary> get_players(const nlohmann::json& json)
{
	std::vector<PlayerSummary> players;

	if (!json.contains("players") || !json.contains("phases") || json.at("phases").empty())
		return players;

	// boss dps is the dps against the main targets of the full fight phase
	auto boss_targets = json.at("phases").at(0).value("targets", std::vector<size_t>());

	for (const auto& player : json.at("players"))
	{
		if (player.value("notInSquad", false) || player.value("friendlyNPC", false))
			continue;

		PlayerSummary summary;
		summary.name = player.value("name", std::string());
		summary.account = player.value("account", std::string());
		summary.profession = player.value("profession", std::string());
		summary.group = player.value("group", 0);
		summary.cleave_dps = player.at("dpsAll").at(0).value("dps", 0);
		summary.downs = player.at("defenses").at(0).value("downCount", 0);
		summary.deaths = player.at("defenses").at(0).value("deadCount", 0);

		for (auto target : boss_targets)
		{
			const auto& target_dps = player.at("dpsTargets").at(target).at(0);
			summary.boss_dps += target_dps.value("dps", 0);
			summary.breakbar_damage += target_dps.value("breakbarDamage", 0.0);
		}

		if (player.contains("buffUptimes"))
		{
			for (const auto& buff : player.at("buffUptimes"))
			{
				auto id = buff.value("id", 0);

				if (std::any_of(Boons.begin(), Boons.end(), [&](const auto& boon) { return boon.id == id; }))
					summary.boon_uptimes[id] = buff.at("buffData").at(0).value("uptime", 0.f);
			}
		}

		players.push_back(summary);
	}

	return players;
}
} // namespace

ParserData EliteInsights::parse(const std::filesystem::path& evtc_file_path, const std::filesystem::path& output_directory)
{
	ParserData data;

	data.status = ParseStatus::FAILED;

	if (!installed.load())
	{
		throw std::runtime_error("Elite Insights is not installed.");
	}

	if (!std::filesystem::exists(evtc_file_path))
	{
		throw std::runtime_error("EVTC file does not exist: " + addon::to_utf8(evtc_file_path));
	}

	SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
	HANDLE r, w;
	if (!CreatePipe(&r, &w, &sa, 0))
		throw std::runtime_error("Failed to create read/write pipe");
	HandleGuard read_pipe(r), write_pipe(w);

	STARTUPINFOW si = {};
	PROCESS_INFORMATION pi = {};
	si.cb = sizeof(si);
	si.dwFlags |= STARTF_USESTDHANDLES;
	si.hStdOutput = write_pipe.handle;
	si.hStdError = write_pipe.handle;

	if (!std::filesystem::exists(output_directory))
		if (!std::filesystem::create_directories(output_directory))
			throw std::runtime_error("Failed to create Elite Insights output directory: " + addon::to_utf8(output_directory));

	write_settings(output_directory);

	auto command = L"\"" + executable_file.wstring() + L"\" -c \"" + settings_file.wstring() + L"\" \"" + evtc_file_path.wstring() + L"\"";
	std::vector<wchar_t> command_buffer(command.begin(), command.end());
	command_buffer.push_back(L'\0');

	if (!CreateProcessW(NULL, command_buffer.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
		throw std::runtime_error("Failed to start Elite Insights");

	HandleGuard process_handle(pi.hProcess), thread_handle(pi.hThread);
	write_pipe.close();

	std::string output;

	// read while the parser runs, it blocks once the pipe is full
	std::thread output_thread([&]() {
		CHAR buffer[4096];
		DWORD bytes_read;

		while (ReadFile(read_pipe.handle, buffer, sizeof(buffer), &bytes_read, NULL) && bytes_read > 0)
			output.append(buffer, bytes_read);
	});

	auto timeout = std::chrono::steady_clock::now() + std::chrono::minutes(3);
	auto finished = false;

	// wait in short steps so unloading the addon can stop the parser
	while (!finished && !cancelled.load() && std::chrono::steady_clock::now() < timeout)
		finished = WaitForSingleObject(process_handle.handle, 100) != WAIT_TIMEOUT;

	if (!finished)
	{
		TerminateProcess(process_handle.handle, EXIT_FAILURE);
		output_thread.join();

		if (cancelled.load())
			throw std::runtime_error("Parsing cancelled");

		throw std::runtime_error("Elite Insights parser timeout. PID: " + std::to_string(pi.dwProcessId));
	}

	output_thread.join();

	std::smatch matches;

	// the result line has the paths escaped, the other lines use the console code page
	std::regex result_regex(R"(Processed - (.+))");

	if (std::regex_search(output, matches, result_regex) && matches.size() > 1)
	{
		auto result = nlohmann::json::parse(matches[1].str(), nullptr, false);

		if (result.is_object() && result.contains("generatedFiles"))
		{
			for (const auto& file : result.at("generatedFiles"))
			{
				auto path = file.get<std::filesystem::path>();

				if (path.extension() == ".json")
					data.json_file_path = path;
				else if (path.extension() == ".html")
					data.html_file_path = path;
			}
		}
	}
	else
	{
		// versions before 3.21 have no result line
		std::regex json_regex(R"(Generated:\s*(.+\.json)\s*)");

		if (std::regex_search(output, matches, json_regex) && matches.size() > 1)
			data.json_file_path = std::filesystem::path(matches[1].str());

		std::regex html_regex(R"(Generated:\s*(.+\.html)\s*)");

		if (std::regex_search(output, matches, html_regex) && matches.size() > 1)
			data.html_file_path = std::filesystem::path(matches[1].str());
	}

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

			data.encounter.difficulty = lcm ? EncounterDifficulty::LEGENDARY_CHALLENGE_MODE : cm ? EncounterDifficulty::CHALLENGE_MODE : EncounterDifficulty::NORMAL_MODE;

			static const auto parse_time = [](const std::string& utc_time_str) -> std::chrono::system_clock::time_point {
				std::istringstream ss(utc_time_str);
				std::chrono::system_clock::time_point tp;
				// the offset has a colon, for example +05:30
				ss >> std::chrono::parse("%F %T %Ez", tp);
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

						if (id == trigger_id)
						{
							if (target.contains("healthPercentBurned"))
								data.encounter.health_percent_burned = target.at("healthPercentBurned").get<float>();

							data.encounter.has_boss = true;

							break;
						}
					}
				}
			}

			data.players = get_players(json);

			json_file.close();
			data.status = ParseStatus::PARSED;
			return data;
		}
		else
		{
			throw std::runtime_error("Failed to open json file: " + addon::to_utf8(data.json_file_path));
		}
	}
	else
	{
		std::regex failure_regex_message(R"(Parsing Failure - .*?: .*?: (.+))");

		if (std::regex_search(output, matches, failure_regex_message) && matches.size() > 1)
			throw std::runtime_error("Parsing failed: " + matches[1].str());

		throw std::runtime_error("Elite Insights did not generate a report");
	}
}

void EliteInsights::install()
{
	installation_directory = addon::directory / INSTALLATION_DIRECTORY;

	executable_file = installation_directory / EXECUTABLE_FILE;
	settings_file = installation_directory / SETTINGS_FILE;
	version_file = installation_directory / VERSION_FILE;

	if (installation_directory.empty())
		throw std::runtime_error("Installation directory is empty");

	if (!std::filesystem::exists(installation_directory))
		if (!std::filesystem::create_directory(installation_directory))
		{
			throw std::runtime_error("Failed to create installation directory: " + addon::to_utf8(installation_directory));
		}

	auto settings = addon::settings->get().parser;

	auto local_version = get_local_version();
	auto is_installed = [&] { return local_version.is_valid() && std::filesystem::exists(executable_file); };

	auto installed = is_installed();

	if (!settings.auto_update && installed)
	{
		addon::log("Parser auto update disabled.", LOGLEVEL_DEBUG);
		this->installed.store(installed);
		return;
	}

	auto latest_version = get_latest_version(settings.update_channel);

	if (latest_version.is_valid() && !latest_version.download_url.empty())
	{
		if (latest_version > local_version || !installed)
		{
			if (installed)
				addon::log("Updating Elite Insights: " + local_version.get_tag() + " -> " + latest_version.get_tag(), LOGLEVEL_DEBUG);
			else
				addon::log("Installing Elite Insights " + latest_version.get_tag(), LOGLEVEL_DEBUG);

			const auto download = cpr::Get(cpr::Url{ latest_version.download_url }, CPR_PARAMETERS);

			mz_zip_archive zip_archive{};
			mz_zip_zero_struct(&zip_archive);
			auto downloaded = download.status_code == 200 && mz_zip_reader_init_mem(&zip_archive, reinterpret_cast<const unsigned char*>(download.text.c_str()), download.text.size(), 0);

			// a failed update keeps the installed version
			if (!downloaded && installed)
			{
				addon::log("Failed to download Elite Insights " + latest_version.get_tag() + ", keeping " + local_version.get_tag(), LOGLEVEL_WARNING);
				this->installed.store(true);
				return;
			}

			if (download.status_code != 200)
				throw std::runtime_error("Invalid HTTP status code: " + (download.status_code == 0 ? std::string("timeout") : std::to_string(download.status_code)));

			if (!downloaded)
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
					throw std::runtime_error("Failed to extract file from zip archive: " + addon::to_utf8(output_path));
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
					throw std::runtime_error("Failed to write file: " + addon::to_utf8(output_path));
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
				throw std::runtime_error("Failed to write version file: " + addon::to_utf8(version_file));

			local_version = get_local_version();

			if (is_installed())
			{
				addon::log("Installed Elite Insights " + local_version.get_tag(), LOGLEVEL_INFO);
				this->installed.store(true);
				return;
			}
			else
				throw std::runtime_error("Post install validation failed");
		}
		else
		{
			addon::log("Elite Insights is up to date: " + local_version.get_tag(), LOGLEVEL_DEBUG);
			this->installed.store(true);
			return;
		}
	}
	else
	{
		addon::log("Failed to determine latest Elite Insights version", LOGLEVEL_WARNING);

		if (installed)
			this->installed.store(true);
		else
			throw std::runtime_error("Elite Insights is not installed!");
	}
}

void EliteInsights::write_settings(const std::filesystem::path& output_directory)
{
	std::filesystem::create_directories(settings_file.parent_path());

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
OutLocation=)" << std::regex_replace(addon::to_utf8(output_directory), std::regex(R"(\\)"), R"(\\)");

		settings_file_stream << custom_settings.str();
		settings_file_stream.close();
	}
	else
		throw std::runtime_error("Failed to write settings file: " + addon::to_utf8(settings_file));
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

	auto get_github_version = [this](const std::string& url) -> EliteInsightsVersion {
		auto version = EliteInsightsVersion();

		try
		{
			auto version_response = cpr::Get(cpr::Url{ url }, CPR_PARAMETERS);

			if (version_response.status_code != 200)
				throw std::runtime_error("Invalid http status code");

			const auto json_response = nlohmann::json::parse(version_response.text);

			if (!json_response.contains("tag_name") || !json_response.contains("assets") || !json_response["assets"].is_array() || json_response["assets"].empty())
				throw std::runtime_error("Invalid json response");

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
				throw std::runtime_error("Asset download url not found");

			version = EliteInsightsVersion(json_response["tag_name"].get<std::string>(), download_url);
		}
		catch (const std::exception& e)
		{
			addon::log("Failed to parse release information: " + std::string(e.what()), LOGLEVEL_WARNING);
		}

		return version;
	};

	if (update_channel == ParserUpdateChannel::LATEST_WINGMAN)
	{
		const auto version_response = cpr::Get(cpr::Url{ WINGMAN_URL "/api/EIversion" }, CPR_PARAMETERS);

		if (version_response.status_code != 200)
		{
			addon::log("Failed to fetch version tag from " WINGMAN_URL "/api/EIversion. (" + (version_response.status_code ? std::to_string(version_response.status_code) : "timeout") + ")", LOGLEVEL_WARNING);
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
