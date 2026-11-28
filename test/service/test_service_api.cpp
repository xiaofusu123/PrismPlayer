#include <gtest/gtest.h>

#include "Player.h"

#include <vector>

namespace {

/** 事件记录器：把回调收到的事件按顺序收集起来。 */
struct EventLog {
    static void on_event(PrismEventType type, const void*, void* user_data) {
        auto* log = static_cast<EventLog*>(user_data);
        log->events.push_back(type);
    }
    std::vector<PrismEventType> events;
};

PrismLoginParams make_login_params() {
    PrismLoginParams params{};
    params.username = "alice";
    params.password = "secret";
    params.server_url = "ws://localhost:8080";
    return params;
}

/* ==================== 句柄与入参校验 ==================== */

TEST(PlayerHandle, NullHandleReturnsError) {
    PrismLoginParams params = make_login_params();

    EXPECT_EQ(prism_player_login(nullptr, &params), PRISM_ERROR_INVALID_HANDLE);
    EXPECT_EQ(prism_player_join_room(nullptr, "room-1"), PRISM_ERROR_INVALID_HANDLE);
    EXPECT_EQ(prism_player_open(nullptr, "fake://media"), PRISM_ERROR_INVALID_HANDLE);
    EXPECT_EQ(prism_player_play(nullptr), PRISM_ERROR_INVALID_HANDLE);
    EXPECT_EQ(prism_player_pause(nullptr), PRISM_ERROR_INVALID_HANDLE);
    EXPECT_EQ(prism_player_seek(nullptr, 0, PRISM_SEEK_ABSOLUTE), PRISM_ERROR_INVALID_HANDLE);
    EXPECT_EQ(prism_player_set_volume(nullptr, 1.0f), PRISM_ERROR_INVALID_HANDLE);
    EXPECT_EQ(prism_player_set_audio_factory(nullptr, nullptr), PRISM_ERROR_INVALID_HANDLE);
    EXPECT_EQ(prism_player_get_state(nullptr), PRISM_STATE_ERROR);
    EXPECT_EQ(prism_player_get_position(nullptr), -1);
    EXPECT_EQ(prism_player_get_volume(nullptr), 0.0f);
    EXPECT_EQ(prism_player_get_last_error(nullptr), PRISM_ERROR_INVALID_HANDLE);
    prism_player_destroy(nullptr);
}

TEST(PlayerHandle, InvalidParamDistinguishedFromInvalidHandle) {
    PrismPlayerHandle player = prism_player_create(nullptr, nullptr, nullptr);
    ASSERT_NE(player, nullptr);

    EXPECT_EQ(prism_player_set_audio_factory(player, nullptr), PRISM_ERROR_INVALID_HANDLE);
    EXPECT_EQ(prism_player_set_video_factory(player, nullptr), PRISM_ERROR_INVALID_HANDLE);
    EXPECT_EQ(prism_player_set_network_client(player, nullptr), PRISM_ERROR_INVALID_HANDLE);
    EXPECT_EQ(prism_player_login(player, nullptr), PRISM_ERROR_INVALID_PARAM);
    EXPECT_EQ(prism_player_join_room(player, nullptr), PRISM_ERROR_INVALID_PARAM);

    prism_player_destroy(player);
}

TEST(PlayerHandle, CreateWithNullConfigUsesDefaults) {
    PrismPlayerHandle player = prism_player_create(nullptr, nullptr, nullptr);
    ASSERT_NE(player, nullptr);

    EXPECT_EQ(prism_player_get_state(player), PRISM_STATE_IDLE);
    EXPECT_FLOAT_EQ(prism_player_get_volume(player), 1.0f);
    EXPECT_FLOAT_EQ(prism_player_get_playback_speed(player), 1.0f);
    EXPECT_FALSE(prism_player_get_mute(player));
    EXPECT_FALSE(prism_player_get_loop(player));

    prism_player_destroy(player);
}

/* ==================== 状态流转 ==================== */

TEST(PlayerState, OpenPlayPauseStopFlow) {
    PrismPlayerHandle player = prism_player_create(nullptr, nullptr, nullptr);
    ASSERT_NE(player, nullptr);

    EXPECT_EQ(prism_player_get_state(player), PRISM_STATE_IDLE);

    ASSERT_EQ(prism_player_open(player, "fake://media"), PRISM_OK);
    EXPECT_NE(prism_player_get_state(player), PRISM_STATE_IDLE);

    ASSERT_EQ(prism_player_play(player), PRISM_OK);
    EXPECT_EQ(prism_player_get_state(player), PRISM_STATE_PLAYING);

    ASSERT_EQ(prism_player_pause(player), PRISM_OK);
    EXPECT_EQ(prism_player_get_state(player), PRISM_STATE_PAUSED);

    ASSERT_EQ(prism_player_stop(player), PRISM_OK);
    EXPECT_EQ(prism_player_get_state(player), PRISM_STATE_STOPPED);

    prism_player_destroy(player);
}

TEST(PlayerState, PlayWithoutMediaFails) {
    PrismPlayerHandle player = prism_player_create(nullptr, nullptr, nullptr);
    ASSERT_NE(player, nullptr);

    EXPECT_EQ(prism_player_play(player), PRISM_ERROR_NO_MEDIA);
    EXPECT_EQ(prism_player_get_last_error(player), PRISM_ERROR_NO_MEDIA);

    prism_player_destroy(player);
}

/* ==================== 音量 / 静音 / 倍速 / 循环 ==================== */

TEST(PlayerVolume, VolumeClampsToRange) {
    PrismPlayerHandle player = prism_player_create(nullptr, nullptr, nullptr);
    ASSERT_NE(player, nullptr);

    EXPECT_EQ(prism_player_set_volume(player, 2.0f), PRISM_OK);
    EXPECT_FLOAT_EQ(prism_player_get_volume(player), 1.0f);

    EXPECT_EQ(prism_player_set_volume(player, -1.0f), PRISM_OK);
    EXPECT_FLOAT_EQ(prism_player_get_volume(player), 0.0f);

    EXPECT_EQ(prism_player_set_volume(player, 0.5f), PRISM_OK);
    EXPECT_FLOAT_EQ(prism_player_get_volume(player), 0.5f);

    prism_player_destroy(player);
}

TEST(PlayerMute, MuteAndUnmuteRestoreVolume) {
    PrismPlayerHandle player = prism_player_create(nullptr, nullptr, nullptr);
    ASSERT_NE(player, nullptr);

    ASSERT_EQ(prism_player_set_volume(player, 0.75f), PRISM_OK);
    ASSERT_EQ(prism_player_set_mute(player, true), PRISM_OK);
    EXPECT_TRUE(prism_player_get_mute(player));

    ASSERT_EQ(prism_player_set_mute(player, false), PRISM_OK);
    EXPECT_FALSE(prism_player_get_mute(player));
    EXPECT_FLOAT_EQ(prism_player_get_volume(player), 0.75f);

    prism_player_destroy(player);
}

TEST(PlayerPlayback, SpeedAndLoopRoundtrip) {
    PrismPlayerHandle player = prism_player_create(nullptr, nullptr, nullptr);
    ASSERT_NE(player, nullptr);

    EXPECT_EQ(prism_player_set_playback_speed(player, 3.0f), PRISM_OK);
    EXPECT_FLOAT_EQ(prism_player_get_playback_speed(player), 2.0f);

    EXPECT_EQ(prism_player_set_playback_speed(player, 0.1f), PRISM_OK);
    EXPECT_FLOAT_EQ(prism_player_get_playback_speed(player), 0.5f);

    EXPECT_EQ(prism_player_set_loop(player, true), PRISM_OK);
    EXPECT_TRUE(prism_player_get_loop(player));
    EXPECT_EQ(prism_player_set_loop(player, false), PRISM_OK);
    EXPECT_FALSE(prism_player_get_loop(player));

    prism_player_destroy(player);
}

/* ==================== Seek ==================== */

TEST(PlayerSeek, SeekWithoutMediaFails) {
    PrismPlayerHandle player = prism_player_create(nullptr, nullptr, nullptr);
    ASSERT_NE(player, nullptr);

    EXPECT_EQ(prism_player_seek(player, 1000, PRISM_SEEK_ABSOLUTE), PRISM_ERROR_NO_MEDIA);
    EXPECT_EQ(prism_player_get_last_error(player), PRISM_ERROR_NO_MEDIA);

    prism_player_destroy(player);
}

TEST(PlayerSeek, AbsoluteNegativeFails) {
    PrismPlayerHandle player = prism_player_create(nullptr, nullptr, nullptr);
    ASSERT_NE(player, nullptr);
    ASSERT_EQ(prism_player_open(player, "fake://media"), PRISM_OK);

    EXPECT_EQ(prism_player_seek(player, -100, PRISM_SEEK_ABSOLUTE), PRISM_ERROR_INVALID_PARAM);
    EXPECT_EQ(prism_player_get_last_error(player), PRISM_ERROR_INVALID_PARAM);

    prism_player_destroy(player);
}

/* ==================== 网络桩行为 ==================== */

TEST(PlayerNetwork, LoginEmitsSuccessEvent) {
    EventLog log;
    PrismPlayerHandle player = prism_player_create(nullptr, EventLog::on_event, &log);
    ASSERT_NE(player, nullptr);

    PrismLoginParams params = make_login_params();
    EXPECT_EQ(prism_player_login(player, &params), PRISM_OK);

    ASSERT_EQ(log.events.size(), 1U);
    EXPECT_EQ(log.events[0], PRISM_EVENT_LOGIN_SUCCESS);

    prism_player_destroy(player);
}

TEST(PlayerNetwork, JoinRoomRequiresLogin) {
    PrismPlayerHandle player = prism_player_create(nullptr, nullptr, nullptr);
    ASSERT_NE(player, nullptr);

    EXPECT_EQ(prism_player_join_room(player, "room-1"), PRISM_ERROR_NETWORK);
    EXPECT_EQ(prism_player_get_last_error(player), PRISM_ERROR_NETWORK);
    EXPECT_EQ(prism_player_get_room_state(player), PRISM_ROOM_DISCONNECTED);

    prism_player_destroy(player);
}

TEST(PlayerNetwork, JoinLeaveRoomRoundtrip) {
    EventLog log;
    PrismPlayerHandle player = prism_player_create(nullptr, EventLog::on_event, &log);
    ASSERT_NE(player, nullptr);

    PrismLoginParams params = make_login_params();
    ASSERT_EQ(prism_player_login(player, &params), PRISM_OK);

    EXPECT_EQ(prism_player_join_room(player, "room-1"), PRISM_OK);
    EXPECT_EQ(prism_player_get_room_state(player), PRISM_ROOM_CONNECTED);

    PrismRoomInfo info{};
    ASSERT_EQ(prism_player_get_room_info(player, &info), PRISM_OK);
    EXPECT_STREQ(info.room_id, "room-1");

    EXPECT_EQ(prism_player_leave_room(player), PRISM_OK);
    EXPECT_EQ(prism_player_get_room_state(player), PRISM_ROOM_DISCONNECTED);

    ASSERT_EQ(log.events.size(), 3U);
    EXPECT_EQ(log.events[0], PRISM_EVENT_LOGIN_SUCCESS);
    EXPECT_EQ(log.events[1], PRISM_EVENT_ROOM_JOINED);
    EXPECT_EQ(log.events[2], PRISM_EVENT_ROOM_LEFT);

    prism_player_destroy(player);
}

TEST(PlayerNetwork, LogoutAutoLeavesRoom) {
    EventLog log;
    PrismPlayerHandle player = prism_player_create(nullptr, EventLog::on_event, &log);
    ASSERT_NE(player, nullptr);

    PrismLoginParams params = make_login_params();
    ASSERT_EQ(prism_player_login(player, &params), PRISM_OK);
    ASSERT_EQ(prism_player_join_room(player, "room-1"), PRISM_OK);
    ASSERT_EQ(prism_player_get_room_state(player), PRISM_ROOM_CONNECTED);

    EXPECT_EQ(prism_player_logout(player), PRISM_OK);
    EXPECT_EQ(prism_player_get_room_state(player), PRISM_ROOM_DISCONNECTED);

    ASSERT_EQ(log.events.size(), 3U);
    EXPECT_EQ(log.events[2], PRISM_EVENT_ROOM_LEFT);

    prism_player_destroy(player);
}

/* ==================== 媒体信息与版本 ==================== */

TEST(PlayerMediaInfo, GetMediaInfoWithoutMediaFails) {
    PrismPlayerHandle player = prism_player_create(nullptr, nullptr, nullptr);
    ASSERT_NE(player, nullptr);

    PrismMediaInfo info{};
    EXPECT_EQ(prism_player_get_media_info(player, &info), PRISM_ERROR_NO_MEDIA);
    EXPECT_EQ(prism_player_get_last_error(player), PRISM_ERROR_NO_MEDIA);

    prism_player_destroy(player);
}

TEST(PlayerVersion, VersionStringMatchesMacros) {
    const char* version = prism_player_get_version();
    ASSERT_NE(version, nullptr);
    EXPECT_STREQ(version, "PrismPlayer 0.0.1");
}

} // namespace
