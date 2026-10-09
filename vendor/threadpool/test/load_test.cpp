// 负载测试 - 持续压力下行为观测 (绕过 wait_all bug)
#include "common.hpp"
#include "ThreadPool.h"
#include <atomic>
#include <thread>
#include <future>

int main(int argc, char* argv[]) {
    Tier tier = parse_tier(argc, argv);
    std::cout << "=== 负载测试 [" << tier_name(tier) << "] ===\n";
    unsigned hw = std::thread::hardware_concurrency();
    std::cout << "硬件线程: " << hw << "\n";

    // 1. 持续吞吐量
    {
        double dur_s = (tier == Tier::Fast) ? 1.0 : (tier == Tier::Normal ? 5.0 : 30.0);
        unsigned tc = (hw > 8) ? 8 : hw;
        print_sep(("1. 持续吞吐量 (" + std::to_string((int)dur_s) + "s, pool=" + std::to_string(tc) + "/" + std::to_string(tc) + ")").c_str());

        ThreadPool pool(tc, tc);
        std::atomic<int> done{0}, submitted{0};
        std::atomic<bool> stop_flag{false};
        std::vector<std::future<void>> futs;

        std::thread producer([&] {
            auto t0 = std::chrono::high_resolution_clock::now();
            while (!stop_flag) {
                futs.push_back(pool.push_task([&] {
                    done++;
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }));
                submitted++;
                if (std::chrono::duration<double>(
                        std::chrono::high_resolution_clock::now() - t0).count() >= dur_s)
                    break;
                std::this_thread::sleep_for(std::chrono::microseconds(500));
            }
        });

        std::this_thread::sleep_for(std::chrono::duration<double>(dur_s + 2.0));
        stop_flag = true;
        producer.join();
        for (auto& f : futs) f.get();

        print_result({"提交数", std::to_string(submitted.load()), ""});
        print_result({"完成数", std::to_string(done.load()), ""});
    }

    // 2. 混合负载
    {
        int each = (tier == Tier::Fast) ? 200 : (tier == Tier::Normal ? 1000 : 20000);
        unsigned tc = (hw > 8) ? 8 : hw;
        print_sep(("2. 混合负载 (" + std::to_string(each) + " 短 + " + std::to_string(each) + " 长, pool=" + std::to_string(tc) + "/" + std::to_string(tc) + ")").c_str());

        ThreadPool pool(tc, tc);
        std::atomic<int> counter{0};
        std::vector<std::future<void>> futs; futs.reserve(each * 2);
        size_t peak_threads = 0;

        auto t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < each; ++i) {
            futs.push_back(pool.push_task([&] {
                counter++;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }));
            futs.push_back(pool.push_task([&] {
                counter++;
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }));
            if (i % 20 == 0) {
                size_t cur = pool.get_current_threads();
                if (cur > peak_threads) peak_threads = cur;
            }
        }

        auto t1 = std::chrono::high_resolution_clock::now();
        for (auto& f : futs) f.get();
        auto t2 = std::chrono::high_resolution_clock::now();

        print_result({"完成数", std::to_string(counter.load()), ""});
        print_result({"峰值线程", std::to_string(peak_threads), "/" + std::to_string(tc)});
        print_result({"提交耗时", dur_str(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0)), ""});
        print_result({"总耗时", dur_str(std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t0)), ""});
    }

    // 3. 队列深度
    {
        int burst = (tier == Tier::Fast) ? 500 : (tier == Tier::Normal ? 2000 : 50000);
        unsigned tc = (hw > 8) ? 8 : hw;
        print_sep(("3. 爆发队列深度 (" + std::to_string(burst) + " 个 10ms 任务, pool=" + std::to_string(tc) + "/" + std::to_string(tc) + ")").c_str());

        ThreadPool pool(tc, tc);
        std::atomic<int> counter{0};
        std::vector<std::future<void>> futs; futs.reserve(burst);

        auto sub_start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < burst; ++i)
            futs.push_back(pool.push_task([&] {
                counter++;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }));
        auto sub_end = std::chrono::high_resolution_clock::now();

        size_t max_q = 0;
        while (counter.load() < burst) {
            size_t cur = pool.get_total_tasks();
            if (cur > max_q) max_q = cur;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        for (auto& f : futs) f.get();

        print_result({"完成数", std::to_string(counter.load()), ""});
        print_result({"最大队列深度", std::to_string(max_q), ""});
        print_result({"提交耗时", dur_str(std::chrono::duration_cast<std::chrono::nanoseconds>(sub_end - sub_start)), ""});
    }

    std::cout << "\n=== 负载测试完成 ===\n";
    return 0;
}
