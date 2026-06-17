#pragma once

#include <filesystem>
#include <cstring>
#include <memory>
#include <string>

#ifndef LOG_UPLOADER_MAC_DEV
#include <ShlObj.h>
#include <shellapi.h>
#include <windows.h>
#endif

namespace addon::platform
{
#ifdef LOG_UPLOADER_MAC_DEV
void open_url(const std::string& url);
void open_file(const std::filesystem::path& path);
bool copy_text_to_clipboard(const std::string& text);
std::filesystem::path get_default_log_directory();
std::filesystem::path read_ini_path_value(const std::filesystem::path& file_path, const std::string& section, const std::string& key);
#else
inline void open_url(const std::string& url)
{
	ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

inline void open_file(const std::filesystem::path& path)
{
	ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

inline bool copy_text_to_clipboard(const std::string& text)
{
	if (!OpenClipboard(nullptr))
		return false;

	EmptyClipboard();
	HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);

	if (handle == nullptr)
	{
		CloseClipboard();
		return false;
	}

	if (void* locked = GlobalLock(handle))
	{
		memcpy(locked, text.c_str(), text.size() + 1);
		GlobalUnlock(handle);
		SetClipboardData(CF_TEXT, handle);
		CloseClipboard();
		return true;
	}

	GlobalFree(handle);
	CloseClipboard();
	return false;
}

inline std::filesystem::path get_default_log_directory()
{
	PWSTR path = nullptr;
	if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &path)) && path != nullptr)
	{
		std::unique_ptr<wchar_t, decltype(&::CoTaskMemFree)> guard(path, ::CoTaskMemFree);
		return std::filesystem::path(path) / "Guild Wars 2" / "addons" / "arcdps" / "arcdps.cbtlogs";
	}

	return {};
}

inline std::filesystem::path read_ini_path_value(const std::filesystem::path& file_path, const std::string& section, const std::string& key)
{
	wchar_t buffer[MAX_PATH] = {};
	GetPrivateProfileStringW(std::filesystem::path(section).c_str(), std::filesystem::path(key).c_str(), L"", buffer, MAX_PATH, file_path.c_str());
	return std::filesystem::path(buffer);
}
#endif
} // namespace addon::platform
