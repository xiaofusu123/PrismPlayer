#include <gtest/gtest.h>

#include <atomic>

#include "SyncTypes.h"
#include "IPlaybackStateMachine.h"
#include "ISyncAlgorithm.h"
#include "IEngineObserver.h"

#include "PlaybackStateMachine.h"
#include "SyncAlgorithm.h"
#include "EngineObserver.h"

#include "AudioEngine.h"
#include "VideoEngine.h"

using namespace Prism::Business;

class EngineObserverTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        sm_       = std::make_unique<PlaybackStateMachine>();
        algo_     = std::make_unique<SyncAlgorithm>();
        observer_ = std::make_unique<EngineObserver>();

        observer_->set_state_machine(sm_.get());
        observer_->set_sync_algorithm(algo_.get());
    }

    void TearDown() override
    {
        observer_.reset();
        algo_.reset();
        sm_.reset();
    }

    Prism::Engine::AudioSyncInfo audio_info_;
    Prism::Engine::VideoSyncInfo video_info_;

    void set_audio_pts(uint64_t pts) { audio_info_.current_pts.store(pts); }
    void set_video_pts(uint64_t pts) { video_info_.current_pts.store(pts); }

    std::unique_ptr<IPlaybackStateMachine> sm_;
    std::unique_ptr<ISyncAlgorithm>      algo_;
    std::unique_ptr<IEngineObserver>     observer_;
};

/* ========== on_audio_frame ========== */

TEST_F(EngineObserverTest, on_audio_frame_transitions_uninit_to_calibrating)
{
    EXPECT_EQ(sm_->get_state(), SyncState::UNINIT);

    set_audio_pts(100);
    observer_->on_audio_frame(audio_info_);

    EXPECT_EQ(sm_->get_state(), SyncState::CALIBATING);
}

TEST_F(EngineObserverTest, on_audio_frame_does_not_retrigger_from_calibrating)
{
    set_audio_pts(100);
    observer_->on_audio_frame(audio_info_);
    EXPECT_EQ(sm_->get_state(), SyncState::CALIBATING);

    set_audio_pts(200);
    observer_->on_audio_frame(audio_info_);
    // Already CALIBATING, should stay CALIBATING
    EXPECT_EQ(sm_->get_state(), SyncState::CALIBATING);
}

/* ========== on_video_frame ========== */

TEST_F(EngineObserverTest, on_video_frame_calibrates_and_transitions_to_synchronized)
{
    // Set up: move to CALIBATING first
    set_audio_pts(100);
    observer_->on_audio_frame(audio_info_);

    // Video at similar PTS -> RENDER -> SYNCHRONIZED
    set_video_pts(120);
    observer_->on_video_frame(video_info_);

    EXPECT_EQ(sm_->get_state(), SyncState::SYNCHRONIZED);
}

TEST_F(EngineObserverTest, on_video_frame_transitions_to_ahead_when_video_leads)
{
    set_audio_pts(100);
    observer_->on_audio_frame(audio_info_);
    EXPECT_EQ(sm_->get_state(), SyncState::CALIBATING);

    // First video frame: drift = 400 > threshold -> WAIT, but state machine
    // requires CALIBATING -> SYNCHRONIZED first before reaching AHEAD
    set_video_pts(500);
    observer_->on_video_frame(video_info_);
    EXPECT_EQ(sm_->get_state(), SyncState::SYNCHRONIZED);

    // Second video frame: still ahead -> AHEAD
    set_video_pts(600);
    observer_->on_video_frame(video_info_);
    EXPECT_EQ(sm_->get_state(), SyncState::AHEAD);
}

TEST_F(EngineObserverTest, on_video_frame_transitions_to_behind_when_video_lags)
{
    set_audio_pts(500);
    observer_->on_audio_frame(audio_info_);
    EXPECT_EQ(sm_->get_state(), SyncState::CALIBATING);

    // First video frame: drift = -400 -> DROP, but CALIBATING -> SYNCHRONIZED first
    set_video_pts(100);
    observer_->on_video_frame(video_info_);
    EXPECT_EQ(sm_->get_state(), SyncState::SYNCHRONIZED);

    // Second video frame: still behind -> BEHIND
    set_video_pts(50);
    observer_->on_video_frame(video_info_);
    EXPECT_EQ(sm_->get_state(), SyncState::BEHIND);
}

TEST_F(EngineObserverTest, on_video_frame_ignored_when_uninit)
{
    // No audio frame received yet -- state is UNINIT
    // on_video_frame checks: s == CALIBATING || s == SYNCHRONIZED || s == AHEAD || s == BEHIND
    // UNINIT matches none -> skip
    set_video_pts(100);
    observer_->on_video_frame(video_info_);

    EXPECT_EQ(sm_->get_state(), SyncState::UNINIT);
}

/* ========== Full sync cycle ========== */

TEST_F(EngineObserverTest, full_sync_cycle_uninit_to_synchronized)
{
    // 1. Audio frame arrives -> UNINIT -> CALIBATING
    set_audio_pts(1000);
    observer_->on_audio_frame(audio_info_);
    EXPECT_EQ(sm_->get_state(), SyncState::CALIBATING);

    // 2. Video frame within sync threshold -> CALIBATING -> SYNCHRONIZED
    set_video_pts(1020);
    observer_->on_video_frame(video_info_);
    EXPECT_EQ(sm_->get_state(), SyncState::SYNCHRONIZED);
}

TEST_F(EngineObserverTest, sync_cycle_recovery_from_ahead)
{
    // Phase 1: reach SYNCHRONIZED
    set_audio_pts(1000);
    observer_->on_audio_frame(audio_info_);
    EXPECT_EQ(sm_->get_state(), SyncState::CALIBATING);

    set_video_pts(1020);
    observer_->on_video_frame(video_info_);
    EXPECT_EQ(sm_->get_state(), SyncState::SYNCHRONIZED);

    // Phase 2: drift into AHEAD
    set_video_pts(2000);
    observer_->on_video_frame(video_info_);
    EXPECT_EQ(sm_->get_state(), SyncState::AHEAD);

    // Phase 3: audio catches up, video now within range -> SYNCHRONIZED
    set_audio_pts(1980);
    observer_->on_audio_frame(audio_info_);
    set_video_pts(2000);
    observer_->on_video_frame(video_info_);
    EXPECT_EQ(sm_->get_state(), SyncState::SYNCHRONIZED);
}

/* ========== Wiring validation ========== */

TEST_F(EngineObserverTest, null_sync_algorithm_does_not_crash_on_video_frame)
{
    observer_->set_sync_algorithm(nullptr);
    set_audio_pts(100);
    observer_->on_audio_frame(audio_info_);

    // on_video_frame checks sync_algo_ -- if null, skips calibration
    EXPECT_NO_FATAL_FAILURE(observer_->on_video_frame(video_info_));
}

TEST_F(EngineObserverTest, null_state_machine_does_not_crash_on_audio_frame)
{
    observer_->set_state_machine(nullptr);
    EXPECT_NO_FATAL_FAILURE(observer_->on_audio_frame(audio_info_));
}

/* ========== on_render_ready ========== */

TEST_F(EngineObserverTest, on_render_ready_does_not_crash)
{
    Prism::Engine::RenderMetadata metadata{};
    metadata.width     = 1920;
    metadata.height    = 1080;
    metadata.timestamp = 1000;

    EXPECT_NO_FATAL_FAILURE(observer_->on_render_ready(metadata));
}
