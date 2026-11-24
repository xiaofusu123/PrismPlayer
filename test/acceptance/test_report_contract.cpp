#include "support/Report.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

/**
 * @file test_report_contract.cpp
 * @brief 报告契约自检：失败用例必须带 `evidence.repro`，跳过用例必须带原因。
 *
 * 这些用例验证的是“报告写入器”本身，而不是产品；它们让 #87 的 JSON 契约
 * 在验收里可被机器检查，而不是只靠人工看一份样例。
 */

namespace {

std::string read_all(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

Prism::Qa::ReportContext make_context(const std::filesystem::path& path) {
    Prism::Qa::ReportContext context;
    context.binary = "test-acceptance.exe";
    context.path = path.string();
    context.preset = "contract-test";
    context.mode = Prism::Qa::MODE_IN_PROCESS;
    context.commit = "deadbeef";
    context.started_utc = "2026-01-01T00:00:00Z";
    return context;
}

} // namespace

TEST(ReportContract, SkipReasonIsExtractedFromGtestSummary) {
    // gtest 把 `GTEST_SKIP() <<` 的文本放进 skip 摘要；结果里只留可读原因。
    EXPECT_EQ(Prism::Qa::extract_skip_reason("缺 GPU，按约定跳过\n\n"), "缺 GPU，按约定跳过");
    EXPECT_EQ(Prism::Qa::extract_skip_reason("Skipped\n缺 GPU，按约定跳过\n"),
              "缺 GPU，按约定跳过");
    EXPECT_EQ(Prism::Qa::extract_skip_reason("Skipped"), "");
}

TEST(ReportContract, FailedCaseCarriesReproCommandAndFailedVerdict) {
    std::filesystem::path path =
        std::filesystem::temp_directory_path() / "prism-acceptance-failed.json";
    Prism::Qa::ReportContext context = make_context(path);

    std::vector<Prism::Qa::CaseResult> cases = {
        {"Sample.Fails", "failed", "", 12},
    };
    ASSERT_TRUE(Prism::Qa::write_report(context, cases, std::chrono::milliseconds{100}));

    std::string json = read_all(path);
    EXPECT_NE(json.find("\"verdict\": \"failed\""), std::string::npos);
    EXPECT_NE(json.find("--gtest_filter=Sample.Fails"), std::string::npos);
    // 复现命令要带上失败发生时的 preset / mode，否则换台机器照抄也复现不了。
    EXPECT_NE(json.find("PRISM_E2E_PRESET='contract-test'"), std::string::npos);
    EXPECT_NE(json.find("PRISM_E2E_MODE='in-process'"), std::string::npos);
    EXPECT_NE(json.find("\"status\": \"failed\""), std::string::npos);
    EXPECT_EQ(json.find("\"repro\": null"), std::string::npos);

    std::filesystem::remove(path);
}

TEST(ReportContract, EmptyRunIsNotGreen) {
    // filter 写错、一条用例都没跑时，不能因为“没有 failed”就判 passed。
    std::filesystem::path path =
        std::filesystem::temp_directory_path() / "prism-acceptance-empty.json";
    Prism::Qa::ReportContext context = make_context(path);

    ASSERT_TRUE(Prism::Qa::write_report(context, {}, std::chrono::milliseconds{1}));

    std::string json = read_all(path);
    EXPECT_NE(json.find("\"verdict\": \"failed\""), std::string::npos);

    std::filesystem::remove(path);
}

TEST(ReportContract, SkippedCaseCarriesReasonAndKeepsverdict_passed) {
    std::filesystem::path path =
        std::filesystem::temp_directory_path() / "prism-acceptance-skipped.json";
    Prism::Qa::ReportContext context = make_context(path);

    std::vector<Prism::Qa::CaseResult> cases = {
        {"Sample.Skips", "skipped", "缺 GPU，按约定跳过", 0},
    };
    ASSERT_TRUE(Prism::Qa::write_report(context, cases, std::chrono::milliseconds{5}));

    std::string json = read_all(path);
    EXPECT_NE(json.find("\"verdict\": \"passed\""), std::string::npos);
    EXPECT_NE(json.find("\"skip_reason\": \"缺 GPU，按约定跳过\""), std::string::npos);
    EXPECT_NE(json.find("\"repro\": null"), std::string::npos);

    std::filesystem::remove(path);
}

TEST(ReportContract, DefaultContextWritesNamedPendingBatches) {
    // 没有覆盖率来源时（旧调用方、只关心用例结果的运行），coverage 仍必须是三个具名
    // pending 批次：不能写出三个空 key，也不能把「没读过报告」伪装成 measured。
    std::filesystem::path path =
        std::filesystem::temp_directory_path() / "prism-acceptance-coverage-default.json";
    Prism::Qa::ReportContext context = make_context(path);

    std::vector<Prism::Qa::CaseResult> cases = {
        {"Sample.Passes", "passed", "", 1},
    };
    ASSERT_TRUE(Prism::Qa::write_report(context, cases, std::chrono::milliseconds{5}));

    std::string json = read_all(path);
    EXPECT_EQ(json.find("\"\": {"), std::string::npos);
    EXPECT_NE(json.find("\"av_sync\": {"), std::string::npos);
    EXPECT_NE(json.find("\"service\": {"), std::string::npos);
    EXPECT_NE(json.find("\"threadpool\": {"), std::string::npos);
    EXPECT_EQ(json.find("\"status\": \"measured\""), std::string::npos);
    EXPECT_NE(json.find("待 #84"), std::string::npos);

    std::filesystem::remove(path);
}

TEST(ReportContract, CoverageSectionCarriesMeasuredBatchesAndExpectation) {
    std::filesystem::path path =
        std::filesystem::temp_directory_path() / "prism-acceptance-coverage.json";
    Prism::Qa::ReportContext context = make_context(path);
    context.coverage = Prism::Qa::parse_coverage_report(
        "business/av_sync/src/SyncAlgorithm.cpp  16 8 50.00% 5 5 0.00% 38 19 50.00% 8 8 0.00%\n"
        "service/src/Player.cpp  252 252 0.00% 44 44 0.00% 399 399 0.00% 148 148 0.00%\n");

    std::vector<Prism::Qa::CaseResult> cases = {
        {"Sample.Passes", "passed", "", 1},
    };
    ASSERT_TRUE(Prism::Qa::write_report(context, cases, std::chrono::milliseconds{5}));

    std::string json = read_all(path);
    EXPECT_NE(json.find("\"coverage\": {"), std::string::npos);
    EXPECT_NE(json.find("\"av_sync\": {"), std::string::npos);
    EXPECT_NE(json.find("\"status\": \"measured\""), std::string::npos);
    // 38 行中 19 行未覆盖 → 恰好 50.00%。
    EXPECT_NE(json.find("\"line_percent\": 50.00"), std::string::npos);
    EXPECT_NE(json.find("\"covered_lines\": 19"), std::string::npos);
    EXPECT_NE(json.find("\"total_lines\": 38"), std::string::npos);
    // 80% 只是标注，不做阈值判定。
    EXPECT_NE(json.find("\"expected_percent\": 80"), std::string::npos);
    EXPECT_NE(json.find("\"verdict\": \"passed\""), std::string::npos);

    std::filesystem::remove(path);
}

TEST(ReportContract, CoverageSectionPendingSaysWaitFor84AndNeverFailsVerdict) {
    std::filesystem::path path =
        std::filesystem::temp_directory_path() / "prism-acceptance-coverage-pending.json";
    Prism::Qa::ReportContext context = make_context(path);
    context.coverage = Prism::Qa::load_coverage_summary("missing-coverage-report.txt");

    std::vector<Prism::Qa::CaseResult> cases = {
        {"Sample.Passes", "passed", "", 1},
    };
    ASSERT_TRUE(Prism::Qa::write_report(context, cases, std::chrono::milliseconds{5}));

    std::string json = read_all(path);
    EXPECT_NE(json.find("\"status\": \"pending\""), std::string::npos);
    EXPECT_NE(json.find("\"line_percent\": null"), std::string::npos);
    EXPECT_NE(json.find("待 #84"), std::string::npos);
    EXPECT_NE(json.find("\"verdict\": \"passed\""), std::string::npos);

    std::filesystem::remove(path);
}
