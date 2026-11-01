#pragma once

#include <cstdint>
#include <string>
typedef unsigned short int uint16_t;

namespace Prism::Server::Network {

/**
 * @class IHttpApi
 * @brief 服务端与客户端之间的 HTTP 连接（REST API：登录、房间列表等）
 *
 * 预留：后续用 asio 实现 REST 接口。本阶段不建实现、不接线、不参与构建。
 */
class IHttpApi {
  public:
    virtual ~IHttpApi() = default;

    /**
     * @brief 启动 HTTP 服务，监听客户端连接
     * @param host 监听地址
     * @param port 监听端口
     * @return 成功返回 true
     */
    virtual bool start(const std::string& host, uint16_t port) = 0;

    /**
     * @brief 停止 HTTP 服务
     */
    virtual void stop() = 0;
};

} // namespace Prism::Server::Network
