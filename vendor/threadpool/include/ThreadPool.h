#pragma once

#include <thread>
#include <vector>
#include <queue>
#include <future>
#include <functional>
#include <stdexcept>
#include <cstdlib>
#include <chrono>

class ThreadPool {
public:
    ThreadPool(size_t min_thread_count = 2, size_t max_thread_count = 4);
    ~ThreadPool();

    template<class F, class... Args>
    [[nodiscard]] auto push_task(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>;

    void start();
    void stop();
    void wait_all();
    
    size_t get_current_threads() const {
        std::lock_guard<std::mutex> lock(mutex);
        return workers.size();
    }
    bool is_running() const { return running.load(); }
    size_t get_total_tasks() const {
        std::lock_guard<std::mutex> lock(mutex);
        return total_tasks;
    }
    size_t get_hardware_threads() const { return std::thread::hardware_concurrency(); }
    size_t get_min_thread_count() const { return min_thread_count; }
    size_t get_max_thread_count() const { return max_thread_count; }

private:
    void create_work_thread(size_t count);
    void kill_work_thread();

    // 线程池属性
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> task_queue;

    size_t min_thread_count;
    size_t max_thread_count;
    size_t total_tasks{0};

    // 同步控制
    mutable std::mutex mutex;
    std::condition_variable condition;
    std::condition_variable wait_cv;
    std::condition_variable kill_cv;
    std::atomic<bool> running{true};

    // 动态伸缩
    std::atomic<size_t> pending_death{0};
    std::thread kill_thread;
    std::atomic<bool> kill_thread_running{true};
    std::atomic<bool> kill_thread_exited{false};
};

template<class F, class... Args>
[[nodiscard]] auto ThreadPool::push_task(F&& f, Args&&... args)
    -> std::future<std::invoke_result_t<F, Args...>>
{
    using return_type = std::invoke_result_t<F, Args...>;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    std::future<return_type> result = task->get_future();

    int current_tasks, current_threads;

    {
        std::lock_guard<std::mutex> lock(mutex);

        if (!running) {
            throw std::runtime_error("Thread pool is not running!");
        }

        task_queue.emplace([task]() {
            (*task)();
        });
        total_tasks++;

        current_tasks = total_tasks;
        current_threads = workers.size();
    }

    int count = static_cast<int>(current_tasks) - static_cast<int>(current_threads);
    if (count >= 1 && current_tasks <= max_thread_count) {
        create_work_thread(count);
    } else if (count <= -1 && current_tasks >= min_thread_count) {
        count = abs(count);
        pending_death += count;
        condition.notify_all();
        kill_cv.notify_all();
    }

    condition.notify_all();
    return result;
}
