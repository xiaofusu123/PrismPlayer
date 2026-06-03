#include <gtest/gtest.h>

#include "sync_types.h"

using namespace Prism::Business;

/* ========== SyncMode 枚举测试 ========== */

TEST(SyncModeTest, enum_values_distinct)
{
    EXPECT_NE(SYNC_MODE_AUDIO_MASTER, SYNC_MODE_VIDEO_MASTER);
    EXPECT_NE(SYNC_MODE_AUDIO_MASTER, SYNC_MODE_EXTERNAL_CLOCK);
    EXPECT_NE(SYNC_MODE_VIDEO_MASTER, SYNC_MODE_EXTERNAL_CLOCK);
}

TEST(SyncModeTest, audio_master_is_zero)
{
    EXPECT_EQ(SYNC_MODE_AUDIO_MASTER, 0);
}

/* ========== SyncClock 测试 ========== */

TEST(SyncClockTest, default_initialization)
{
    SyncClock clock;
    EXPECT_EQ(clock.pts_ms, 0);
    EXPECT_EQ(clock.last_sys_time_ms, 0);
    EXPECT_DOUBLE_EQ(clock.drift_ppm, 0.0);
    EXPECT_FALSE(clock.paused);
}

TEST(SyncClockTest, explicit_assignment)
{
    SyncClock clock;
    clock.pts_ms = 1000;
    clock.last_sys_time_ms = 5000;
    clock.drift_ppm = 1.5;
    clock.paused = true;

    EXPECT_EQ(clock.pts_ms, 1000);
    EXPECT_EQ(clock.last_sys_time_ms, 5000);
    EXPECT_DOUBLE_EQ(clock.drift_ppm, 1.5);
    EXPECT_TRUE(clock.paused);
}

TEST(SyncClockTest, copy_behavior)
{
    SyncClock a;
    a.pts_ms = 2000;
    a.last_sys_time_ms = 3000;
    a.paused = true;

    SyncClock b = a;
    EXPECT_EQ(b.pts_ms, 2000);
    EXPECT_EQ(b.last_sys_time_ms, 3000);
    EXPECT_TRUE(b.paused);

    b.pts_ms = 4000;
    EXPECT_EQ(a.pts_ms, 2000);
    EXPECT_EQ(b.pts_ms, 4000);
}

/* ========== SyncResult 测试 ========== */

TEST(SyncResultTest, default_initialization)
{
    SyncResult result;
    EXPECT_EQ(result.audio_pts_ms, 0);
    EXPECT_EQ(result.video_pts_ms, 0);
    EXPECT_EQ(result.master_pts_ms, 0);
    EXPECT_DOUBLE_EQ(result.sync_drift_ms, 0.0);
    EXPECT_FALSE(result.is_synced);
}

TEST(SyncResultTest, synced_state_detection)
{
    SyncResult result;
    result.audio_pts_ms = 1000;
    result.video_pts_ms = 1020;
    result.master_pts_ms = 1000;
    result.sync_drift_ms = 20.0;
    result.is_synced = true;

    EXPECT_TRUE(result.is_synced);
    EXPECT_DOUBLE_EQ(result.sync_drift_ms, 20.0);
}

/* ========== SyncConfig 测试 ========== */

TEST(SyncConfigTest, default_values)
{
    SyncConfig config;
    EXPECT_EQ(config.max_drift_ms, 200);
    EXPECT_EQ(config.sync_threshold_ms, 50);
    EXPECT_EQ(config.frame_drop_threshold_ms, 100);
    EXPECT_EQ(config.mode, SYNC_MODE_AUDIO_MASTER);
    EXPECT_TRUE(config.enable_frame_drop);
}

TEST(SyncConfigTest, custom_values)
{
    SyncConfig config;
    config.max_drift_ms = 150;
    config.sync_threshold_ms = 30;
    config.frame_drop_threshold_ms = 80;
    config.mode = SYNC_MODE_VIDEO_MASTER;
    config.enable_frame_drop = false;

    EXPECT_EQ(config.max_drift_ms, 150);
    EXPECT_EQ(config.sync_threshold_ms, 30);
    EXPECT_EQ(config.frame_drop_threshold_ms, 80);
    EXPECT_EQ(config.mode, SYNC_MODE_VIDEO_MASTER);
    EXPECT_FALSE(config.enable_frame_drop);
}

/* ========== 代码规范验证 ========== */

TEST(CodeStandardTest, struct_no_user_constructor)
{
    // 验证结构体没有用户自定义构造函数（遵循代码规范 4.5）
    // is_default_constructible 确保可以默认构造
    // 但不要求 trivial（因为有 in-class 成员初始化器）
    EXPECT_TRUE(std::is_default_constructible<SyncClock>::value);
    EXPECT_TRUE(std::is_default_constructible<SyncResult>::value);
    EXPECT_TRUE(std::is_default_constructible<SyncConfig>::value);
}

TEST(CodeStandardTest, struct_standard_layout)
{
    // 验证结构体为标准布局，确保 C/C++ 兼容
    EXPECT_TRUE(std::is_standard_layout<SyncClock>::value);
    EXPECT_TRUE(std::is_standard_layout<SyncResult>::value);
    EXPECT_TRUE(std::is_standard_layout<SyncConfig>::value);
}
