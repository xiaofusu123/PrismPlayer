#pragma once

#include <array>
#include <cstddef>
#include <string>

/**
 * @file Coverage.h
 * @brief 消费 #84 的 `llvm-cov report` 文本，按分批口径聚合行覆盖率（#93）。
 *
 * #84 产出 `build/<Platform>/coverage/coverage-report.txt`。本模块只解析与聚合，
 * 不实现统计工具链、不设阈值、不重复决定口径（#93 AC）：批次与路径前缀对齐
 * `docs/markdown/代码规范.md` §7.1 —— ① `business/av_sync/src/`、② `service/src/`、
 * ③ `threadpool/`。报告缺失或某批次没有源文件时标记为待 #84，而不是谎报 0%。
 */

namespace Prism::Qa {

/// 期望行覆盖率，仅作报告里的标注，不参与判定（#93 AC）。
inline constexpr double COVERAGE_EXPECTED_PERCENT = 80.0;

/// 分批覆盖率的固定顺序：av_sync、service、threadpool（与 #87 报告契约同名）。
inline constexpr std::size_t COVERAGE_BATCH_COUNT = 3;

/**
 * @struct BatchCoverage
 * @brief 单个批次的行覆盖率结果；没有数字时 `measured == false` 且 `reason` 非空。
 */
struct BatchCoverage {
    std::string name;          /**< 批次名，与 #87 报告契约同名 */
    bool measured{false};      /**< 是否拿到该批次的行覆盖数字 */
    long long lines{0};        /**< 该批次的总行数 */
    long long missed_lines{0}; /**< 其中未覆盖的行数 */
    /// 没有数字时的原因（#84 未就绪时以「待 #84」开头）；measured 时为空。
    std::string reason{"待 #84：未读取覆盖率报告"};

    [[nodiscard]] long long covered_lines() const { return lines - missed_lines; }
    /// 行覆盖率百分比；分母为 0 时返回 0.0（调用方应结合 measured 判断）。
    [[nodiscard]] double line_percent() const;
};

/**
 * @struct CoverageSummary
 * @brief 三个批次的聚合结果；默认值就是「没有读取报告」的 pending 状态。
 */
struct CoverageSummary {
    std::string source; /**< 读取的报告路径；空表示没有来源 */
    /// 固定三个批次；名字在这里给好，调用方漏填时也不会写出空 key。
    std::array<BatchCoverage, COVERAGE_BATCH_COUNT> batches{
        BatchCoverage{"av_sync"}, BatchCoverage{"service"}, BatchCoverage{"threadpool"}};

    [[nodiscard]] bool any_measured() const;
    /// 按批次名取结果；未知名字返回一个带原因的 pending 占位，便于调用方统一处理。
    [[nodiscard]] const BatchCoverage& batch(const std::string& name) const;
};

/// 解析 `llvm-cov report` 的文本输出。空文本或格式不可识别时全部标记为待 #84。
[[nodiscard]] CoverageSummary parse_coverage_report(const std::string& text);

/// 读取 `llvm-cov report` 报告文件。文件缺失时全部标记为待 #84，不抛异常。
[[nodiscard]] CoverageSummary load_coverage_summary(const std::string& path);

} // namespace Prism::Qa
