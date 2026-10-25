#pragma once

#include <cstdint>
#include <string>

namespace Prism::Server::Network {

/**
 * @class IHttpApi
 * @brief HTTP API 抽象接口（接口先行，本阶段不实现）
 *
 * 预留：后续用 asio 实现 REST 接口。本阶段不建实现、不接线、不参与构建。
 */
class IHttpApi {
  public:
    virtual ~IHttpApi() = default;

    /**
     * @brief 启动 HTTP 服务
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
