#include <gtest/gtest.h>

/**
 * @file test_environment.cpp
 * @brief 环境缺件时的判定：skip（带原因），不是 fail。
 *
 * 模式 A 用假引擎，不需要声卡/GPU；真实设备、Server 二进制与 ThreadPool 覆盖率
 * 分别由后续票 / ADR 0008 / #84 的口径决定。这些用例在报告里留下
 * `status=skipped` 的原因，而不是让整套验收红掉（#87 决策 4 / #88 AC）。
 * 覆盖率数字本身由 `test_coverage.cpp` 消费 #84 的报告（#93）。
 */

TEST(Environment, RealDevicePlaybackIsNotRequiredByModeA) {
    GTEST_SKIP() << "模式 A 只验收假引擎注入与状态机；真实声卡/GPU 输出不在本票范围";
}

TEST(Environment, ServerBinaryIsNotPartOfModeA) {
    GTEST_SKIP() << "服务端实现已于 2026-11-23 移除（ADR 0008），多客户端用例待重写";
}

TEST(Environment, ThreadPoolCoverageIsNotMeasurableUnderCurrentPolicy) {
    // 报告里 threadpool 节同步标记 pending（带同一原因），verdict 不受影响。
    GTEST_SKIP()
        << "无法测量：覆盖率口径排除 vendor/，且 vendor/threadpool 不参与构建，无行覆盖数字";
}
