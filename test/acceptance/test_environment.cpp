#include <gtest/gtest.h>

/**
 * @file test_environment.cpp
 * @brief 环境缺件时的判定：skip（带原因），不是 fail。
 *
 * 模式 A 用假引擎，不需要声卡/GPU；真实设备、Server 二进制与覆盖率报告
 * 分别由后续票或 #84 交付。这些用例在报告里留下 `status=skipped` 的原因，
 * 而不是让整套验收红掉（#87 决策 4 / #88 AC）。
 */

TEST(Environment, RealDevicePlaybackIsNotRequiredByModeA) {
    GTEST_SKIP() << "模式 A 只验收假引擎注入与状态机；真实声卡/GPU 输出不在本票范围";
}

TEST(Environment, ServerBinaryIsNotPartOfModeA) {
    GTEST_SKIP() << "服务端实现已于 2026-11-23 移除（ADR 0008），多客户端用例待重写";
}

TEST(Environment, CoverageReportIsNotProducedByThisTicket) {
    GTEST_SKIP() << "覆盖率由 #84 的 build/Coverage 报告消费；本票不设阈值、不生成报告";
}
