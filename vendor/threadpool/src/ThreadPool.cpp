#include "ThreadPool.h"

ThreadPool::ThreadPool(size_t min_thread_count, size_t max_thread_count)
    : kill_thread([this]() { this->kill_work_thread(); }) {
    size_t hardware_threads = get_hardware_threads();

    if (min_thread_count > hardware_threads) {
        throw std::runtime_error("The minimum thread count is more than hardware threads");
    }

    if (max_thread_count > hardware_threads) {
        max_thread_count = hardware_threads;
    }

    this->min_thread_count = min_thread_count;
    this->max_thread_count = max_thread_count;

    create_work_thread(min_thread_count);
}

ThreadPool::~ThreadPool() {
    stop();

    kill_thread_exited = true;
    kill_cv.notify_all();
    if (kill_thread.joinable()) {
        kill_thread.join();
    }

    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::start() {
    {
        std::unique_lock<std::mutex> lock(this->mutex);
        running = true;
        kill_thread_running = true;
    }

    create_work_thread(min_thread_count);
}

void ThreadPool::stop() {
    {
        std::unique_lock<std::mutex> lock(this->mutex);
        running = false;
        kill_thread_running = false;
    }

    condition.notify_all();
    kill_cv.notify_all();
}

void ThreadPool::wait_all() {
    std::unique_lock<std::mutex> lock(mutex);
    wait_cv.wait(lock, [this]() {
        return this->total_tasks == 0;
    });
}

void ThreadPool::create_work_thread(size_t count) {
    for (int i = 0; i < count; i++) {
        std::lock_guard<std::mutex> lock(mutex);

        workers.emplace_back([this]() {

            while (true) {
                std::function<void()> task;

                {
                    std::unique_lock<std::mutex> lock(this->mutex);

                    this->condition.wait(lock, [this]() {
                        return !this->running || !this->task_queue.empty()
                            || (this->pending_death > 0 && this->task_queue.empty());
                    });

                    if (!this->running && this->task_queue.empty()) {
                        return;
                    }

                    size_t prev = pending_death.load();
                    if (prev > 0 && this->task_queue.empty()
                        && workers.size() > min_thread_count) {
                        if (pending_death.compare_exchange_weak(prev, prev - 1)) {
                            return;
                        }
                    }

                    if (!this->task_queue.empty()) {
                        task = std::move(this->task_queue.front());
                        this->task_queue.pop();
                    } else {
                        continue;
                    }
                }

                task();

                {
                    std::lock_guard<std::mutex> lock(mutex);
                    total_tasks--;

                    if (total_tasks == 0) {
                        wait_cv.notify_all();
                    }
                }
            }
        });
    }
}

void ThreadPool::kill_work_thread() {
    while (!kill_thread_exited) {
        if (pending_death > 0) {
            std::unique_lock<std::mutex> lock(mutex);

            kill_cv.wait(lock, [this]() {
                return !this->kill_thread_running || pending_death > 0
                    || (total_tasks > 0 && kill_thread_exited);
            });

            for (auto it = workers.begin(); it < workers.end(); ) {
                if (it->joinable()) {
                    it->join();
                    it = workers.erase(it);
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
