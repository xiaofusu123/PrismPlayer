#include "support/Coverage.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

/**
 * @file test_coverage.cpp
 * @brief 覆盖率接入自检（#93）：解析 #84 的 `llvm-cov report` 文本，按分批口径聚合。
 *
 * 解析与聚合用固定文本自测（不依赖构建树）；最后一条用例消费真实的
 * `build/<Platform>/coverage/coverage-report.txt`，缺件时 skip 并写明“待 #84”，
 * 不让“没数字”把验收判红（#87 决策 4 / #93 AC）。
 */

namespace {

// 与 #84 的 `llvm-cov report` 输出同形：文件名 + 12 列，最后一行是 TOTAL。
constexpr const char* LLVM_COV_REPORT =
    R"(Filename                                             Regions    Missed Regions     Cover   Functions  Missed Functions  Executed       Lines      Missed Lines     Cover    Branches   Missed Branches     Cover
----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
business/av_sync/src/CommandDispatcher.cpp                63                63     0.00%           9                 9     0.00%         116               116     0.00%          50                50     0.00%
business/av_sync/src/SyncAlgorithm.cpp                    16                 8    50.00%           5                 5     0.00%          38                19    50.00%           8                 8     0.00%
service/src/Player.cpp                                   252               252     0.00%          44                44     0.00%         399               399     0.00%         148               148     0.00%
vendor/threadpool/ThreadPool.h                             4                 1    25.00%           4                 1    75.00%           8                 2    75.00%           0                 0         -
test/acceptance/test_lifecycle.cpp                         9                 9     0.00%           9                 9     0.00%          23                23     0.00%           0                 0         -
TOTAL                                                    438               411     6.16%         111                84    24.32%         737               676     8.28%         280               280     0.00%
)";

const char* env_or_empty(const char* name) {
    const char* value = std::getenv(name);
    return value == nullptr ? "" : value;
}

} // namespace

TEST(CoverageParser, AggregatesRowsByBatchAndIgnoresTestOwnCode) {
    Prism::Qa::CoverageSummary summary = Prism::Qa::parse_coverage_report(LLVM_COV_REPORT);

    ASSERT_TRUE(summary.any_measured());
    const Prism::Qa::BatchCoverage& av_sync = summary.batch("av_sync");
    EXPECT_TRUE(av_sync.measured);
    EXPECT_EQ(av_sync.lines, 116 + 38);
    EXPECT_EQ(av_sync.missed_lines, 116 + 19);

    const Prism::Qa::BatchCoverage& service = summary.batch("service");
    EXPECT_TRUE(service.measured);
    EXPECT_EQ(service.lines, 399);
    EXPECT_EQ(service.missed_lines, 399);

    const Prism::Qa::BatchCoverage& threadpool = summary.batch("threadpool");
    EXPECT_TRUE(threadpool.measured);
    EXPECT_EQ(threadpool.lines, 8);
    EXPECT_EQ(threadpool.missed_lines, 2);

    // test/ 的源码不计入任何批次（与 #84 的 ignore 口径一致）。
    EXPECT_EQ(av_sync.lines + service.lines + threadpool.lines, 116 + 38 + 399 + 8);
}

TEST(CoverageParser, ComputesLinePercentFromCoveredOverTotal) {
    Prism::Qa::CoverageSummary summary = Prism::Qa::parse_coverage_report(LLVM_COV_REPORT);

    const Prism::Qa::BatchCoverage& av_sync = summary.batch("av_sync");
    EXPECT_NEAR(av_sync.line_percent(), 100.0 * 19 / 154, 1e-9);
    EXPECT_NEAR(summary.batch("service").line_percent(), 0.0, 1e-9);
    EXPECT_NEAR(summary.batch("threadpool").line_percent(), 75.0, 1e-9);
}

TEST(CoverageParser, TotalRowIsNotCountedAgain) {
    Prism::Qa::CoverageSummary summary = Prism::Qa::parse_coverage_report(LLVM_COV_REPORT);

    // TOTAL 行若被当成普通文件，service 的行数会翻倍；这里钉住它被跳过。
    EXPECT_EQ(summary.batch("service").lines, 399);
}

