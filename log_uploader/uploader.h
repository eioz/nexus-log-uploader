#pragma once

#include "log.h"
#include "module.h"

#include <queue>

class Uploader
{
public:
	Uploader() = default;
	~Uploader() = default;

	virtual void add_log(std::shared_ptr<Log> log) = 0;

	void initialize()
	{
		initialized.store(true);
		upload_thread = std::thread(&Uploader::run, this);
	}

	void clear_upload_queue()
	{
		std::lock_guard lock(this->upload_queue_mutex);
		std::queue<std::shared_ptr<Log>> empty;
		std::swap(upload_queue, empty);
	}

	void release()
	{
		initialized.store(false);

		clear_upload_queue();

		upload_cv.notify_all();

		if (this->upload_thread.joinable())
			this->upload_thread.join();
	}

protected:
	std::condition_variable upload_cv;

	std::mutex upload_queue_mutex;
	std::queue<std::shared_ptr<Log>> upload_queue;

	std::thread upload_thread;

	std::atomic<bool> initialized = false;

	virtual void run() = 0;
};