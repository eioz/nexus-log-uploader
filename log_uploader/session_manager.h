#pragma once

#include "log.h"
#include "module.h"

#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <thread>
#include <vector>

struct SessionInfo
{
	std::string id;
	std::chrono::system_clock::time_point start_time{};
	std::chrono::system_clock::time_point end_time{};
	int log_count = 0;
	bool archived = false;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(SessionInfo, id, start_time, end_time, log_count, archived)
};

class SessionManager
{
public:
	void initialize();
	void release();

	void add_log(std::shared_ptr<Log> log);
	void start_new_session();
	void resume_session(const std::string& session_id);
	bool set_archived(const std::string& session_id, bool archived);
	bool delete_session(const std::string& session_id);

	std::vector<SessionInfo> get_sessions();
	std::optional<SessionInfo> get_session(const std::string& session_id);
	std::deque<std::shared_ptr<Log>> get_logs(const std::string& session_id);
	std::string get_current_session_id();

	// waits until archiving has removed the old reports, fails while the archived session cannot be saved
	std::filesystem::path get_reports_directory(std::shared_ptr<Log> log);

private:
	std::filesystem::path sessions_directory;

	std::mutex mutex;
	std::condition_variable session_cv;
	std::condition_variable remove_cv;
	std::thread session_thread;
	bool running = false;

	std::map<std::string, SessionInfo> sessions;
	std::map<std::string, std::deque<std::shared_ptr<Log>>> session_logs; // loaded sessions, newest log first
	std::set<std::string> changed_sessions;
	std::set<std::string> load_requests;
	std::vector<std::filesystem::path> remove_requests; // kept until the files are removed
	std::vector<std::filesystem::path> kept_reports;    // reports of sessions that failed to save, kept until they are saved
	std::string current_session_id;
	bool index_save_required = false;

	void run();
	void save();
	void load_session(const std::string& id);

	std::string create_session(std::chrono::system_clock::time_point time);
};

DECLARE_MODULE(SessionManager, session_manager)
