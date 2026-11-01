#pragma once

#include <cstdint>
#include <string>

namespace Prism::Server::Business {
class IAuthService;
class IRoomManager;
class IMessageRouter;
class IClientNotifier;
} // namespace Prism::Server::Business

namespace Prism::Server::Network {

/**
 * @class ISignalingServer
 * @brief WebSocket 信令服务端抽象接口
 */
class ISignalingServer {
  public:
    virtual ~ISignalingServer() = default;

    /**
     * @brief 暴露内部客户端推送器
     * @return IClientNotifier 指针（不转移所有权）
     */
    virtual Business::IClientNotifier* notifier() = 0;

    /**
     * @brief 注入业务接口，接收入站消息路由
     * @param auth 认证服务
     * @param rooms 房间管理
     * @param router 消息路由
     */
    virtual void set_handlers(Business::IAuthService* auth, Business::IRoomManager* rooms,
                              Business::IMessageRouter* router) = 0;

    /**
     * @brief 启动服务（非阻塞，内部起线程）
     * @param host 监听地址
     * @param port 监听端口
     * @return 成功返回 true
     */
    virtual bool start(const std::string& host, uint16_t port) = 0;

    /**
     * @brief 停止服务
     */
    virtual void stop() = 0;

    /**
     * @brief 阻塞等待服务线程结束
     */
    virtual void wait() = 0;
};

} // namespace Prism::Server::Network
