#include "support/PlayerHarness.h"

#include <gtest/gtest.h>

#include <memory>

/**
 * @file test_media_session.cpp
 * @brief 媒体会话：open/close 的状态机与引擎生命周期，以及配置对引擎组合的影响。
 */

namespace {

class MediaSessionTest : public ::testing::Test {
  protected:
    void SetUp() override {
        player_ = std::make_unique<Prism::Qa::PlayerGuard>(nullptr, &Prism::Qa::EventLog::callback,
                                                           &events_);
        ASSERT_TRUE(*player_);
        ASSERT_EQ(prism_player_set_audio_factory(player_->get(), &backends_.audio_factory),
                  PRISM_OK);
        ASSERT_EQ(prism_player_set_video_factory(player_->get(), &backends_.video_factory),
                  PRISM_OK);
    }

    void TearDown() override { player_.reset(); }

    Prism::Qa::FakeBackends backends_;
    Prism::Qa::EventLog events_;
    std::unique_ptr<Prism::Qa::PlayerGuard> player_;
};

} // namespace

TEST_F(MediaSessionTest, OpenInitializesEnginesAndFiresMediaLoaded) {
    ASSERT_EQ(prism_player_open(player_->get(), Prism::Qa::FAKE_MEDIA_URI), PRISM_OK);

    EXPECT_EQ(prism_player_get_state(player_->get()), PRISM_STATE_PAUSED);
    EXPECT_TRUE(events_.contains(PRISM_EVENT_MEDIA_LOADED));
    EXPECT_EQ(events_.count(PRISM_EVENT_MEDIA_LOADED), 1U);

    EXPECT_EQ(backends_.audio->create_calls.load(), 1);
    EXPECT_EQ(backends_.audio->init_calls.load(), 1);
    EXPECT_EQ(backends_.video->create_calls.load(), 1);
    EXPECT_EQ(backends_.video->init_calls.load(), 1);
}

TEST_F(MediaSessionTest, CloseReturnsToIdleAndReleasesEngines) {
    ASSERT_EQ(prism_player_open(player_->get(), Prism::Qa::FAKE_MEDIA_URI), PRISM_OK);
    ASSERT_EQ(prism_player_close(player_->get()), PRISM_OK);

    EXPECT_EQ(prism_player_get_state(player_->get()), PRISM_STATE_IDLE);
    EXPECT_EQ(backends_.audio->close_calls.load(), 1);
    EXPECT_EQ(backends_.video->close_calls.load(), 1);

    PrismMediaInfo info{};
    EXPECT_EQ(prism_player_get_media_info(player_->get(), &info), PRISM_ERROR_NO_MEDIA);
}

TEST_F(MediaSessionTest, ReopenAfterCloseRecreatesEngines) {
    ASSERT_EQ(prism_player_open(player_->get(), Prism::Qa::FAKE_MEDIA_URI), PRISM_OK);
    ASSERT_EQ(prism_player_close(player_->get()), PRISM_OK);
    ASSERT_EQ(prism_player_open(player_->get(), Prism::Qa::FAKE_MEDIA_URI), PRISM_OK);

    EXPECT_EQ(backends_.audio->create_calls.load(), 2);
    EXPECT_EQ(backends_.audio->init_calls.load(), 2);
    EXPECT_EQ(backends_.video->create_calls.load(), 2);
    EXPECT_EQ(backends_.video->init_calls.load(), 2);
}

TEST(MediaSessionConfig, DisabledVideoSkipsVideoEngine) {
    PrismConfig config{};
    config.enable_video = false;
    config.enable_audio = true;
    config.default_volume = 1.0F;
    config.log_level = "error";

    Prism::Qa::FakeBackends backends;
    Prism::Qa::PlayerGuard player(&config);
    ASSERT_TRUE(player);
    ASSERT_EQ(prism_player_set_audio_factory(player.get(), &backends.audio_factory), PRISM_OK);
    ASSERT_EQ(prism_player_set_video_factory(player.get(), &backends.video_factory), PRISM_OK);

    ASSERT_EQ(prism_player_open(player.get(), Prism::Qa::FAKE_MEDIA_URI), PRISM_OK);
    EXPECT_EQ(backends.audio->create_calls.load(), 1);
    EXPECT_EQ(backends.video->create_calls.load(), 0);
}

TEST(MediaSessionConfig, OpenWithoutVideoFactoryStillSucceeds) {
    Prism::Qa::FakeBackends backends;
    Prism::Qa::PlayerGuard player;
    ASSERT_TRUE(player);
    ASSERT_EQ(prism_player_set_audio_factory(player.get(), &backends.audio_factory), PRISM_OK);

    EXPECT_EQ(prism_player_open(player.get(), Prism::Qa::FAKE_MEDIA_URI), PRISM_OK);
    EXPECT_EQ(prism_player_get_state(player.get()), PRISM_STATE_PAUSED);
    EXPECT_EQ(backends.video->create_calls.load(), 0);
}
