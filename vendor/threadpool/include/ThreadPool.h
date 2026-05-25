#pragma once

#include <thread>
#include <vector>
#include <queue>
#include <functional>

class ThreadPool {
public:
    ThreadPool(size_t min_thread_counts = 2, size_t max_thread_counts = 4, size_t);
    ~ThreadPool();

    // 删除拷贝构造函数和复制构造函数
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // 允许移动
    ThreadPool(ThreadPool&&) = default;
    ThreadPool& operator=(ThreadPool&&) = default;

    // 获取线程池信息
    size_t get_thread_count() const { return workers.size(); }
    size_t get_busy_threads() const { return busy_threads; }
    bool is_running() const { return !(stop); }

    // 线程池操作
    bool submit();
    bool stop();
    bool wait();

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void*()>> task_queue;
    std::atomic<size_t> busy_threads;

    // 线程同步
    mutable std::mutex mutex;
    std::condition_variable condition;

    // 状态标志
    std::atomic<bool> stop{false};
    std::atomic<int> submitted_tasks;
    std::atomic<int> completed_tasks;
};
