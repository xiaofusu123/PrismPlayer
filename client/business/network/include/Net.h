#pragma once

#include <cstdint>
#include <memory>

namespace Prism::Business {

/* ========================================================================
 *  枚举类型
 * ======================================================================== */

/**
 * @enum NetworkState
 * @brief 网络连接状态
 *
 * - DISCONNECTED: 未连接
 * 
 * - CONNECTING: 连接中
 * 
 * - CONNECTED: 已连接
 * 
 * - ERROR: 连接异常
 */
enum class NetworkState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    ERROR
};

/**
 * @enum LoginResult
 * @brief 登录操作结果
 *
 * - SUCCESS: 登录成功
 * 
 * - INVALID_CREDENTIALS: 用户名或密码错误
 * 
 * - NETWORK_ERROR: 网络连接失败
 * 
 * - SERVER_ERROR: 服务器内部错误
 * 
 * - TIMEOUT: 请求超时
 */
enum class LoginResult {
    SUCCESS,
    INVALID_CREDENTIALS,
    NETWORK_ERROR,
    SERVER_ERROR,
    TIMEOUT
};

/**
 * @enum PlaybackCommand
 * @brief 联机播放控制指令类型
 *
 * - PLAY: 播放
 * 
 * - PAUSE: 暂停
 * 
 * - SEEK: 跳转
 * 
 * - SPEED: 变速
 */
enum class PlaybackCommand {
    PLAY,
    PAUSE,
    SEEK,
    SPEED
};

/* ========================================================================
 *  结构体类型
 * ======================================================================== */

/**
 * @struct RoomInfo
 * @brief 房间信息
 */
struct RoomInfo {
    char room_id[256]{};   /**< 房间 ID */
    int member_count{0};   /**< 房间在线人数 */
};

/* ========================================================================
 *  观察者接口
 * ======================================================================== */

/**
 * @class INetworkObserver
 * @brief 网络事件观察者抽象接口
 *
 * 上层（Service）实现此接口以接收网络层的异步事件回调。
 * 通过 INetworkClient::set_observer() 注册。
 */
class INetworkObserver {
public:
    virtual ~INetworkObserver() = default;

    /**
     * @brief 登录结果回调
     * @param result 登录结果
     */
    virtual void on_login_result(LoginResult result) = 0;

    /**
     * @brief 成功加入房间回调
     * @param info 房间信息
     */
    virtual void on_room_joined(const RoomInfo& info) = 0;

    /**
     * @brief 离开房间回调
     */
    virtual void on_room_left() = 0;

    /**
     * @brief 服务器推送消息回调
     * @param message 消息内容（JSON 字符串）
     */
    virtual void on_room_message(const char* message) = 0;

    /**
     * @brief 网络错误回调
     * @param error_code 错误码
     * @param error_msg 错误描述
     */
    virtual void on_network_error(int error_code, const char* error_msg) = 0;
};

/* ========================================================================
 *  网络客户端门面接口
 * ======================================================================== */

/**
 * @class INetworkClient
 * @brief 网络模块对外统一接口
 *
 * 封装账号认证、房间管理、信令通信等全部网络功能。
 * 所有网络操作均为异步，结果通过 INetworkObserver 回调通知。
 */
class INetworkClient {
public:
    virtual ~INetworkClient() = default;

    /* ---------- 账号认证 ---------- */

    /**
     * @brief 发起异步登录请求
     * @param username 用户名
     * @param password 密码
     * @param server_url 服务器地址
     * @return true 请求已发出，结果通过 on_login_result 回调
     */
    virtual bool login(const char* username, const char* password,
                       const char* server_url) = 0;

    /**
     * @brief 登出并断开与服务器的连接
     * @return true 操作成功
     */
    virtual bool logout() = 0;

    /**
     * @brief 查询当前登录状态
     * @return true 已登录
     */
    virtual bool is_logged_in() const = 0;

    /* ---------- 房间管理 ---------- */

    /**
     * @brief 加入指定房间
     * @param room_id 房间 ID
     * @return true 请求已发出，结果通过 on_room_joined 回调
     */
    virtual bool join_room(const char* room_id) = 0;

    /**
     * @brief 离开当前房间
     * @return true 操作成功，结果通过 on_room_left 回调
     */
    virtual bool leave_room() = 0;

    /**
     * @brief 获取当前房间信息
     * @param info [out] 房间信息输出参数
     * @return true 获取成功
     */
    virtual bool get_room_info(RoomInfo* info) const = 0;

    /**
     * @brief 获取当前房间连接状态
     * @return NetworkState 当前状态
     */
    virtual NetworkState get_room_state() const = 0;

    /* ---------- 信令通信 ---------- */

    /**
     * @brief 向服务器发送消息
     * @param message 消息内容
     * @return true 发送成功
     */
    virtual bool send_message(const char* message) = 0;

    /* ---------- 观察者管理 ---------- */

    /**
     * @brief 注册事件观察者
     * @param observer 观察者指针（不持有所有权）
     */
    virtual void set_observer(INetworkObserver* observer) = 0;
};

/* ========================================================================
 *  工厂函数
 * ======================================================================== */

/**
 * @brief 创建网络客户端实例
 * @return INetworkClient 实例
 */
std::unique_ptr<INetworkClient> create_network_client();

} // namespace Prism::Business
