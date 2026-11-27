#include "DefaultNetworkClient.h"

#include <spdlog/spdlog.h>

namespace Prism::Service {

int DefaultNetworkClient::login(const char* username, const char* password,
                                const char* server_url) {
    spdlog::info("[DefaultNetwork] login: user={} server={}", username, server_url);
    logged_in_ = true;
    return PRISM_OK;
}

int DefaultNetworkClient::logout() {
    spdlog::info("[DefaultNetwork] logout");
    logged_in_ = false;
    in_room_ = false;
    room_info_ = {};
    return PRISM_OK;
}

int DefaultNetworkClient::join_room(const char* room_id) {
    if (!logged_in_) {
        spdlog::warn("[DefaultNetwork] join_room rejected: not logged in");
        return PRISM_ERROR_NETWORK;
    }
    spdlog::info("[DefaultNetwork] join_room: {}", room_id);
    in_room_ = true;
    room_info_ = {};
    constexpr size_t MAX_ROOM_ID_LENGTH = sizeof(room_info_.room_id) - 1;
    size_t length = 0;
    for (; length < MAX_ROOM_ID_LENGTH && room_id[length] != '\0'; ++length) {
        room_info_.room_id[length] = room_id[length];
    }
    room_info_.room_id[length] = '\0';
    room_info_.member_count = 1;
    return PRISM_OK;
}

int DefaultNetworkClient::leave_room() {
    spdlog::info("[DefaultNetwork] leave_room");
    in_room_ = false;
    room_info_ = {};
    return PRISM_OK;
}

int DefaultNetworkClient::get_room_info(PrismRoomInfo* info) {
    if (!in_room_) {
        return PRISM_ERROR_NETWORK;
    }
    if (info) {
        *info = room_info_;
    }
    return PRISM_OK;
}

PrismRoomState DefaultNetworkClient::get_room_state() const {
    return in_room_ ? PRISM_ROOM_CONNECTED : PRISM_ROOM_DISCONNECTED;
}

} // namespace Prism::Service
