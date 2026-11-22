#pragma once

namespace Prism::Server::Business {

// 入站消息 type
inline constexpr const char* MSG_REGISTER = "register";
inline constexpr const char* MSG_LOGIN = "login";
inline constexpr const char* MSG_CREATE_ROOM = "create_room";
inline constexpr const char* MSG_JOIN_ROOM = "join_room";
inline constexpr const char* MSG_LEAVE_ROOM = "leave_room";
inline constexpr const char* MSG_PLAYBACK = "playback";
inline constexpr const char* MSG_CHAT = "chat";

// 出站消息 type
inline constexpr const char* MSG_ACK = "ack";
inline constexpr const char* MSG_ERROR = "error";
inline constexpr const char* MSG_ROOM_UPDATE = "room_update";

} // namespace Prism::Server::Business
