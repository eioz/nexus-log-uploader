#include "directory_monitor.h"
#include "addon.h"
#include "log_manager.h"

#include <ShlObj.h>
#include <deque>
#include <fstream>
#include <set>

IMPLEMENT_MODULE(DirectoryMonitor, directory_monitor)

namespace
{
bool is_log_file(const std::filesystem::path& path)
{
	auto extension = path.extension();

	return extension == ".evtc" || extension == ".zevtc";
}

void add_log_file(const std::filesystem::path& file_path)
{
	try
	{
		static const auto is_file_openable = [](const std::filesystem::path& log_path) -> bool {
			std::ifstream file_stream(log_path);
			return file_stream.is_open();
		};

		auto log_available = is_file_openable(file_path);

		for (auto cooldown = 1; !log_available && cooldown < 150; ++cooldown)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(cooldown));
			log_available = is_file_openable(file_path);
		}

		if (log_available)
		{
			addon::log("New evtc file detected: " + addon::to_utf8(file_path), LOGLEVEL_DEBUG);
			addon::log_manager->add_log(file_path);
		}
		else
			addon::log("Evtc file unavailable: " + addon::to_utf8(file_path), LOGLEVEL_WARNING);
	}
	catch (const std::exception& e)
	{
		addon::log("Failed to add new evtc file: " + std::string(e.what()), LOGLEVEL_WARNING);
	}
}
} // namespace

void DirectoryMonitor::initialize()
{
	monitor_directory = addon::get_log_directory();

	if (monitor_directory.empty())
	{
		try
		{
			PWSTR path = nullptr;
			if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &path)) && path != nullptr)
			{
				std::unique_ptr<wchar_t, decltype(&::CoTaskMemFree)> guard(path, ::CoTaskMemFree);
				monitor_directory = std::filesystem::path(path) / "Guild Wars 2" / "addons" / "arcdps" / "arcdps.cbtlogs";
			}
			else
				throw std::runtime_error("Failed to get documents path");
		}
		catch (const std::exception& e)
		{
			addon::log("Failed to initialize directory monitor: " + std::string(e.what()), LOGLEVEL_CRITICAL);
			initialized.store(false);
			return;
		}
	}

	stop_event = CreateEvent(nullptr, TRUE, FALSE, nullptr);

	if (stop_event == NULL)
	{
		addon::log("Failed to create directory monitor stop event", LOGLEVEL_CRITICAL);
		return;
	}

	initialized.store(true);

	addon::log("Monitoring directory: " + addon::to_utf8(monitor_directory), LOGLEVEL_INFO);

	this->monitor_thread = std::thread(&DirectoryMonitor::run, this);
}

void DirectoryMonitor::release()
{
	initialized.store(false);

	if (stop_event != NULL)
		SetEvent(stop_event);

	if (monitor_thread.joinable())
		monitor_thread.join();

	if (stop_event != NULL)
	{
		CloseHandle(stop_event);
		stop_event = NULL;
	}

	monitor_overlapped = { 0 };
	monitor_directory.clear();
}

