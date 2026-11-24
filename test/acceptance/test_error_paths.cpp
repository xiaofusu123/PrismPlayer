#include "support/PlayerHarness.h"

#include <gtest/gtest.h>

#include <memory>

/**
 * @file test_error_paths.cpp
 * @brief 错误路径：空句柄、非法参数、未加载媒体、引擎初始化失败的返回码与状态。
 *
 * 这些用例把 ABI 文档里的返回值当契约（#88 AC：错误路径返回码）。
 */

namespace {} // namespace

TEST(PlayerErrorPaths, NullHandleReturnsInvalidHandleOrSentinel) {
    EXPECT_EQ(prism_player_set_audio_factory(nullptr, nullptr), PRISM_ERROR_INVALID_HANDLE);
    EXPECT_EQ(prism_player_set_video_factory(nullptr, nullptr), PRISM_ERROR_INVALID_HANDLE);
    EXPECT_EQ(prism_player_set_network_client(nullptr, nullptr), PRISM_ERROR_INVALID_HANDLE);
    EXPECT_EQ(prism_player_login(nullptr, nullptr), PRISM_ERROR_INVALID_HANDLE);
    EXPECT_EQ(prism_player_logout(nullptr), PRISM_ERROR_INVALID_HANDLE);
    EXPECT_EQ(prism_player_join_room(nullptr, "room"), PRISM_ERROR_INVALID_HANDLE);
    EXPECT_EQ(prism_player_leave_room(nullptr), PRISM_ERROR_INVALID_HANDLE);
    EXPECT_EQ(prism_player_get_room_info(nullptr, nullptr), PRISM_ERROR_INVALID_HANDLE);
    EXPECT_EQ(prism_player_get_room_state(nullptr), PRISM_ROOM_DISCONNECTED);
    EXPECT_EQ(prism_player_open(nullptr, "x"), PRISM_ERROR_INVALID_HANDLE);
    EXPECT_EQ(prism_player_close(nullptr), PRISM_ERROR_INVALID_HANDLE);
    EXPECT_EQ(prism_player_play(nullptr), PRISM_ERROR_INVALID_HANDLE);
    EXPECT_EQ(prism_player_pause(nullptr), PRISM_ERROR_INVALID_HANDLE);
    EXPECT_EQ(prism_player_stop(nullptr), PRISM_ERROR_INVALID_HANDLE);
    EXPECT_EQ(prism_player_seek(nullptr, 0, PRISM_SEEK_ABSOLUTE), PRISM_ERROR_INVALID_HANDLE);
    EXPECT_EQ(prism_player_get_state(nullptr), PRISM_STATE_ERROR);
    EXPECT_EQ(prism_player_get_position(nullptr), -1);
    EXPECT_EQ(prism_player_get_duration(nullptr), -1);
    EXPECT_EQ(prism_player_set_volume(nullptr, 0.5F), PRISM_ERROR_INVALID_HANDLE);
    EXPECT_FLOAT_EQ(prism_player_get_volume(nullptr), 0.0F);
    EXPECT_EQ(prism_player_set_mute(nullptr, true), PRISM_ERROR_INVALID_HANDLE);
    EXPECT_FALSE(prism_player_get_mute(nullptr));
    EXPECT_EQ(prism_player_set_playback_speed(nullptr, 1.5F), PRISM_ERROR_INVALID_HANDLE);
    EXPECT_FLOAT_EQ(prism_player_get_playback_speed(nullptr), 1.0F);
    EXPECT_EQ(prism_player_set_loop(nullptr, true), PRISM_ERROR_INVALID_HANDLE);
    EXPECT_FALSE(prism_player_get_loop(nullptr));
    EXPECT_EQ(prism_player_set_video_window(nullptr, nullptr), PRISM_ERROR_INVALID_HANDLE);
    EXPECT_EQ(prism_player_get_media_info(nullptr, nullptr), PRISM_ERROR_INVALID_HANDLE);
    EXPECT_EQ(prism_player_get_last_error(nullptr), PRISM_ERROR_INVALID_HANDLE);
}

