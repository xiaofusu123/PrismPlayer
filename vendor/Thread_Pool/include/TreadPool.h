#pragma once

#include <thread>
#include <vector>
#include <queue>
#include <functional>

class ThreadPool {
public:
    ThreadPool(size_t max_thread_counts, size_t min_thread_counts, size_t);
    ~ThreadPool();

    // 删除拷贝构造函数和复制构造函数
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // 允许移动
    ThreadPool(ThreadPool&&) = default;
    ThreadPool& operator=(ThreadPool&&) = default;


    size_t get_thread_count() const { return workers.size(); }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void*()>> task_queue;

    // 线程同步
    mutable std::mutex mutex;
    std::condition_variable condition;

    // 状态标志
    std::atomic<bool> stop{false};
    std::atomic<int> submitted_tasks;
    std::atomic<int> completed_tasks;
};
