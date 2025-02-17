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

	auto initialize() -> void
	{
		initialized.store(true);
		upload_thread = std::thread(&Uploader::run, this);
	}

	auto clear_upload_queue() -> void
	{
		std::lock_guard lock(this->upload_queue_mutex);
		std::queue<std::shared_ptr<Log>> empty;
		std::swap(upload_queue, empty);
	}

	auto release() -> void
	{
		initialized.store(false);

		clear_upload_queue();

		upload_cv.notify_all();

		if (this->upload_thread.joinable())
			this->upload_thread.join();
	};

protected:
	std::mutex upload_mutex;
	std::condition_variable upload_cv;

	std::mutex upload_queue_mutex;
	std::queue<std::shared_ptr<Log>> upload_queue;

	std::thread upload_thread;

	std::atomic<bool> initialized = false;

	virtual void run() = 0;
};