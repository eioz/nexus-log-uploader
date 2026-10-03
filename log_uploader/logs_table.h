#pragma once

#include "log.h"

#include <imgui.h>

#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>

struct LogView
{
	bool selected = false;

	std::string name;
	std::string time;
	std::string result;
	std::string duration;
	std::string time_ago;
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

		{
			std::shared_lock lock(ptr->mutex);
			this->data = ptr->get_data();
		}

		update_view();
	}

	void update()
	{
		auto view_update_required = false;

		{
			std::shared_lock lock(ptr->mutex);
			if (view_update_required = ptr->view_updated_required.exchange(false))
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

	void toggle_visibility()
	{
		std::lock_guard<std::mutex> lock(mutex);
		open = !open;
		if (in_combat)
			in_combat_override = open;
	}

	bool is_visible() const { return open; }

	void set_visible(bool v)
	{
		std::lock_guard<std::mutex> lock(mutex);
		open = v;
		if (in_combat)
			in_combat_override = v;
	}

	ImVec2 window_size = ImVec2(0, 0);

private:
	bool open = false;
	bool in_combat = false;
	bool in_combat_override = false;

	std::mutex mutex;

	std::deque<LogTableEntry> entries;

	std::string viewed_session_id; // empty for the current session

	void update_entries();
	void draw_context_menu();
	void draw_session_menu();

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