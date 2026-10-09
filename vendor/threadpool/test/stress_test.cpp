// 压力测试 - 极限条件下的稳定性 (绕过 wait_all bug)
#include "common.hpp"
#include "ThreadPool.h"
#include <atomic>
#include <thread>
#include <future>

int main(int argc, char* argv[]) {
    Tier tier = parse_tier(argc, argv);
    std::cout << "=== 压力测试 [" << tier_name(tier) << "] ===\n";
    unsigned hw = std::thread::hardware_concurrency();
    unsigned tc = (hw > 8) ? 8 : hw;
    std::cout << "硬件线程: " << hw << "  测试线程: " << tc << "\n";

    // 1. 快速启停
    {
        int cycles = (tier == Tier::Fast) ? 10 : (tier == Tier::Normal ? 50 : 1000);
        print_sep(("1. 快速启停 (" + std::to_string(cycles) + " 次, pool=4/4)").c_str());

        std::chrono::nanoseconds total;
        {
            Timer t(&total);
            for (int i = 0; i < cycles; ++i) {
                ThreadPool pool(4, 4);
                pool.stop();
            }
        }
        print_result({"循环次数", std::to_string(cycles), "无崩溃"});
        print_result({"总耗时", dur_str(total), ""});
    }

    // 2. 任务爆发正确性
    {
        int n = (tier == Tier::Fast) ? 1000 : (tier == Tier::Normal ? 5000 : 100000);
        print_sep(("2. 任务爆发 (" + std::to_string(n) + " 任务, pool=" + std::to_string(tc) + "/" + std::to_string(tc) + ")").c_str());

        ThreadPool pool(tc, tc);
        std::atomic<long long> sum{0};
        std::vector<std::future<void>> futs; futs.reserve(n);

        for (int i = 0; i < n; ++i)
            futs.push_back(pool.push_task([&, i] { sum += i; }));
        for (auto& f : futs) f.get();

        long long expected = static_cast<long long>(n) * (n - 1) / 2;
        bool ok = (sum.load() == expected);
        print_result({"期望值", std::to_string(expected), ""});
        print_result({"实际值", std::to_string(sum.load()), ""});
        print_result({"结果", ok ? "PASS" : "FAIL", ""});
    }

    // 3. 过载提交
    {
        int n = (tier == Tier::Fast) ? 200 : (tier == Tier::Normal ? 500 : 10000);
        print_sep(("3. 过载提交 (" + std::to_string(n) + " x 10ms 任务, pool=4/4)").c_str());

        ThreadPool pool(4, 4);
        std::atomic<int> counter{0};
        std::vector<std::future<void>> futs; futs.reserve(n);

        for (int i = 0; i < n; ++i)
            futs.push_back(pool.push_task([&] {
                counter++;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }));
        for (auto& f : futs) f.get();

        bool ok = (counter.load() == n);
        print_result({"完成数", std::to_string(counter.load()), ""});
        print_result({"期望数", std::to_string(n), ""});
        print_result({"结果", ok ? "PASS" : "FAIL", ""});
    }

    // 4. 并发生产者
    {
        int nprod = (tier == Tier::Fast) ? 2 : (tier == Tier::Normal ? 5 : 20);
        int each = (tier == Tier::Fast) ? 200 : (tier == Tier::Normal ? 500 : 5000);
        print_sep(("4. 并发生产者 (" + std::to_string(nprod) + " 线程 x " + std::to_string(each) + " 任务, pool=" + std::to_string(tc) + "/" + std::to_string(tc) + ")").c_str());

        ThreadPool pool(tc, tc);
        std::atomic<int> counter{0};

        std::vector<std::thread> producers;
        for (int p = 0; p < nprod; ++p) {
            producers.emplace_back([&, each] {
                std::vector<std::future<void>> local; local.reserve(each);
                for (int i = 0; i < each; ++i)
                    local.push_back(pool.push_task([&] { counter++; }));
                for (auto& f : local) f.get();
            });
        }
        for (auto& t : producers) t.join();

        int expected = nprod * each;
        bool ok = (counter.load() == expected);
        print_result({"完成数", std::to_string(counter.load()), ""});
        print_result({"期望数", std::to_string(expected), ""});
        print_result({"结果", ok ? "PASS" : "FAIL", ""});
    }

    // 5. 边界条件
    {
        print_sep("5. 边界: stop 后 push_task (pool=4/4)");
        ThreadPool pool(4, 4);
        pool.stop();
        bool caught = false;
        try {
            pool.push_task([] {});
        } catch (const std::runtime_error& e) {
            caught = true;
            std::cout << "  异常: " << e.what() << "\n";
        }
        print_result({"异常捕获", caught ? "PASS" : "FAIL", ""});
    }

    std::cout << "\n=== 压力测试完成 ===\n";
    return 0;
}
