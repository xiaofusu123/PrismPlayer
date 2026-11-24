#include "support/PlayerHarness.h"

#include <gtest/gtest.h>

#include <memory>

/**
 * @file test_playback_control.cpp
 * @brief 播放控制：play/pause/stop 的状态迁移与假引擎收到的调用。
 *
 * 本票只断言“状态 + 调用次数”（#87 决策 9）；位置推进与 presented/dropped/waited
 * 依赖引擎→AV Sync 回调（#55），不在本票范围。
 */

namespace {

class PlaybackControlTest : public ::testing::Test {
  protected:
    void SetUp() override {
        player_ = std::make_unique<Prism::Qa::PlayerGuard>(nullptr, &Prism::Qa::EventLog::callback,
                                                           &events_);
        ASSERT_TRUE(*player_);
        ASSERT_EQ(prism_player_set_audio_factory(player_->get(), &backends_.audio_factory),
                  PRISM_OK);
        ASSERT_EQ(prism_player_set_video_factory(player_->get(), &backends_.video_factory),
                  PRISM_OK);
        ASSERT_EQ(prism_player_open(player_->get(), Prism::Qa::FAKE_MEDIA_URI), PRISM_OK);
    }

    void TearDown() override { player_.reset(); }

    Prism::Qa::FakeBackends backends_;
    Prism::Qa::EventLog events_;
    std::unique_ptr<Prism::Qa::PlayerGuard> player_;
};

} // namespace

TEST_F(PlaybackControlTest, PlayPauseResumeStopTransitions) {
    ASSERT_EQ(prism_player_play(player_->get()), PRISM_OK);
    EXPECT_EQ(prism_player_get_state(player_->get()), PRISM_STATE_PLAYING);
    EXPECT_EQ(backends_.audio->play_calls.load(), 1);
    EXPECT_EQ(backends_.video->play_calls.load(), 1);

    ASSERT_EQ(prism_player_pause(player_->get()), PRISM_OK);
    EXPECT_EQ(prism_player_get_state(player_->get()), PRISM_STATE_PAUSED);
    EXPECT_EQ(backends_.audio->pause_calls.load(), 1);
    EXPECT_EQ(backends_.video->pause_calls.load(), 1);

    ASSERT_EQ(prism_player_play(player_->get()), PRISM_OK);
    EXPECT_EQ(prism_player_get_state(player_->get()), PRISM_STATE_PLAYING);
    EXPECT_EQ(backends_.audio->play_calls.load(), 2);
    EXPECT_EQ(backends_.video->play_calls.load(), 2);

    ASSERT_EQ(prism_player_stop(player_->get()), PRISM_OK);
    EXPECT_EQ(prism_player_get_state(player_->get()), PRISM_STATE_STOPPED);
    // stop = pause + 释放引擎，所以 pause 计数再加一次、close 各一次
    EXPECT_EQ(backends_.audio->pause_calls.load(), 2);
    EXPECT_EQ(backends_.audio->close_calls.load(), 1);
    EXPECT_EQ(backends_.video->close_calls.load(), 1);
}

TEST_F(PlaybackControlTest, PlayWhilePlayingIsIdempotent) {
    ASSERT_EQ(prism_player_play(player_->get()), PRISM_OK);
    ASSERT_EQ(prism_player_play(player_->get()), PRISM_OK);

    EXPECT_EQ(prism_player_get_state(player_->get()), PRISM_STATE_PLAYING);
    EXPECT_EQ(backends_.audio->play_calls.load(), 1);
    EXPECT_EQ(backends_.video->play_calls.load(), 1);
}

TEST_F(PlaybackControlTest, StopThenPlayRecreatesEngines) {
    ASSERT_EQ(prism_player_play(player_->get()), PRISM_OK);
    ASSERT_EQ(prism_player_stop(player_->get()), PRISM_OK);

    ASSERT_EQ(prism_player_play(player_->get()), PRISM_OK);
    EXPECT_EQ(prism_player_get_state(player_->get()), PRISM_STATE_PLAYING);
    EXPECT_EQ(backends_.audio->create_calls.load(), 2);
    EXPECT_EQ(backends_.audio->init_calls.load(), 2);
    EXPECT_EQ(backends_.video->create_calls.load(), 2);
    EXPECT_EQ(backends_.video->init_calls.load(), 2);
}

TEST(PlaybackControlIdle, PauseWithoutMediaIsNoOp) {
    Prism::Qa::FakeBackends backends;
    Prism::Qa::PlayerGuard player;
    ASSERT_TRUE(player);

    EXPECT_EQ(prism_player_pause(player.get()), PRISM_OK);
    EXPECT_EQ(prism_player_get_state(player.get()), PRISM_STATE_IDLE);
    EXPECT_EQ(backends.audio->pause_calls.load(), 0);
}

TEST_F(PlaybackControlTest, SeekDispatchesAbsolutePositionToEngines) {
    ASSERT_EQ(prism_player_seek(player_->get(), 4200, PRISM_SEEK_ABSOLUTE), PRISM_OK);

    EXPECT_EQ(backends_.audio->seek_calls.load(), 1);
    EXPECT_EQ(backends_.audio->last_seek_ms.load(), 4200);
    EXPECT_EQ(backends_.video->seek_calls.load(), 1);
    EXPECT_EQ(backends_.video->last_seek_ms.load(), 4200);
    EXPECT_TRUE(events_.contains(PRISM_EVENT_SEEK_COMPLETED));
}
