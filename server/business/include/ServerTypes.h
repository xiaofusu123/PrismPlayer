#pragma once

namespace Prism::Server::Business {

// 入站消息 type
inline constexpr const char* MSG_REGISTER = "register";
inline constexpr const char* MSG_LOGIN = "login";
inline constexpr const char* MSG_LOGOUT = "logout";
inline constexpr const char* MSG_CREATE_ROOM = "create_room";
inline constexpr const char* MSG_JOIN_ROOM = "join_room";
inline constexpr const char* MSG_LEAVE_ROOM = "leave_room";
inline constexpr const char* MSG_PLAYBACK = "playback";
inline constexpr const char* MSG_CHAT = "chat";

// 出站消息 type
inline constexpr const char* MSG_ACK = "ack";
inline constexpr const char* MSG_ERROR = "error";
inline constexpr const char* MSG_ROOM_UPDATE = "room_update";

// 播放控制动作（对齐 AsyncAPI 契约 Playback.action）
inline constexpr const char* PLAYBACK_ACTION_PLAY = "play";
inline constexpr const char* PLAYBACK_ACTION_PAUSE = "pause";
inline constexpr const char* PLAYBACK_ACTION_STOP = "stop";
inline constexpr const char* PLAYBACK_ACTION_SEEK = "seek";

// 出站 error.code（HTTP 风格；三个认证错误互不相同）
inline constexpr int ERR_INVALID_CREDENTIALS = 401; /**< 口令错（登录凭据无效） */
inline constexpr int ERR_NOT_AUTHENTICATED = 403;   /**< 未认证（缺 token 或 token 无效） */
inline constexpr int ERR_TOKEN_EXPIRED = 419;       /**< token 已过期 */

} // namespace Prism::Server::Business
