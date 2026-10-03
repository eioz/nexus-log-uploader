#pragma once

#include "log.h"
#include "module.h"

#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <thread>

struct SessionInfo
{
	std::string id;
	std::chrono::system_clock::time_point start_time{};
	std::chrono::system_clock::time_point end_time{};
	int log_count = 0;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(SessionInfo, id, start_time, end_time, log_count)
};

class SessionManager
{
public:
	void initialize();
	void release();

	void add_log(std::shared_ptr<Log> log);

	std::deque<std::shared_ptr<Log>> get_logs();
	std::filesystem::path get_reports_directory(std::shared_ptr<Log> log);

private:
	std::filesystem::path sessions_directory;

	std::mutex mutex;
	std::condition_variable save_cv;
	std::thread save_thread;
	bool running = false;

	std::map<std::string, SessionInfo> sessions;
	std::map<std::string, std::deque<std::shared_ptr<Log>>> session_logs; // loaded sessions, newest log first
	std::set<std::string> changed_sessions;
	std::string current_session_id;

	void run();
	void save();

	std::string create_session(std::chrono::system_clock::time_point time);
};

DECLARE_MODULE(SessionManager, session_manager)
