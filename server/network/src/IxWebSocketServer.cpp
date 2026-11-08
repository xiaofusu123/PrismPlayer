#include "IxWebSocketServer.h"

#include "IAuthService.h"
#include "IMessageRouter.h"
#include "IRoomManager.h"
#include "ServerTypes.h"
#include "SessionManager.h"

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocketMessage.h>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace Prism::Server::Network {

IxWebSocketServer::IxWebSocketServer() {
    ix::initNetSystem();
    session_manager_ = std::make_unique<SessionManager>();
}

IxWebSocketServer::~IxWebSocketServer() {
    stop();
}

Business::IClientNotifier* IxWebSocketServer::notifier() {
    return session_manager_.get();
}

void IxWebSocketServer::set_handlers(Business::IAuthService* auth, Business::IRoomManager* rooms,
                                     Business::IMessageRouter* router) {
    auth_ = auth;
    rooms_ = rooms;
    router_ = router;
}

bool IxWebSocketServer::start(const std::string& host, uint16_t port) {
    server_ = std::make_unique<ix::WebSocketServer>(port, host);

    server_->setOnClientMessageCallback(
        [this](std::shared_ptr<ix::ConnectionState> conn, ix::WebSocket& socket,
               const ix::WebSocketMessagePtr& message) { handle_message(conn, socket, message); });

    if (!server_->listenAndStart()) {
        spdlog::error("IxWebSocketServer: failed to listen on {}:{}", host, port);
        return false;
    }

    running_ = true;
    spdlog::info("IxWebSocketServer: listening on {}:{}", host, port);
    return true;
}

void IxWebSocketServer::stop() {
    if (!running_) {
        return;
    }
    if (server_) {
        server_->stop();
    }
    running_ = false;
}

void IxWebSocketServer::wait() {
    if (server_) {
        server_->wait();
    }
}

void IxWebSocketServer::handle_message(const std::shared_ptr<ix::ConnectionState>& conn,
                                       ix::WebSocket& socket,
                                       const ix::WebSocketMessagePtr& message) {
    std::string conn_id = conn->getId();

    switch (message->type) {
    case ix::WebSocketMessageType::Open:
        session_manager_->register_connection(conn_id, &socket);
        spdlog::info("connection {} opened", conn_id);
        break;
    case ix::WebSocketMessageType::Close: {
        auto user_id = session_manager_->user_of(conn_id);
        std::vector<std::string> room_ids;
        if (user_id.has_value()) {
            room_ids = session_manager_->rooms_of_user(*user_id);
        }
        session_manager_->unregister_connection(conn_id);
        if (user_id.has_value() && rooms_ != nullptr) {
            for (const auto& room_id : room_ids) {
                rooms_->leave_room(room_id, *user_id);
            }
        }
        spdlog::info("connection {} closed", conn_id);
        break;
    }
    case ix::WebSocketMessageType::Message:
        handle_json_message(conn_id, socket, message->str);
        break;
    default:
        break;
    }
}

void IxWebSocketServer::handle_json_message(const std::string& conn_id, ix::WebSocket& socket,
                                            const std::string& payload) {
    nlohmann::json message;
    try {
        message = nlohmann::json::parse(payload);
    } catch (const nlohmann::json::exception& e) {
        spdlog::warn("handle_json_message: invalid JSON: {}", e.what());
        send_error(socket, 400, "invalid json");
        return;
    }

    if (!message.contains("type") || !message["type"].is_string()) {
        send_error(socket, 400, "missing type");
        return;
    }

    std::string type = message["type"].get<std::string>();

    if (type == Business::MSG_REGISTER) {
        handle_register(conn_id, socket, message);
    } else if (type == Business::MSG_LOGIN) {
        handle_login(conn_id, socket, message);
    } else if (type == Business::MSG_LOGOUT) {
        handle_logout(conn_id, socket, message);
    } else if (type == Business::MSG_CREATE_ROOM) {
        handle_create_room(conn_id, socket, message);
    } else if (type == Business::MSG_JOIN_ROOM) {
        handle_join_room(conn_id, socket, message);
    } else if (type == Business::MSG_LEAVE_ROOM) {
        handle_leave_room(conn_id, socket, message);
    } else {
        auto sender = require_auth(socket, message);
        if (router_ != nullptr && sender.has_value()) {
            router_->route(*sender, payload);
        }
    }
}

void IxWebSocketServer::handle_register(const std::string& /*conn_id*/, ix::WebSocket& socket,
                                        const nlohmann::json& message) {
    if (auth_ == nullptr) {
        send_error(socket, 500, "auth not ready");
        return;
    }
    if (!message.contains("username") || !message.contains("password")) {
        send_error(socket, 400, "missing username or password");
        return;
    }
    std::string username = message["username"].get<std::string>();
    std::string password = message["password"].get<std::string>();
    if (auth_->register_user(username, password)) {
        send_ack(socket, Business::MSG_REGISTER);
    } else {
        send_error(socket, 409, "username already exists");
    }
}

void IxWebSocketServer::handle_login(const std::string& conn_id, ix::WebSocket& socket,
                                     const nlohmann::json& message) {
    if (auth_ == nullptr) {
        send_error(socket, 500, "auth not ready");
        return;
    }
    if (!message.contains("username") || !message.contains("password")) {
        send_error(socket, 400, "missing username or password");
        return;
    }
    std::string username = message["username"].get<std::string>();
    std::string password = message["password"].get<std::string>();
    auto result = auth_->login(username, password);
    if (result.error != Business::AuthError::NONE) {
        send_error(socket, Business::ERR_INVALID_CREDENTIALS, "invalid credentials");
        return;
    }
    session_manager_->bind_user(result.user_id, conn_id);

    nlohmann::json ack;
    ack["type"] = Business::MSG_ACK;
    ack["ok"] = true;
    ack["for"] = Business::MSG_LOGIN;
    ack["token"] = result.token;
    socket.send(ack.dump());
}

void IxWebSocketServer::handle_logout(const std::string& /*conn_id*/, ix::WebSocket& socket,
                                      const nlohmann::json& message) {
    if (auth_ == nullptr) {
        send_error(socket, 500, "auth not ready");
        return;
    }
    if (!message.contains("token") || !message["token"].is_string()) {
        send_error(socket, Business::ERR_NOT_AUTHENTICATED, "not authenticated");
        return;
    }
    std::string token = message["token"].get<std::string>();
    auto result = auth_->verify_token(token);
    if (result.error == Business::AuthError::TOKEN_EXPIRED) {
        send_error(socket, Business::ERR_TOKEN_EXPIRED, "token expired");
        return;
    }
    if (result.error != Business::AuthError::NONE) {
        send_error(socket, Business::ERR_NOT_AUTHENTICATED, "not authenticated");
        return;
    }
    auth_->logout(token);
    session_manager_->unbind_user(result.user_id);
    send_ack(socket, Business::MSG_LOGOUT);
}

void IxWebSocketServer::handle_create_room(const std::string& /*conn_id*/, ix::WebSocket& socket,
                                           const nlohmann::json& message) {
    auto user_id = require_auth(socket, message);
    if (!user_id.has_value()) {
        return;
    }
    if (!message.contains("room_id")) {
        send_error(socket, 400, "missing room_id");
        return;
    }
    std::string room_id = message["room_id"].get<std::string>();
    std::string name = message.value("name", room_id);
    if (rooms_ != nullptr && rooms_->create_room(room_id, name, *user_id)) {
        session_manager_->add_room_member(room_id, *user_id);
        send_ack(socket, Business::MSG_CREATE_ROOM);
    } else {
        send_error(socket, 409, "room already exists");
    }
}

void IxWebSocketServer::handle_join_room(const std::string& /*conn_id*/, ix::WebSocket& socket,
                                         const nlohmann::json& message) {
    auto user_id = require_auth(socket, message);
    if (!user_id.has_value()) {
        return;
    }
    if (!message.contains("room_id")) {
        send_error(socket, 400, "missing room_id");
        return;
    }
    std::string room_id = message["room_id"].get<std::string>();
    if (rooms_ != nullptr && rooms_->join_room(room_id, *user_id)) {
        session_manager_->add_room_member(room_id, *user_id);
        send_ack(socket, Business::MSG_JOIN_ROOM);
    } else {
        send_error(socket, 404, "room not found or already joined");
    }
}

void IxWebSocketServer::handle_leave_room(const std::string& /*conn_id*/, ix::WebSocket& socket,
                                          const nlohmann::json& message) {
    auto user_id = require_auth(socket, message);
    if (!user_id.has_value()) {
        return;
    }
    if (!message.contains("room_id")) {
        send_error(socket, 400, "missing room_id");
        return;
    }
    std::string room_id = message["room_id"].get<std::string>();
    if (rooms_ != nullptr && rooms_->leave_room(room_id, *user_id)) {
        session_manager_->remove_room_member(room_id, *user_id);
        send_ack(socket, Business::MSG_LEAVE_ROOM);
    } else {
        send_error(socket, 404, "not in room");
    }
}

std::optional<int64_t> IxWebSocketServer::require_auth(ix::WebSocket& socket,
                                                       const nlohmann::json& message) {
    if (auth_ == nullptr) {
        send_error(socket, 500, "auth not ready");
        return std::nullopt;
    }
    if (!message.contains("token") || !message["token"].is_string()) {
        send_error(socket, Business::ERR_NOT_AUTHENTICATED, "not authenticated");
        return std::nullopt;
    }
    std::string token = message["token"].get<std::string>();
    auto result = auth_->verify_token(token);
    if (result.error == Business::AuthError::TOKEN_EXPIRED) {
        send_error(socket, Business::ERR_TOKEN_EXPIRED, "token expired");
        return std::nullopt;
    }
    if (result.error != Business::AuthError::NONE) {
        send_error(socket, Business::ERR_NOT_AUTHENTICATED, "not authenticated");
        return std::nullopt;
    }
    return result.user_id;
}

void IxWebSocketServer::send_ack(ix::WebSocket& socket, const std::string& for_type) {
    nlohmann::json ack;
    ack["type"] = Business::MSG_ACK;
    ack["ok"] = true;
    ack["for"] = for_type;
    socket.send(ack.dump());
}

void IxWebSocketServer::send_error(ix::WebSocket& socket, int code, const std::string& message) {
    nlohmann::json err;
    err["type"] = Business::MSG_ERROR;
    err["code"] = code;
    err["message"] = message;
    socket.send(err.dump());
}

} // namespace Prism::Server::Network
