#pragma once

#include "INetworkObserver.h"

namespace Prism::Business {

/**
 * @class ISignalingClient
 * @brief WebSocket 信令客户端抽象接口
 *
 * 负责与服务器的实时信令通信，包括播放控制同步消息的收发。
 */
class ISignalingClient {
public:
    virtual ~ISignalingClient() = default;

    /**
     * @brief 连接到信令服务器
     * @param url WebSocket 服务器地址
     * @return true 连接成功
     */
    virtual bool connect(const char* url) = 0;

    /**
     * @brief 断开与信令服务器的连接
     */
    virtual void disconnect() = 0;

    /**
     * @brief 向服务器发送消息
     * @param message 消息内容
     * @return true 发送成功
     */
    virtual bool send_message(const char* message) = 0;

    /**
     * @brief 获取当前连接状态
     * @return NetworkState 当前状态
     */
    virtual NetworkState get_state() const = 0;

    /**
     * @brief 注册事件观察者
     * @param observer 观察者指针（不持有所有权）
     */
    virtual void set_observer(INetworkObserver* observer) = 0;
};

} // namespace Prism::Business
