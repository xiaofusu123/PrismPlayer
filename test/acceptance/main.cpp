#include "support/Report.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstring>
#include <iostream>
#include <vector>

namespace {

bool has_flag(int argc, char** argv, const char* flag) {
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], flag) == 0) {
            return true;
        }
    }
    return false;
}

long long elapsed_ms(std::chrono::steady_clock::time_point started) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                                 started)
        .count();
}

/**
 * @brief 收尾：写报告 → 打摘要 → 用 verdict 决定退出码（两种运行模式共用）。
 */
int finish_run(const Prism::Qa::ReportContext& context,
               const std::vector<Prism::Qa::CaseResult>& cases, int gtest_rc,
               std::chrono::steady_clock::time_point started) {
    auto duration = std::chrono::milliseconds{elapsed_ms(started)};

    bool report_written = true;
    if (!context.path.empty()) {
        report_written = Prism::Qa::write_report(context, cases, duration);
    }
    Prism::Qa::print_summary(context, cases);

    if (!report_written) {
        std::cerr << "[acceptance] 无法写入报告：" << context.path << "\n";
        return 1;
    }
    if (cases.empty()) {
        std::cerr << "[acceptance] 没有用例被执行（检查 --gtest_filter）；verdict=failed\n";
        return 1;
    }
    return Prism::Qa::verdict_passed(cases) ? gtest_rc : 1;
}

} // namespace

/**
 * @brief 验收入口：跑 GTest 用例 → 收结果 → 写证据报告 → 用 verdict 决定退出码。
 *
 * 与 `gtest_main` 的差别只在“跑完之后”：`ReportListener` 收集每条用例，
 * 报告路径由编排脚本经 `PRISM_E2E_REPORT` 传入；未设置时只跑用例、不写文件
 * （构建期 `gtest_discover_tests` 会执行本二进制，不能污染它的 stdout / 落盘）。
 */
int main(int argc, char** argv) {
    // InitGoogleTest 会把已识别的 gtest 旗标从 argv 里摘掉，所以必须先判断。
    bool list_tests = has_flag(argc, argv, "--gtest_list_tests");

    ::testing::InitGoogleTest(&argc, argv);

    // CMake 的 gtest_discover_tests 在构建期带 --gtest_list_tests 执行本二进制：
    // 只允许 gtest 自己的列表输出，不能混入摘要、也不能写报告。
    if (list_tests) {
        return RUN_ALL_TESTS();
    }

    auto context = Prism::Qa::load_report_context(argc, argv);
    auto* listener = new Prism::Qa::ReportListener();
    ::testing::UnitTest::GetInstance()->listeners().Append(listener);

    auto started = std::chrono::steady_clock::now();

    // 模式 B（多进程）的用例由 #87 第 3 步交付。本票只保证编排入口能选中该模式，
    // 且“未交付”表现为 skip 而不是 fail（#87 决策 4）。
    if (context.mode == Prism::Qa::MODE_MULTI_PROCESS) {
        std::vector<Prism::Qa::CaseResult> cases;
        cases.push_back(Prism::Qa::make_skipped_case(
            "e2e-multi.multi_process", "模式 B（多进程）用例由 #87 第 3 步交付；本票只覆盖模式 A"));
        return finish_run(context, cases, 0, started);
    }

    int gtest_rc = RUN_ALL_TESTS();

    std::vector<Prism::Qa::CaseResult> cases = listener->cases();
    if (context.perf_requested) {
        cases.push_back(Prism::Qa::make_skipped_case(
            "perf.baseline", "性能基线由 #87 第 4 步交付；本票只验证 --perf 开关与 skip 语义"));
    }

    return finish_run(context, cases, gtest_rc, started);
}
