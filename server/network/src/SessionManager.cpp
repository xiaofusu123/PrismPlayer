#include "SessionManager.h"

namespace Prism::Server::Network {

bool SessionManager::send_to_user(int64_t user_id, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto user_it = user_id_to_conn_id_.find(user_id);
    if (user_it == user_id_to_conn_id_.end()) {
        return false;
    }
    auto socket_it = conn_id_to_socket_.find(user_it->second);
    if (socket_it == conn_id_to_socket_.end()) {
        return false;
    }
    socket_it->second->send(message);
    return true;
}

bool SessionManager::broadcast_to_room(const std::string& room_id, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto room_it = room_id_to_members_.find(room_id);
    if (room_it == room_id_to_members_.end()) {
        return false;
    }
    for (int64_t user_id : room_it->second) {
        auto user_it = user_id_to_conn_id_.find(user_id);
        if (user_it == user_id_to_conn_id_.end()) {
            continue;
        }
        auto socket_it = conn_id_to_socket_.find(user_it->second);
        if (socket_it == conn_id_to_socket_.end()) {
            continue;
        }
        socket_it->second->send(message);
    }
    return true;
}

void SessionManager::register_connection(const std::string& conn_id, ix::WebSocket* socket) {
    std::lock_guard<std::mutex> lock(mutex_);
    conn_id_to_socket_[conn_id] = socket;
}

void SessionManager::unregister_connection(const std::string& conn_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    conn_id_to_socket_.erase(conn_id);

    auto user_it = conn_id_to_user_id_.find(conn_id);
    if (user_it == conn_id_to_user_id_.end()) {
        return;
    }
    int64_t user_id = user_it->second;
    conn_id_to_user_id_.erase(user_it);
    user_id_to_conn_id_.erase(user_id);

    for (auto& entry : room_id_to_members_) {
        entry.second.erase(user_id);
    }
}

void SessionManager::bind_user(int64_t user_id, const std::string& conn_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    user_id_to_conn_id_[user_id] = conn_id;
    conn_id_to_user_id_[conn_id] = user_id;
}

std::optional<int64_t> SessionManager::user_of(const std::string& conn_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = conn_id_to_user_id_.find(conn_id);
    if (it == conn_id_to_user_id_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void SessionManager::add_room_member(const std::string& room_id, int64_t user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    room_id_to_members_[room_id].insert(user_id);
}

void SessionManager::remove_room_member(const std::string& room_id, int64_t user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = room_id_to_members_.find(room_id);
    if (it != room_id_to_members_.end()) {
        it->second.erase(user_id);
    }
}

} // namespace Prism::Server::Network
