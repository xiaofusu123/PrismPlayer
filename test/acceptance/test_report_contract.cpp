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
