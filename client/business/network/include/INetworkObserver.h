#pragma once

#include "NetworkTypes.h"

namespace Prism::Business {

/**
 * @class INetworkObserver
 * @brief 网络事件观察者抽象接口
 *
 * 上层（Service）实现此接口以接收网络层的异步事件回调。
 * 各网络子模块（AccountManager / RoomManager / SignalingClient）
 * 通过此接口向上通知事件。
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

} // namespace Prism::Business
