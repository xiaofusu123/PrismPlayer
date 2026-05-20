#pragma once

#include <thread>
#include <vector>
#include <queue>
#include <functional>

class ThreadPool {
public:
    ThreadPool(size_t max_thread_counts, size_t min_thread_counts, size_t);
    ~ThreadPool();


private:
    std::vector<std::thread> workers;
    std::queue<std::function<void*>> task_queue;

    // 线程同步
    mutable std::mutex mutex;
    std::condition_variable condition;
    std::atomic<bool> stop{false};
};
