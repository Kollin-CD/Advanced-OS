#pragma once

#include <queue>
#include <vector>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <memory>
#include <future>
#include <functional>
#include <stdexcept>


class threadpool
{
public:
	threadpool(int num_threads);
	int num_threads;
	int size();
	template<class F, class... Args>
	auto enqueue(F&& f, Args&&... args)
		-> std::future<typename std::result_of<F(Args...)>::type>;
	~threadpool();

private:
	// Keep track of threads to join them later
	std::vector<std::thread> workers;
	// Task Queue
	std::queue<std::function<void()>> tasks;

	// Synchronization
	std::mutex queue_mutex;
	std::condition_variable condition;
};

inline threadpool::threadpool(int num_threads) {
	for (int i = 0; i < num_threads; ++i)
	{
		workers.emplace_back([this] {
			while(true) {
				std::function<void()> task;
				{
					std::unique_lock<std::mutex> lock(this->queue_mutex);
					this->condition.wait(lock, [this] { return !this->tasks.empty(); });
					task = this->tasks.front();
					this->tasks.pop();
				}

				task();
			}
		});
	}
}

// Add new work item to the pools
// How we will add the task of getting bids
template<class F, class... Args>
auto threadpool::enqueue(F&& f, Args&&... args)
	-> std::future<typename std::result_of<F(Args...)>::type>
{
	using return_type = typename std::result_of<F(Args...)>::type;
	auto task = std::make_shared<std::packaged_task<return_type()>>
		(std::bind(std::forward<F>(f), std::forward<Args>(args)...));

	std::future<return_type> res = task->get_future();
	{
		std::unique_lock<std::mutex> lock(queue_mutex);
		tasks.emplace([task](){ (*task)(); });
	}
	condition.notify_one();
	return res;
}

inline int threadpool::size() {
	return num_threads;
}

// Destructor to join all threads
inline threadpool::~threadpool() {
	{
		std::unique_lock<std::mutex> lock(queue_mutex);
	}
	condition.notify_all();
	for(std::thread &worker: workers)
		worker.join();
}