void DirectoryMonitor::run()
{
	// unreachable network paths throw without an error code
	std::error_code ec;

	// arcdps creates the log directory when it saves its first log
	auto waited = false;

	while (!monitor_directory.empty() && !std::filesystem::exists(monitor_directory, ec) && !ec)
	{
		if (!waited)
			addon::log("Waiting for the log directory: " + addon::to_utf8(monitor_directory), LOGLEVEL_INFO);

		waited = true;

		if (WaitForSingleObject(stop_event, 1000) != WAIT_TIMEOUT)
			return;
	}

	if (monitor_directory.empty() || !std::filesystem::exists(monitor_directory, ec))
	{
		addon::log("Directory monitor not started: path does not exist: " + addon::to_utf8(monitor_directory), LOGLEVEL_WARNING);
		return;
	}

	auto directory_handle = CreateFileW(monitor_directory.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);

	if (directory_handle == INVALID_HANDLE_VALUE)
	{
		addon::log("Failed to get directory handle", LOGLEVEL_CRITICAL);
		return;
	}

	std::vector<BYTE> buffer(4096);
	DWORD bytes_returned;

	monitor_overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

	if (monitor_overlapped.hEvent == NULL)
	{
		addon::log("Failed to create event", LOGLEVEL_CRITICAL);
		CloseHandle(directory_handle);
		return;
	}

	addon::log("Started monitoring: " + addon::to_utf8(monitor_directory), LOGLEVEL_DEBUG);

	auto read_pending = false;
	std::set<std::filesystem::path> found_logs;

	while (true)
	{
		auto result = ReadDirectoryChangesW(directory_handle, buffer.data(), static_cast<DWORD>(buffer.size()), TRUE, FILE_NOTIFY_CHANGE_FILE_NAME, &bytes_returned, &monitor_overlapped, nullptr);

		if (!result && GetLastError() != ERROR_IO_PENDING)
		{
			addon::log("Failed to read directory changes", LOGLEVEL_CRITICAL);
			break;
		}

		read_pending = true;

		// logs saved before the directory was watched, the first log creates it
		if (waited)
		{
			waited = false;

			try
			{
				for (const auto& entry : std::filesystem::recursive_directory_iterator(monitor_directory))
					if (is_log_file(entry.path()) && found_logs.insert(entry.path()).second)
						add_log_file(entry.path());
			}
			catch (const std::exception& e)
			{
				addon::log("Failed to read the log directory: " + std::string(e.what()), LOGLEVEL_WARNING);
			}
		}

		HANDLE events[] = { stop_event, monitor_overlapped.hEvent };

		auto wait_result = WaitForMultipleObjects(2, events, FALSE, INFINITE);

		if (wait_result == WAIT_OBJECT_0 + 1)
		{
			read_pending = false;

			DWORD bytes_transferred;

			if (GetOverlappedResult(directory_handle, &monitor_overlapped, &bytes_transferred, TRUE))
			{
				ResetEvent(monitor_overlapped.hEvent);

				// zero bytes means the buffer overflowed and the changes are lost
				if (bytes_transferred == 0)
				{
					addon::log("Directory changes overflowed, new logs may have been missed", LOGLEVEL_WARNING);
					continue;
				}

				auto* fni = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer.data());

				do
				{
					if (fni->Action == FILE_ACTION_RENAMED_NEW_NAME)
					{
						auto file_path = monitor_directory / std::wstring(fni->FileName, fni->FileNameLength / sizeof(wchar_t));

						// logs found right after the directory was created are added already
						if (is_log_file(file_path) && !found_logs.erase(file_path))
							add_log_file(file_path);
					}

					fni = (fni->NextEntryOffset != 0) ? reinterpret_cast<FILE_NOTIFY_INFORMATION*>(reinterpret_cast<BYTE*>(fni) + fni->NextEntryOffset) : nullptr;

				} while (fni != nullptr && this->initialized.load());
			}
		}
		else
		{
			if (wait_result == WAIT_FAILED)
				addon::log("WaitForMultipleObjects failed", LOGLEVEL_CRITICAL);

			break;
		}
	}

	// cancel the pending read before its buffer is released, a read that failed to start never completes
	if (read_pending)
	{
		CancelIoEx(directory_handle, &monitor_overlapped);

		DWORD bytes_transferred;
		GetOverlappedResult(directory_handle, &monitor_overlapped, &bytes_transferred, TRUE);
	}

	if (monitor_overlapped.hEvent != NULL)
	{
		CloseHandle(monitor_overlapped.hEvent);
		monitor_overlapped.hEvent = NULL;
	}

	if (directory_handle != INVALID_HANDLE_VALUE)
		CloseHandle(directory_handle);

	addon::log("Directory monitor stopped", LOGLEVEL_DEBUG);
}
