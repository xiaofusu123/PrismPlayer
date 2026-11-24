#include "support/PlayerHarness.h"

#include <gtest/gtest.h>

#include <cstring>

/**
 * @file test_lifecycle.cpp
 * @brief 句柄生命周期：创建、显式配置、销毁（含“销毁时释放已注入引擎”）。
 */

namespace {} // namespace

TEST(PlayerLifecycle, CreateWithDefaultConfigStartsIdle) {
    Prism::Qa::PlayerGuard player;
    ASSERT_TRUE(player);

    EXPECT_EQ(prism_player_get_state(player.get()), PRISM_STATE_IDLE);
    EXPECT_EQ(prism_player_get_last_error(player.get()), PRISM_OK);
}

TEST(PlayerLifecycle, CreateWithExplicitConfigAppliesVolume) {
    PrismConfig config{};
    config.enable_video = true;
    config.enable_audio = true;
    config.default_volume = 0.25F;
    config.log_level = "error";

    Prism::Qa::PlayerGuard player(&config);
    ASSERT_TRUE(player);

    EXPECT_FLOAT_EQ(prism_player_get_volume(player.get()), 0.25F);
    EXPECT_EQ(prism_player_get_state(player.get()), PRISM_STATE_IDLE);
}

TEST(PlayerLifecycle, VersionStringIsReported) {
    const char* version = prism_player_get_version();
    ASSERT_NE(version, nullptr);
    EXPECT_NE(std::strstr(version, "PrismPlayer"), nullptr);
}

TEST(PlayerLifecycle, DestroyNullHandleIsNoOp) {
    prism_player_destroy(nullptr);
    SUCCEED();
}

TEST(PlayerLifecycle, DestroyReleasesInjectedEngines) {
    Prism::Qa::FakeBackends backends;
    {
        Prism::Qa::PlayerGuard player;
        ASSERT_TRUE(player);
        ASSERT_EQ(prism_player_set_audio_factory(player.get(), &backends.audio_factory), PRISM_OK);
        ASSERT_EQ(prism_player_set_video_factory(player.get(), &backends.video_factory), PRISM_OK);
        ASSERT_EQ(prism_player_open(player.get(), Prism::Qa::FAKE_MEDIA_URI), PRISM_OK);
    }

    EXPECT_EQ(backends.audio->close_calls.load(), 1);
    EXPECT_EQ(backends.video->close_calls.load(), 1);
}
