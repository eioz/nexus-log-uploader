#include "directory_monitor.h"
#include "log_manager.h"
#include "logger.h"
#include "platform.h"

#include <ShlObj.h>
#include <deque>
#include <fstream>

IMPLEMENT_MODULE(DirectoryMonitor, directory_monitor)

void DirectoryMonitor::initialize()
{
	monitor_directory = addon::get_log_directory();

	if (monitor_directory.empty())
	{
		try
		{
			PWSTR path = nullptr;
			if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, NULL, &path)) && path != nullptr)
			{
				std::unique_ptr<wchar_t, decltype(&::CoTaskMemFree)> guard(path, ::CoTaskMemFree);
				const auto size = WideCharToMultiByte(CP_UTF8, 0, path, -1, NULL, 0, NULL, NULL);
				if (size > 0)
				{
					std::string string(size, 0);
					WideCharToMultiByte(CP_UTF8, 0, path, -1, &string[0], size, NULL, NULL);
					string.resize(static_cast<std::basic_string<char, std::char_traits<char>, std::allocator<char>>::size_type>(size) - 1);
					if (!string.empty())
					{
						const auto documents_path = std::filesystem::path(string);
						monitor_directory = documents_path / "Guild Wars 2" / "addons" / "arcdps" / "arcdps.cbtlogs";
					}
					else
						throw std::exception("Failed to get documents path: Unable to convert path");
				}
				else
					throw std::exception("Failed to get documents path: Path is empty");
			}
			else
				throw std::exception("Failed to get documents path: SHGetKnownFolderPath failed");
		}
		catch (std::exception& e)
		{
			LOG("Failed to initialize directory monitor:" + std::string(e.what()), LOGLEVEL_CRITICAL);
			initialized.store(false);
			return;
		}
	}

	initialized.store(true);

	LOG("Monitoring directory: " + monitor_directory.string(), LOGLEVEL_INFO);

	this->monitor_thread = std::thread(&DirectoryMonitor::run, this);
}

void DirectoryMonitor::release()
{
	initialized.store(false);

	if (monitor_overlapped.hEvent != NULL)
		SetEvent(monitor_overlapped.hEvent);

	if (monitor_thread.joinable())
		monitor_thread.join();

	monitor_overlapped = { 0 };
	monitor_directory.clear();
}

void DirectoryMonitor::run()
{
	if (monitor_directory.empty() || !std::filesystem::exists(monitor_directory))
	{
		LOG("Directory monitor not started: path does not exist: " + monitor_directory.string(), LOGLEVEL_WARNING);
		return;
	}

	auto directory_handle = CreateFileW(monitor_directory.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);

	if (directory_handle == INVALID_HANDLE_VALUE)
	{
		LOG("Failed to get directory handle", LOGLEVEL_CRITICAL);
		return;
	}

	std::vector<BYTE> buffer(4096);
	DWORD bytes_returned;

	monitor_overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

	if (monitor_overlapped.hEvent == NULL)
	{
		LOG("Failed to create event", LOGLEVEL_CRITICAL);
		CloseHandle(directory_handle);
		return;
	}

	LOG("Started monitoring: " + monitor_directory.string(), LOGLEVEL_DEBUG);

	while (true)
	{
		auto result = ReadDirectoryChangesW(directory_handle, buffer.data(), static_cast<DWORD>(buffer.size()), TRUE, FILE_NOTIFY_CHANGE_FILE_NAME, &bytes_returned, &monitor_overlapped, nullptr);

		if (!result && GetLastError() != ERROR_IO_PENDING)
		{
			LOG("Failed to read directory changes", LOGLEVEL_CRITICAL);
			break;
		}

		auto wait_result = WaitForSingleObject(monitor_overlapped.hEvent, INFINITE);

		if (!this->initialized.load() || monitor_overlapped.hEvent == NULL)
			break;

		if (wait_result == WAIT_OBJECT_0)
		{
			DWORD bytes_transferred;

			if (GetOverlappedResult(directory_handle, &monitor_overlapped, &bytes_transferred, TRUE))
			{
				ResetEvent(monitor_overlapped.hEvent);

				auto* fni = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer.data());

				do
				{
					if (fni->Action == FILE_ACTION_RENAMED_NEW_NAME)
					{
						auto file_name = std::filesystem::path(std::wstring(fni->FileName, fni->FileNameLength / sizeof(wchar_t)));

						auto extension = file_name.extension().string();

						if (extension == ".evtc" || extension == ".zevtc")
						{
							{
								static const auto is_file_openable = [](const std::filesystem::path& log_path) -> bool {
									std::ifstream file_stream(log_path);
									return file_stream.is_open();
								};

								auto file_path = monitor_directory / file_name;

								auto log_available = is_file_openable(file_path);

								for (auto cooldown = 1; !log_available && cooldown < 150; ++cooldown)
								{
									std::this_thread::sleep_for(std::chrono::milliseconds(cooldown));
									log_available = is_file_openable(file_path);
								}

								if (log_available)
								{
									LOG("New evtc file detected: " + file_path.string(), LOGLEVEL_DEBUG);

									addon::log_manager->add_log(file_path);
								}
								else
									LOG("Evtc file unavailable: " + file_path.string(), LOGLEVEL_WARNING);
							}
						}
					}

					fni = (fni->NextEntryOffset != 0) ? reinterpret_cast<FILE_NOTIFY_INFORMATION*>(reinterpret_cast<BYTE*>(fni) + fni->NextEntryOffset) : nullptr;

				} while (fni != nullptr && this->initialized.load());
			}
		}
		else if (wait_result == WAIT_FAILED)
		{
			LOG("WaitForSingleObject failed", LOGLEVEL_CRITICAL);
			break;
		}
	}

	if (monitor_overlapped.hEvent != NULL)
	{
		CloseHandle(monitor_overlapped.hEvent);
		monitor_overlapped.hEvent = NULL;
	}

	if (directory_handle != INVALID_HANDLE_VALUE)
		CloseHandle(directory_handle);

	LOG("Directory monitor stopped", LOGLEVEL_DEBUG);
}
