#include <gtest/gtest.h>

#include "av_synchronizer.h"
#include "av_synchronizer_impl.h"

using namespace Prism::Business;

/* ========== 构造测试 ========== */

TEST(AVSynchronizerTest, construct_with_default_config)
{
    AVSynchronizer sync;
    const auto& cfg = sync.get_config();
    EXPECT_EQ(cfg.max_drift_ms, 200);
    EXPECT_EQ(cfg.sync_threshold_ms, 50);
    EXPECT_EQ(cfg.mode, SYNC_MODE_AUDIO_MASTER);
    EXPECT_TRUE(cfg.enable_frame_drop);
}

TEST(AVSynchronizerTest, construct_with_custom_config)
{
    SyncConfig cfg;
    cfg.max_drift_ms = 100;
    cfg.sync_threshold_ms = 30;
    cfg.mode = SYNC_MODE_VIDEO_MASTER;
    cfg.enable_frame_drop = false;

    AVSynchronizer sync(cfg);
    const auto& stored = sync.get_config();
    EXPECT_EQ(stored.max_drift_ms, 100);
    EXPECT_EQ(stored.sync_threshold_ms, 30);
    EXPECT_EQ(sync.get_mode(), SYNC_MODE_VIDEO_MASTER);
}

TEST(AVSynchronizerTest, initial_master_pts_is_zero)
{
    AVSynchronizer sync;
    EXPECT_EQ(sync.get_master_pts(), 0);
}

/* ========== 时钟更新测试 ========== */

TEST(AVSynchronizerTest, update_audio_clock_sets_master_in_audio_mode)
{
    AVSynchronizer sync;
    sync.update_audio_clock(1000, 5000);
    EXPECT_EQ(sync.get_master_pts(), 1000);
}

TEST(AVSynchronizerTest, update_video_clock_does_not_set_master_in_audio_mode)
{
    AVSynchronizer sync;
    sync.update_audio_clock(1000, 5000);
    sync.update_video_clock(1200, 5000);
    EXPECT_EQ(sync.get_master_pts(), 1000);
}

TEST(AVSynchronizerTest, update_video_clock_sets_master_in_video_mode)
{
    SyncConfig cfg;
    cfg.mode = SYNC_MODE_VIDEO_MASTER;
    AVSynchronizer sync(cfg);

    sync.update_video_clock(2000, 6000);
    EXPECT_EQ(sync.get_master_pts(), 2000);
}

TEST(AVSynchronizerTest, ignore_negative_audio_pts)
{
    AVSynchronizer sync;
    sync.update_audio_clock(1000, 5000);
    sync.update_audio_clock(-1, 6000);
    EXPECT_EQ(sync.get_master_pts(), 1000);
}

TEST(AVSynchronizerTest, ignore_negative_video_pts)
{
    AVSynchronizer sync;
    sync.update_video_clock(1000, 5000);
    sync.update_video_clock(-1, 6000);
    EXPECT_GE(sync.get_master_pts(), 0);
}

TEST(AVSynchronizerTest, ignore_negative_sys_time)
{
    AVSynchronizer sync;
    sync.update_audio_clock(1000, 5000);
    sync.update_audio_clock(2000, -1);
    EXPECT_EQ(sync.get_master_pts(), 1000);
}

TEST(AVSynchronizerTest, sequential_audio_updates)
{
    AVSynchronizer sync;
    sync.update_audio_clock(0, 0);
    sync.update_audio_clock(500, 1000);
    sync.update_audio_clock(1000, 2000);
    sync.update_audio_clock(1500, 3000);
    EXPECT_EQ(sync.get_master_pts(), 1500);
}

/* ========== 同步结果测试 ========== */

TEST(SyncResultTest, synced_when_within_threshold)
{
    SyncConfig cfg;
    cfg.sync_threshold_ms = 50;
    AVSynchronizer sync(cfg);

    sync.update_audio_clock(1000, 5000);
    sync.update_video_clock(1020, 5000);

    SyncResult result = sync.get_sync_result();
    EXPECT_EQ(result.audio_pts_ms, 1000);
    EXPECT_EQ(result.video_pts_ms, 1020);
    EXPECT_TRUE(result.is_synced);
    EXPECT_DOUBLE_EQ(result.sync_drift_ms, 20.0);
}

