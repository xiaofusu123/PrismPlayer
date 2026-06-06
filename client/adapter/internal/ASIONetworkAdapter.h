#pragma once

#include "NetworkAdapter.h"

namespace Prism::Adapter {

/**
 * @class ASIONetworkAdapter
 * @brief 基于 ASIO + ixwebsocket 的网络通信实现
 */
class ASIONetworkAdapter : public NetworkAdapter {
public:
    ASIONetworkAdapter() = default;
    ~ASIONetworkAdapter() override = default;

    bool init() override;
    void shutdown() override;

    HttpResponse http_get(const std::string& url,
                          const std::unordered_map<std::string, std::string>& headers = {}) override;
    HttpResponse http_post(const std::string& url,
                           const std::string& body,
                           const std::unordered_map<std::string, std::string>& headers = {}) override;

    bool ws_connect(const std::string& url, WsCallback* callback) override;
    bool ws_send(const WsMessage& message) override;
    bool ws_close() override;

private:
    // TODO: 添加 asio::io_context、ix::WebSocket 等私有成员
};

} // namespace Prism::Adapter
