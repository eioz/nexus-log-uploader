#pragma once

#include <filesystem>
#include <Windows.h>
#include <thread>

#include "module.h"

class DirectoryMonitor
{
public:
	void initialize();
	void release();

private:
	std::thread monitor_thread;
	OVERLAPPED monitor_overlapped = { 0 };
	std::filesystem::path monitor_directory;

	void run();

	std::atomic<bool> initialized = false;
};

DECLARE_MODULE(DirectoryMonitor, directory_monitor)
