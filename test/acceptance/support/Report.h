#pragma once

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <vector>

/**
 * @file Report.h
 * @brief 验收证据报告（#87 报告契约，JSON）与 GTest 用例收集。
 *
 * 字段名即契约：
 *   run / cases / coverage / verdict
 * 失败用例必须带 `evidence.repro`（可直接复制的命令）。
 * 报告路径由 `PRISM_E2E_REPORT` 给出；未设置时不写文件（构建期 gtest_discover_tests
 * 会执行本二进制一次，不能污染它的 stdout）。
 */

namespace Prism::Qa {

inline constexpr const char* MODE_IN_PROCESS = "in-process";
inline constexpr const char* MODE_MULTI_PROCESS = "multi-process";

/**
 * @struct CaseResult
 * @brief 一个验收用例的结果（报告 `cases[]` 的一项）。
 */
struct CaseResult {
    std::string name;
    std::string status;      /**< passed | failed | skipped */
    std::string skip_reason; /**< status=skipped 时必填 */
    long long duration_ms{0};
};

/**
 * @struct ReportContext
 * @brief 报告的运行上下文，来自编排脚本设置的环境变量。
 */
struct ReportContext {
    std::string binary;         /**< 本可执行文件路径（拼 `evidence.repro` 用，不写进报告） */
    std::string path;           /**< PRISM_E2E_REPORT；空表示不写文件 */
    std::string preset;         /**< PRISM_E2E_PRESET */
    std::string mode;           /**< PRISM_E2E_MODE：in-process | multi-process */
    std::string commit;         /**< PRISM_E2E_COMMIT */
    std::string log_path;       /**< PRISM_E2E_LOG */
    bool perf_requested{false}; /**< PRISM_E2E_PERF=1 */
    std::string started_utc;
};

/**
 * @brief 读取 `PRISM_E2E_*` 环境变量，拼出本次运行的报告上下文。
 * @param argc `main` 的 argc。
 * @param argv `main` 的 argv；`argv[0]` 用作 `evidence.repro` 里的可执行文件路径。
 * @return 报告上下文；未设置的环境变量取默认值（mode 默认 in-process）。
 */
ReportContext load_report_context(int argc, char** argv);

/**
 * @brief 构造一条 skipped 用例（不执行被测代码，只记录原因）。
 * @param name 用例名，与 gtest 的 `Suite.Name` 对齐。
 * @param reason 跳过原因，写进报告的 `skip_reason`。
 */
CaseResult make_skipped_case(std::string name, std::string reason);

/** @brief 从 gtest 的 skip part 摘要里取出 `GTEST_SKIP() <<` 写的可读原因。 */
std::string extract_skip_reason(const std::string& summary);

/**
 * @brief 生成失败用例的可复现命令（含 preset / mode 环境变量与 `--gtest_filter`）。
 * @param context 当前运行上下文（提供 binary、preset、mode）。
 * @param test_name 失败用例名。
 */
std::string make_repro(const ReportContext& context, const std::string& test_name);

/**
 * @brief 按 #87 报告的 JSON 契约写文件（字段名即契约）。
 * @param context 运行上下文；`path` 为空时调用方不应调用本函数。
 * @param cases 本次运行收集到的用例。
 * @param run_duration 整个可执行文件从启动到写报告前的耗时。
 * @return 文件是否成功写完整。
 */
bool write_report(const ReportContext& context, const std::vector<CaseResult>& cases,
                  std::chrono::milliseconds run_duration);

/**
 * @brief 判定 verdict：至少执行过一条用例，且没有 failed（skipped 不算失败）。
 * @param cases 本次运行收集到的用例。
 */
bool verdict_passed(const std::vector<CaseResult>& cases);

/**
 * @brief 把报告路径、用例计数与 verdict 打到 stdout（供编排脚本 / 人直接看）。
 * @param context 运行上下文。
 * @param cases 本次运行收集到的用例。
 */
void print_summary(const ReportContext& context, const std::vector<CaseResult>& cases);

/**
 * @class ReportListener
 * @brief 把 GTest 的每条用例收成 `CaseResult`，由 main 在跑完后统一写报告。
 */
class ReportListener final : public ::testing::EmptyTestEventListener {
  public:
    void OnTestStart(const ::testing::TestInfo& test_info) override;
    void OnTestPartResult(const ::testing::TestPartResult& part_result) override;
    void OnTestEnd(const ::testing::TestInfo& test_info) override;

    [[nodiscard]] const std::vector<CaseResult>& cases() const { return cases_; }

  private:
    std::vector<CaseResult> cases_;
    std::string current_name_;
    std::string current_skip_reason_;
    std::chrono::steady_clock::time_point current_started_{};
};

} // namespace Prism::Qa
