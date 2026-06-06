#pragma once

namespace Prism::Business {

/* ========================================================================
 *  网络连接状态
 * ======================================================================== */

/**
 * @enum NetworkState
 * @brief 网络连接状态
 *
 * - DISCONNECTED: 未连接
 * - CONNECTING: 连接中
 * - CONNECTED: 已连接
 * - ERROR: 连接异常
 */
enum class NetworkState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    ERROR
};

/* ========================================================================
 *  登录结果
 * ======================================================================== */

/**
 * @enum LoginResult
 * @brief 登录操作结果
 *
 * - SUCCESS: 登录成功
 * - INVALID_CREDENTIALS: 用户名或密码错误
 * - NETWORK_ERROR: 网络连接失败
 * - SERVER_ERROR: 服务器内部错误
 * - TIMEOUT: 请求超时
 */
enum class LoginResult {
    SUCCESS,
    INVALID_CREDENTIALS,
    NETWORK_ERROR,
    SERVER_ERROR,
    TIMEOUT
};

/* ========================================================================
 *  播放控制指令
 * ======================================================================== */

/**
 * @enum PlaybackCommand
 * @brief 联机播放控制指令类型
 */
enum class PlaybackCommand {
    PLAY,   /**< 播放 */
    PAUSE,  /**< 暂停 */
    SEEK,   /**< 跳转 */
    SPEED   /**< 变速 */
};

/* ========================================================================
 *  房间信息
 * ======================================================================== */

/**
 * @struct RoomInfo
 * @brief 房间信息
 */
struct RoomInfo {
    char room_id[256]{};   /**< 房间 ID */
    int member_count{0};   /**< 房间在线人数 */
};

} // namespace Prism::Business