TEST(PlayerErrorPaths, InvalidParametersAreRejected) {
    Prism::Qa::PlayerGuard player;
    ASSERT_TRUE(player);
    PrismPlayerHandle handle = player.get();

    EXPECT_EQ(prism_player_open(handle, nullptr), PRISM_ERROR_INVALID_PARAM);
    EXPECT_EQ(prism_player_login(handle, nullptr), PRISM_ERROR_INVALID_PARAM);

    PrismLoginParams login{};
    EXPECT_EQ(prism_player_login(handle, &login), PRISM_ERROR_INVALID_PARAM);

    EXPECT_EQ(prism_player_join_room(handle, nullptr), PRISM_ERROR_INVALID_PARAM);
    EXPECT_EQ(prism_player_get_room_info(handle, nullptr), PRISM_ERROR_INVALID_PARAM);
    EXPECT_EQ(prism_player_get_media_info(handle, nullptr), PRISM_ERROR_INVALID_PARAM);

    // 句柄有效但工厂/网络实现为空：按现有 ABI 约定返回 INVALID_HANDLE。
    EXPECT_EQ(prism_player_set_audio_factory(handle, nullptr), PRISM_ERROR_INVALID_HANDLE);
    EXPECT_EQ(prism_player_set_video_factory(handle, nullptr), PRISM_ERROR_INVALID_HANDLE);
    EXPECT_EQ(prism_player_set_network_client(handle, nullptr), PRISM_ERROR_INVALID_HANDLE);
}

TEST(PlayerErrorPaths, OperationsWithoutMediaReturnNoMedia) {
    Prism::Qa::PlayerGuard player;
    ASSERT_TRUE(player);

    EXPECT_EQ(prism_player_play(player.get()), PRISM_ERROR_NO_MEDIA);
    EXPECT_EQ(prism_player_get_last_error(player.get()), PRISM_ERROR_NO_MEDIA);
    EXPECT_EQ(prism_player_seek(player.get(), 0, PRISM_SEEK_ABSOLUTE), PRISM_ERROR_NO_MEDIA);
    EXPECT_EQ(prism_player_get_position(player.get()), -1);
    EXPECT_EQ(prism_player_get_duration(player.get()), -1);

    PrismMediaInfo info{};
    EXPECT_EQ(prism_player_get_media_info(player.get(), &info), PRISM_ERROR_NO_MEDIA);
}

TEST(PlayerErrorPaths, NegativeAbsoluteSeekIsRejected) {
    Prism::Qa::FakeBackends backends;
    Prism::Qa::PlayerGuard player;
    ASSERT_TRUE(player);
    ASSERT_EQ(prism_player_set_audio_factory(player.get(), &backends.audio_factory), PRISM_OK);
    ASSERT_EQ(prism_player_set_video_factory(player.get(), &backends.video_factory), PRISM_OK);
    ASSERT_EQ(prism_player_open(player.get(), Prism::Qa::FAKE_MEDIA_URI), PRISM_OK);

    EXPECT_EQ(prism_player_seek(player.get(), -1, PRISM_SEEK_ABSOLUTE), PRISM_ERROR_INVALID_PARAM);
    EXPECT_EQ(prism_player_get_last_error(player.get()), PRISM_ERROR_INVALID_PARAM);
    EXPECT_EQ(backends.audio->seek_calls.load(), 0);
    EXPECT_EQ(backends.video->seek_calls.load(), 0);
}

TEST(PlayerErrorPaths, EngineInitFailureFailsOpenAndSetsErrorState) {
    auto audio_recorder = std::make_shared<Prism::Qa::EngineRecorder>();
    auto video_recorder = std::make_shared<Prism::Qa::EngineRecorder>();
    Prism::Qa::FakeAudioFactory audio_factory(audio_recorder, false);
    Prism::Qa::FakeVideoFactory video_factory(video_recorder, false);
    Prism::Qa::EventLog events;

    Prism::Qa::PlayerGuard player(nullptr, &Prism::Qa::EventLog::callback, &events);
    ASSERT_TRUE(player);
    ASSERT_EQ(prism_player_set_audio_factory(player.get(), &audio_factory), PRISM_OK);
    ASSERT_EQ(prism_player_set_video_factory(player.get(), &video_factory), PRISM_OK);

    EXPECT_EQ(prism_player_open(player.get(), Prism::Qa::FAKE_MEDIA_URI), PRISM_ERROR_OPEN_FAILED);
    EXPECT_EQ(prism_player_get_state(player.get()), PRISM_STATE_ERROR);
    // 只断言“错误被记下来了”，不锁 last_error 的具体码：open 返回 OPEN_FAILED 还是
    // UNKNOWN 是产品侧精度问题，验收骨架不把它固化成契约。
    EXPECT_NE(prism_player_get_last_error(player.get()), PRISM_OK);
    EXPECT_FALSE(events.contains(PRISM_EVENT_MEDIA_LOADED));

    EXPECT_EQ(prism_player_play(player.get()), PRISM_ERROR_NO_MEDIA);
}
