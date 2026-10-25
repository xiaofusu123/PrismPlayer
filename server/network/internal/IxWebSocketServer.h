#pragma once

#include "ISignalingServer.h"

#include <ixwebsocket/IXWebSocketServer.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

namespace Prism::Server::Network {

class SessionManager;

/**
 * @class IxWebSocketServer
 * @brief 基于 ixwebsocket 的 WebSocket 信令服务端实现
 */
class IxWebSocketServer : public ISignalingServer {
  public:
    IxWebSocketServer();
    ~IxWebSocketServer() override;

    Business::IClientNotifier* notifier() override;
    void set_handlers(Business::IAuthService* auth, Business::IRoomManager* rooms,
                      Business::IMessageRouter* router) override;
    bool start(const std::string& host, uint16_t port) override;
    void stop() override;
    void wait() override;

  private:
    void handle_message(const std::shared_ptr<ix::ConnectionState>& conn, ix::WebSocket& socket,
                        const ix::WebSocketMessagePtr& message);
    void handle_json_message(const std::string& conn_id, ix::WebSocket& socket,
                             const std::string& payload);

    void handle_register(const std::string& conn_id, ix::WebSocket& socket,
                         const nlohmann::json& message);
    void handle_login(const std::string& conn_id, ix::WebSocket& socket,
                      const nlohmann::json& message);
    void handle_create_room(const std::string& conn_id, ix::WebSocket& socket,
                            const nlohmann::json& message);
    void handle_join_room(const std::string& conn_id, ix::WebSocket& socket,
                          const nlohmann::json& message);
    void handle_leave_room(const std::string& conn_id, ix::WebSocket& socket,
                           const nlohmann::json& message);

    void send_ack(ix::WebSocket& socket, const std::string& for_type);
    void send_error(ix::WebSocket& socket, int code, const std::string& message);

    std::unique_ptr<SessionManager> session_manager_;

    Business::IAuthService* auth_{nullptr};
    Business::IRoomManager* rooms_{nullptr};
    Business::IMessageRouter* router_{nullptr};

    std::unique_ptr<ix::WebSocketServer> server_;
    std::atomic<bool> running_{false};
};

} // namespace Prism::Server::Network
