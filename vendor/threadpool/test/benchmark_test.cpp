// 线程池基准测试
#include "common.hpp"
#include "ThreadPool.h"
#include <future>
#include <numeric>
#include <algorithm>

int main(int argc, char* argv[]) {
    Tier tier = parse_tier(argc, argv);
    std::cout << "=== 基准测试 [" << tier_name(tier) << "] ===\n";

    unsigned hw = std::thread::hardware_concurrency();
    unsigned tc = (hw > 8) ? 8 : hw;
    std::cout << "硬件线程: " << hw << "  测试线程: " << tc << "\n";

    // 1. 单任务往返延迟
    {
        const int N = 50;
        print_sep("1. 单任务往返延迟 (50 样本, pool=2/4)");
        ThreadPool pool;
        std::vector<double> lats;
        for (int i = 0; i < N; ++i) {
            std::chrono::nanoseconds elap;
            {
                Timer t(&elap);
                pool.push_task([] {}).get();
            }
            lats.push_back(std::chrono::duration<double, std::micro>(elap).count());
        }
        std::sort(lats.begin(), lats.end());
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1)
           << "中位数=" << lats[N/2] << "us  均值=" << (std::accumulate(lats.begin(),lats.end(),0.0)/N)
           << "us  最小=" << lats.front() << "us  最大=" << lats.back() << "us";
        print_result({"往返延迟", ss.str(), ""});
        pool.stop();
    }

    // 2. 批量吞吐量
    {
        int n = (tier == Tier::Fast) ? 1000 : (tier == Tier::Normal ? 5000 : 100000);
        print_sep(("2. 批量吞吐量 (" + std::to_string(n) + " 任务, pool=" + std::to_string(tc) + ")").c_str());

        ThreadPool pool(tc, tc);
        std::chrono::nanoseconds elap;
        {
            Timer t(&elap);
            for (int i = 0; i < n; ++i)
                pool.push_task([] {});
            pool.wait_all();
        }
        double secs = std::chrono::duration<double>(elap).count();
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(0) << (n / secs);
        print_result({"吞吐量", ss.str(), "tasks/s"});
        print_result({"总耗时", dur_str(elap), ""});
    }

    // 3. 线程伸缩效率 (扩展到 8 线程)
    {
        int n = (tier == Tier::Fast) ? 500 : (tier == Tier::Normal ? 2000 : 2000);
        print_sep(("3. 线程伸缩效率 (" + std::to_string(n) + " 任务)").c_str());

        double baseline = 0;
        std::cout << "  " << std::left << std::setw(12) << "线程数"
                  << std::right << std::setw(12) << "耗时"
                  << "  " << std::setw(8) << "加速比"
                  << "  " << "效率\n";

        for (unsigned th : {1u, 2u, 4u, 8u}) {
            ThreadPool pool(th, th);
            std::chrono::nanoseconds elap;
            {
                Timer t(&elap);
                for (int i = 0; i < n; ++i)
                    pool.push_task([] {});
                pool.wait_all();
            }
            double secs = std::chrono::duration<double>(elap).count();
            if (th == 1) baseline = secs;
            double speedup = baseline / secs;
            std::cout << "  " << std::left << std::setw(12) << th
                      << std::right << std::setw(12) << dur_str(elap)
                      << "  " << std::fixed << std::setprecision(2) << std::setw(6) << speedup << "x"
                      << "  " << std::fixed << std::setprecision(1) << (speedup / th * 100) << "%\n";
        }
    }

    // 4. Future 解析延迟
    {
        const int N = 50;
        print_sep("4. Future 解析延迟 (50 样本, pool=2/4)");
        ThreadPool pool;
        std::vector<double> lats;

        for (int i = 0; i < N; ++i) {
            std::chrono::high_resolution_clock::time_point finish;
            auto fut = pool.push_task([&] { finish = std::chrono::high_resolution_clock::now(); });
            fut.get();
            double us = std::chrono::duration<double, std::micro>(
                std::chrono::high_resolution_clock::now() - finish).count();
            lats.push_back(us);
        }

        std::sort(lats.begin(), lats.end());
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1)
           << "中位数=" << lats[N/2] << "us  均值=" << (std::accumulate(lats.begin(),lats.end(),0.0)/N) << "us";
        print_result({"Future 延迟", ss.str(), ""});
        pool.stop();
    }

    std::cout << "\n=== 基准测试完成 ===\n";
    return 0;
}