TEST(SyncResultTest, not_synced_when_exceeding_threshold)
{
    SyncConfig cfg;
    cfg.sync_threshold_ms = 50;
    AVSynchronizer sync(cfg);

    sync.update_audio_clock(1000, 5000);
    sync.update_video_clock(1100, 5000);

    SyncResult result = sync.get_sync_result();
    EXPECT_FALSE(result.is_synced);
    EXPECT_DOUBLE_EQ(result.sync_drift_ms, 100.0);
}

TEST(SyncResultTest, exactly_at_threshold_is_synced)
{
    SyncConfig cfg;
    cfg.sync_threshold_ms = 50;
    AVSynchronizer sync(cfg);

    sync.update_audio_clock(1000, 5000);
    sync.update_video_clock(1050, 5000);

    SyncResult result = sync.get_sync_result();
    EXPECT_TRUE(result.is_synced);
}

TEST(SyncResultTest, master_pts_reflects_audio_in_default_mode)
{
    AVSynchronizer sync;
    sync.update_audio_clock(3000, 7000);

    SyncResult result = sync.get_sync_result();
    EXPECT_EQ(result.master_pts_ms, 3000);
}

TEST(SyncResultTest, zero_initial_state)
{
    AVSynchronizer sync;
    SyncResult result = sync.get_sync_result();
    EXPECT_EQ(result.audio_pts_ms, 0);
    EXPECT_EQ(result.video_pts_ms, 0);
    EXPECT_EQ(result.master_pts_ms, 0);
    EXPECT_TRUE(result.is_synced);
}

/* ========== 视频延迟计算测试 ========== */

TEST(VideoDelayTest, zero_delay_when_synced)
{
    SyncConfig cfg;
    cfg.sync_threshold_ms = 50;
    AVSynchronizer sync(cfg);

    sync.update_audio_clock(1000, 5000);
    int64_t delay = sync.calc_video_delay(1020);
    EXPECT_EQ(delay, 0);
}

TEST(VideoDelayTest, positive_delay_when_video_ahead)
{
    SyncConfig cfg;
    cfg.sync_threshold_ms = 50;
    cfg.max_drift_ms = 200;
    AVSynchronizer sync(cfg);

    sync.update_audio_clock(1000, 5000);
    int64_t delay = sync.calc_video_delay(1150);
    EXPECT_GT(delay, 0);
    EXPECT_LE(delay, cfg.max_drift_ms);
}

TEST(VideoDelayTest, delay_capped_at_max_drift)
{
    SyncConfig cfg;
    cfg.sync_threshold_ms = 50;
    cfg.max_drift_ms = 200;
    AVSynchronizer sync(cfg);

    sync.update_audio_clock(1000, 5000);
    int64_t delay = sync.calc_video_delay(1500);
    EXPECT_LE(delay, cfg.max_drift_ms);
}

TEST(VideoDelayTest, negative_delay_triggers_frame_drop)
{
    SyncConfig cfg;
    cfg.sync_threshold_ms = 50;
    cfg.frame_drop_threshold_ms = 100;
    cfg.enable_frame_drop = true;
    AVSynchronizer sync(cfg);

    sync.update_audio_clock(2000, 5000);
    int64_t delay = sync.calc_video_delay(1500);
    EXPECT_LT(delay, 0);
}

TEST(VideoDelayTest, zero_delay_when_no_audio_reference)
{
    AVSynchronizer sync;
    int64_t delay = sync.calc_video_delay(500);
    EXPECT_EQ(delay, 0);
}

TEST(VideoDelayTest, zero_delay_in_video_master_mode)
{
    SyncConfig cfg;
    cfg.mode = SYNC_MODE_VIDEO_MASTER;
    AVSynchronizer sync(cfg);

    sync.update_audio_clock(1000, 5000);
    sync.update_video_clock(1200, 5000);
    int64_t delay = sync.calc_video_delay(1200);
    EXPECT_EQ(delay, 0);
}

/* ========== 模式切换测试 ========== */

TEST(ModeSwitchTest, switch_to_video_master)
{
    AVSynchronizer sync;
    sync.update_audio_clock(1000, 5000);
    sync.update_video_clock(2000, 5000);

    sync.set_mode(SYNC_MODE_VIDEO_MASTER);
    EXPECT_EQ(sync.get_mode(), SYNC_MODE_VIDEO_MASTER);
    EXPECT_EQ(sync.get_master_pts(), 2000);
}

