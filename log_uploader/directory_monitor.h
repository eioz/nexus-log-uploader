#pragma once

#ifndef LOG_UPLOADER_MAC_DEV
#include <Windows.h>
#endif

#include <atomic>
#include <filesystem>
#include <thread>

#include "module.h"

class DirectoryMonitor
{
public:
	void initialize();
	void release();

private:
	std::thread monitor_thread;
#ifndef LOG_UPLOADER_MAC_DEV
	OVERLAPPED monitor_overlapped = { 0 };
#endif
	std::filesystem::path monitor_directory;

	void run();

	std::atomic<bool> initialized = false;
};

DECLARE_MODULE(DirectoryMonitor, directory_monitor)