TEST(CoverageParser, BatchWithoutRowsIsPendingWithWaitFor84Reason) {
    Prism::Qa::CoverageSummary summary = Prism::Qa::parse_coverage_report(
        "business/av_sync/src/SyncAlgorithm.cpp  16 8 50.00% 5 5 0.00% 38 19 50.00% 8 8 0.00%\n");

    EXPECT_TRUE(summary.batch("av_sync").measured);
    const Prism::Qa::BatchCoverage& service = summary.batch("service");
    EXPECT_FALSE(service.measured);
    EXPECT_NE(service.reason.find("待 #84"), std::string::npos);
    // 没有数字 ≠ 0%：分母为 0 时不能谎报 0%。
    EXPECT_EQ(service.line_percent(), 0.0);
}

TEST(CoverageParser, ThreadPoolGapIsNotMiscastAsWaitingFor84) {
    // #84 已交付；batch ③ 没数字是因为口径排除 vendor/，不能写成「等 #84 就绪」。
    Prism::Qa::CoverageSummary summary = Prism::Qa::parse_coverage_report(
        "business/av_sync/src/SyncAlgorithm.cpp  16 8 50.00% 5 5 0.00% 38 19 50.00% 8 8 0.00%\n");

    const Prism::Qa::BatchCoverage& threadpool = summary.batch("threadpool");
    EXPECT_FALSE(threadpool.measured);
    EXPECT_NE(threadpool.reason.find("无法测量"), std::string::npos);
    EXPECT_EQ(threadpool.reason.find("待 #84"), std::string::npos);
}

TEST(CoverageParser, WindowsBackslashPathsAreNormalized) {
    Prism::Qa::CoverageSummary summary = Prism::Qa::parse_coverage_report(
        "D:\\Program\\Code\\PrismPlayer\\client\\service\\src\\Player.cpp  1 0 100.00% 1 0 "
        "100.00% 10 0 100.00% 0 0 -\n"
        "client/service/src/Player.cpp  1 0 100.00% 1 0 100.00% 6 0 100.00% 0 0 -\n");

    EXPECT_TRUE(summary.batch("service").measured);
    EXPECT_EQ(summary.batch("service").lines, 16);
}

TEST(CoverageParser, MissingReportIsPendingInsteadOfFailing) {
    Prism::Qa::CoverageSummary summary =
        Prism::Qa::load_coverage_summary("definitely-not-a-real-coverage-report.txt");

    EXPECT_FALSE(summary.any_measured());
    EXPECT_FALSE(summary.source.empty());
    const Prism::Qa::BatchCoverage& av_sync = summary.batch("av_sync");
    EXPECT_FALSE(av_sync.measured);
    EXPECT_NE(av_sync.reason.find("待 #84"), std::string::npos);
}

TEST(Coverage, RealReportIsConsumedWhenAvailable) {
    std::string path = env_or_empty("PRISM_E2E_COVERAGE_REPORT");
    if (path.empty()) {
        GTEST_SKIP() << "覆盖率统计（#84）未就绪：未设置 PRISM_E2E_COVERAGE_REPORT";
    }

    Prism::Qa::CoverageSummary summary = Prism::Qa::load_coverage_summary(path);
    if (!summary.any_measured()) {
        GTEST_SKIP() << "覆盖率统计（#84）未就绪：" << summary.batch("av_sync").reason;
    }

    // 真实报告至少覆盖到 Service 的 32 个 C API（Player.cpp）；缺数字的批次必须带原因。
    EXPECT_TRUE(summary.batch("service").measured);
    for (const Prism::Qa::BatchCoverage& batch : summary.batches) {
        if (batch.measured) {
            EXPECT_GE(batch.line_percent(), 0.0);
            EXPECT_LE(batch.line_percent(), 100.0);
        } else {
            EXPECT_FALSE(batch.reason.empty());
        }
    }
}
