#include <gtest/gtest.h>

#include "SyncTypes.h"
#include "ISyncAlgorithm.h"
#include "SyncAlgorithm.h"

using namespace Prism::Business;

class SyncAlgorithmTest : public ::testing::Test {
protected:
    void SetUp() override { algo_ = std::make_unique<SyncAlgorithm>(); }
    void TearDown() override { algo_.reset(); }
    std::unique_ptr<ISyncAlgorithm> algo_;
};

/* ========== Default configuration ========== */

TEST_F(SyncAlgorithmTest, default_drift_is_zero)
{
    auto info = algo_->get_drift_info();
    EXPECT_EQ(info.drift_ms, 0);
    EXPECT_EQ(info.audio_pts, 0);
    EXPECT_EQ(info.video_pts, 0);
}

/* ========== Calibrate: RENDER (within threshold) ========== */

TEST_F(SyncAlgorithmTest, calibrate_render_when_video_slightly_ahead_within_threshold)
{
    SyncAction action = algo_->calibrate(100, 120);
    EXPECT_EQ(action, SyncAction::RENDER);
}

TEST_F(SyncAlgorithmTest, calibrate_render_when_video_slightly_behind_within_threshold)
{
    SyncAction action = algo_->calibrate(100, 80);
    EXPECT_EQ(action, SyncAction::RENDER);
}

TEST_F(SyncAlgorithmTest, calibrate_render_when_perfectly_synced)
{
    SyncAction action = algo_->calibrate(100, 100);
    EXPECT_EQ(action, SyncAction::RENDER);
}

TEST_F(SyncAlgorithmTest, calibrate_render_when_video_exactly_at_ahead_boundary)
{
    SyncAction action = algo_->calibrate(100, 130);
    EXPECT_EQ(action, SyncAction::RENDER);
}

TEST_F(SyncAlgorithmTest, calibrate_render_when_video_exactly_at_behind_boundary)
{
    SyncAction action = algo_->calibrate(100, 50);
    EXPECT_EQ(action, SyncAction::RENDER);
}

/* ========== Calibrate: WAIT (video ahead) ========== */

TEST_F(SyncAlgorithmTest, calibrate_wait_when_video_ahead_beyond_threshold)
{
    SyncAction action = algo_->calibrate(100, 140);
    EXPECT_EQ(action, SyncAction::WAIT);
}

TEST_F(SyncAlgorithmTest, calibrate_wait_with_large_video_lead)
{
    SyncAction action = algo_->calibrate(100, 500);
    EXPECT_EQ(action, SyncAction::WAIT);
}

/* ========== Calibrate: DROP (video behind) ========== */

TEST_F(SyncAlgorithmTest, calibrate_drop_when_video_behind_beyond_threshold)
{
    SyncAction action = algo_->calibrate(100, 40);
    EXPECT_EQ(action, SyncAction::DROP);
}

TEST_F(SyncAlgorithmTest, calibrate_drop_with_large_video_lag)
{
    SyncAction action = algo_->calibrate(500, 100);
    EXPECT_EQ(action, SyncAction::DROP);
}

/* ========== Calibrate: zero PTS returns RENDER ========== */

TEST_F(SyncAlgorithmTest, calibrate_render_when_audio_pts_is_zero)
{
    SyncAction action = algo_->calibrate(0, 100);
    EXPECT_EQ(action, SyncAction::RENDER);
}

TEST_F(SyncAlgorithmTest, calibrate_render_when_video_pts_is_zero)
{
    SyncAction action = algo_->calibrate(100, 0);
    EXPECT_EQ(action, SyncAction::RENDER);
}

TEST_F(SyncAlgorithmTest, calibrate_render_when_both_pts_are_zero)
{
    SyncAction action = algo_->calibrate(0, 0);
    EXPECT_EQ(action, SyncAction::RENDER);
}

/* ========== DriftInfo correctness ========== */

TEST_F(SyncAlgorithmTest, get_drift_info_reflects_last_calibrate)
{
    algo_->calibrate(100, 150);
    auto info = algo_->get_drift_info();
    EXPECT_EQ(info.audio_pts, 100);
    EXPECT_EQ(info.video_pts, 150);
    EXPECT_EQ(info.drift_ms, 50);
}

TEST_F(SyncAlgorithmTest, get_drift_info_negative_drift)
{
    algo_->calibrate(200, 100);
    auto info = algo_->get_drift_info();
    EXPECT_EQ(info.audio_pts, 200);
    EXPECT_EQ(info.video_pts, 100);
    EXPECT_EQ(info.drift_ms, -100);
}

/* ========== Configure ========== */

TEST_F(SyncAlgorithmTest, configure_changes_thresholds)
{
    SyncConfig cfg;
    cfg.ahead_threshold_ms = 100;
    cfg.behind_threshold_ms = 200;
    algo_->configure(cfg);

    // At default thresholds (30/50), PTS 100→300 (drift=200) would WAIT.
    // With new ahead=100, 100→180 (drift=80) is within 100 → RENDER.
    SyncAction action = algo_->calibrate(100, 180);
    EXPECT_EQ(action, SyncAction::RENDER);
}

TEST_F(SyncAlgorithmTest, configure_tighter_thresholds)
{
    SyncConfig cfg;
    cfg.ahead_threshold_ms = 10;
    cfg.behind_threshold_ms = 10;
    algo_->configure(cfg);

    // drift=15 > 10 -> WAIT
    EXPECT_EQ(algo_->calibrate(100, 115), SyncAction::WAIT);
    // drift=-15 < -10 -> DROP
    EXPECT_EQ(algo_->calibrate(100, 85), SyncAction::DROP);
}

/* ========== Reset ========== */

TEST_F(SyncAlgorithmTest, reset_clears_drift_info)
{
    algo_->calibrate(100, 200);
    algo_->reset();

    auto info = algo_->get_drift_info();
    EXPECT_EQ(info.drift_ms, 0);
    EXPECT_EQ(info.audio_pts, 0);
    EXPECT_EQ(info.video_pts, 0);
}