TEST(ModeSwitchTest, switch_to_external_clock)
{
    AVSynchronizer sync;
    sync.update_audio_clock(1000, 5000);

    sync.set_mode(SYNC_MODE_EXTERNAL_CLOCK);
    EXPECT_EQ(sync.get_mode(), SYNC_MODE_EXTERNAL_CLOCK);
}

TEST(ModeSwitchTest, switch_to_audio_master_noop_when_already_audio)
{
    AVSynchronizer sync;
    sync.update_audio_clock(1000, 5000);

    sync.set_mode(SYNC_MODE_AUDIO_MASTER);
    EXPECT_EQ(sync.get_mode(), SYNC_MODE_AUDIO_MASTER);
    EXPECT_EQ(sync.get_master_pts(), 1000);
}

TEST(ModeSwitchTest, switch_back_to_audio_master)
{
    AVSynchronizer sync;
    sync.update_audio_clock(1000, 5000);
    sync.update_video_clock(2000, 5000);

    sync.set_mode(SYNC_MODE_VIDEO_MASTER);
    EXPECT_EQ(sync.get_master_pts(), 2000);

    sync.set_mode(SYNC_MODE_AUDIO_MASTER);
    EXPECT_EQ(sync.get_mode(), SYNC_MODE_AUDIO_MASTER);
    EXPECT_EQ(sync.get_master_pts(), 1000);
}

/* ========== 暂停/恢复测试 ========== */

TEST(PauseResumeTest, pause_preserves_pts)
{
    AVSynchronizer sync;
    sync.update_audio_clock(1000, 5000);

    sync.pause_clock();
    EXPECT_EQ(sync.get_master_pts(), 1000);
}

TEST(PauseResumeTest, resume_updates_reference_time)
{
    AVSynchronizer sync;
    sync.update_audio_clock(1000, 5000);
    sync.pause_clock();

    sync.resume_clock(8000);
    sync.update_audio_clock(1500, 8500);
    EXPECT_EQ(sync.get_master_pts(), 1500);
}

TEST(PauseResumeTest, pause_idempotent)
{
    AVSynchronizer sync;
    sync.update_audio_clock(1000, 5000);
    sync.pause_clock();
    sync.pause_clock();

    EXPECT_EQ(sync.get_master_pts(), 1000);
}

/* ========== 播放速度测试 ========== */

TEST(PlaySpeedTest, default_speed_is_one)
{
    AVSynchronizer sync;
    EXPECT_DOUBLE_EQ(sync.get_play_speed(), 1.0);
}

TEST(PlaySpeedTest, set_and_get_speed)
{
    AVSynchronizer sync;
    sync.set_play_speed(2.0);
    EXPECT_DOUBLE_EQ(sync.get_play_speed(), 2.0);

    sync.set_play_speed(0.5);
    EXPECT_DOUBLE_EQ(sync.get_play_speed(), 0.5);
}

/* ========== 重置测试 ========== */

TEST(ResetTest, reset_clears_clock_state)
{
    AVSynchronizer sync;
    sync.update_audio_clock(1000, 5000);
    sync.update_video_clock(1200, 5000);
    sync.set_play_speed(2.0);

    sync.reset();

    EXPECT_EQ(sync.get_master_pts(), 0);
    EXPECT_DOUBLE_EQ(sync.get_play_speed(), 1.0);

    SyncResult result = sync.get_sync_result();
    EXPECT_EQ(result.audio_pts_ms, 0);
    EXPECT_EQ(result.video_pts_ms, 0);
}

TEST(ResetTest, reset_preserves_config)
{
    SyncConfig cfg;
    cfg.max_drift_ms = 150;
    AVSynchronizer sync(cfg);

    sync.update_audio_clock(5000, 10000);
    sync.reset();

    EXPECT_EQ(sync.get_config().max_drift_ms, 150);
    EXPECT_EQ(sync.get_mode(), SYNC_MODE_AUDIO_MASTER);
}

/* ========== 综合场景测试 ========== */

