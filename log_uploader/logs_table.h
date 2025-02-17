#pragma once

#include "log.h"

#include <imgui.h>

#include <deque>
#include <unordered_map>
#include <string>
#include <mutex>
#include <atomic>

struct LogView
{
	bool selected = false;

	std::string name = "";
	std::string time = "";
	std::string result = "";
	std::string duration = "";
	std::string time_ago = "";
};

class LogTableEntry
{
public:
	std::shared_ptr<Log> ptr;
	LogData data;
	LogView view;

	LogTableEntry(std::shared_ptr<Log> ptr)
	{
		this->ptr = ptr;
	}

	void update()
	{
		auto view_update_required = false;

		{
			std::shared_lock lock(ptr->mutex);
			if(view_update_required = ptr->view_updated_required.exchange(false))
				this->data = ptr->get_data();
		}

		if (view_update_required)
			update_view();
	}

	void update_view();
	void refresh_time_ago();
};

class LogsTable
{
public:
	void render();

	void add_log(std::shared_ptr<Log> log_ptr) 
	{
		std::lock_guard<std::mutex> lock(mutex);
		entries.push_front(log_ptr);
	}

	void remove_log(std::shared_ptr<Log> log_ptr) 
	{
		std::lock_guard<std::mutex> lock(mutex);
		entries.erase(std::remove_if(entries.begin(), entries.end(), [&](const LogTableEntry& entry) { return entry.ptr == log_ptr; }), entries.end());
	}

	void toggle_visibility()
	{
		std::lock_guard<std::mutex> lock(mutex);
	
		if (in_combat) 
			in_combat_override = !in_combat_override;
		else
			open = !open;
	}

	ImVec2 window_size = ImVec2(0, 0);

private:
	bool open = false;
	bool in_combat = false;
	bool in_combat_override = false;

	std::mutex mutex;

	auto update_logs() -> void
	{
		for (auto& entry : entries)
			entry.update();
	}

	std::deque<LogTableEntry> entries;

	void draw_context_menu();

	enum Columns : uint8_t
	{
		SELECT,
		TIME,
		NAME,
		RESULT,
		DURATION,
		PARSER,
		DPS_REPORT,
		WINGMAN,
		_COUNT
	};
};