TEST(IntegrationTest, full_playback_lifecycle)
{
    AVSynchronizer sync;

    // 1. 开始播放，音频开始输出
    sync.update_audio_clock(0, 0);
    EXPECT_EQ(sync.get_master_pts(), 0);

    // 2. 音频推进，视频跟随
    sync.update_audio_clock(33, 33);
    sync.update_video_clock(33, 33);
    EXPECT_TRUE(sync.get_sync_result().is_synced);

    sync.update_audio_clock(66, 66);
    sync.update_video_clock(66, 66);

    // 3. 视频严重滞后（模拟解码慢，漂移超过 frame_drop_threshold_ms=100）
    sync.update_audio_clock(200, 200);
    int64_t delay = sync.calc_video_delay(80);
    EXPECT_LT(delay, 0);

    // 4. 暂停
    sync.pause_clock();
    int64_t paused_pts = sync.get_master_pts();
    EXPECT_EQ(paused_pts, 200);

    // 5. Seek 到新位置后恢复
    sync.update_audio_clock(5000, 5000);
    sync.update_video_clock(5000, 5000);
    EXPECT_EQ(sync.get_master_pts(), 5000);

    // 6. 倍速播放
    sync.set_play_speed(2.0);
    EXPECT_DOUBLE_EQ(sync.get_play_speed(), 2.0);

    // 7. 播放结束
    sync.reset();
    EXPECT_EQ(sync.get_master_pts(), 0);
}

TEST(IntegrationTest, seek_updates_both_clocks)
{
    AVSynchronizer sync;

    sync.update_audio_clock(1000, 5000);
    sync.update_video_clock(1000, 5000);

    // 模拟 Seek 到 5000ms
    sync.update_audio_clock(5000, 10000);
    sync.update_video_clock(5000, 10000);

    SyncResult result = sync.get_sync_result();
    EXPECT_EQ(result.master_pts_ms, 5000);
    EXPECT_TRUE(result.is_synced);
}

/* ========== 内部辅助函数测试 ========== */

TEST(InternalImplTest, compute_clock_pts_normal)
{
    SyncClock clock;
    clock.pts_ms = 1000;
    clock.last_sys_time_ms = 5000;
    clock.paused = false;

    int64_t now = 5100;
    int64_t result = impl::compute_clock_pts(clock, now, 1.0);
    EXPECT_EQ(result, 1100);
}

TEST(InternalImplTest, compute_clock_pts_with_speed)
{
    SyncClock clock;
    clock.pts_ms = 1000;
    clock.last_sys_time_ms = 5000;
    clock.paused = false;

    int64_t now = 5100;
    int64_t result = impl::compute_clock_pts(clock, now, 2.0);
    EXPECT_EQ(result, 1200);
}

TEST(InternalImplTest, compute_clock_pts_paused)
{
    SyncClock clock;
    clock.pts_ms = 1000;
    clock.last_sys_time_ms = 5000;
    clock.paused = true;

    int64_t now = 10000;
    int64_t result = impl::compute_clock_pts(clock, now, 1.0);
    EXPECT_EQ(result, 1000);
}

TEST(InternalImplTest, compute_clock_pts_no_reference)
{
    SyncClock clock;
    clock.pts_ms = 1000;
    clock.last_sys_time_ms = 0;
    clock.paused = false;

    int64_t now = 5000;
    int64_t result = impl::compute_clock_pts(clock, now, 1.0);
    EXPECT_EQ(result, 1000);
}

TEST(InternalImplTest, compute_clock_pts_zero_speed)
{
    SyncClock clock;
    clock.pts_ms = 1000;
    clock.last_sys_time_ms = 5000;
    clock.paused = false;

    int64_t now = 5100;
    int64_t result = impl::compute_clock_pts(clock, now, 0.0);
    EXPECT_EQ(result, 1000);
}

TEST(InternalImplTest, clamp_value_within_range)
{
    EXPECT_EQ(impl::clamp_value(5, 0, 10), 5);
    EXPECT_EQ(impl::clamp_value(-5, 0, 10), 0);
    EXPECT_EQ(impl::clamp_value(15, 0, 10), 10);
}

TEST(InternalImplTest, should_drop_frame_detection)
{
    EXPECT_FALSE(impl::should_drop_frame(0, 100));
    EXPECT_FALSE(impl::should_drop_frame(-50, 100));
    EXPECT_FALSE(impl::should_drop_frame(50, 100));
    EXPECT_TRUE(impl::should_drop_frame(-150, 100));
}
