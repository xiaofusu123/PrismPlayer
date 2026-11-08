#include "RoomManager.h"

#include "DBAdapter.h"
#include "IClientNotifier.h"
#include "ServerTypes.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace Prism::Server::Business {

namespace {

const char* room_state_to_str(Adapter::RoomState state) {
    switch (state) {
    case Adapter::RoomState::PLAYING:
        return "playing";
    case Adapter::RoomState::PAUSED:
        return "paused";
    case Adapter::RoomState::IDLE:
        return "idle";
    }
    return "idle";
}

} // namespace

RoomManager::RoomManager(Adapter::DBAdapter* db, IClientNotifier* notifier)
    : db_(db), notifier_(notifier) {}

bool RoomManager::create_room(const std::string& room_id, const std::string& name,
                              int64_t owner_id) {
    Adapter::RoomRecord room;
    room.room_id = room_id;
    room.name = name;
    room.owner_id = owner_id;
    room.state = Adapter::RoomState::IDLE;

    if (!db_->insert_room(room)) {
        spdlog::warn("create_room: room '{}' already exists", room_id);
        return false;
    }

    Adapter::RoomMemberRecord owner;
    owner.room_id = room_id;
    owner.user_id = owner_id;
    if (!db_->insert_room_member(owner)) {
        spdlog::warn("create_room: add owner {} to '{}' failed", owner_id, room_id);
    }

    spdlog::info("create_room: '{}' created by user {}", room_id, owner_id);
    return true;
}

bool RoomManager::join_room(const std::string& room_id, int64_t user_id) {
    if (!db_->query_room_by_id(room_id).has_value()) {
        spdlog::warn("join_room: room '{}' not found", room_id);
        return false;
    }

    Adapter::RoomMemberRecord member;
    member.room_id = room_id;
    member.user_id = user_id;
    if (!db_->insert_room_member(member)) {
        spdlog::warn("join_room: user {} already in room '{}'", user_id, room_id);
        return false;
    }
    spdlog::info("join_room: user {} joined '{}'", user_id, room_id);

    broadcast_room_update(room_id);
    return true;
}

bool RoomManager::leave_room(const std::string& room_id, int64_t user_id) {
    auto room = db_->query_room_by_id(room_id);
    if (!room.has_value()) {
        spdlog::warn("leave_room: room '{}' not found", room_id);
        return false;
    }

    if (!db_->delete_room_member(room_id, user_id)) {
        spdlog::warn("leave_room: user {} not in room '{}'", user_id, room_id);
        return false;
    }
    spdlog::info("leave_room: user {} left '{}'", user_id, room_id);

    auto members = db_->query_room_members(room_id);
    if (members.empty()) {
        db_->delete_room(room_id);
        spdlog::info("leave_room: room '{}' destroyed (empty)", room_id);
        return true;
    }

    if (room->owner_id == user_id) {
        int64_t new_owner_id = members.front().user_id;
        int64_t earliest = members.front().joined_at;
        for (const auto& member : members) {
            if (member.joined_at < earliest) {
                earliest = member.joined_at;
                new_owner_id = member.user_id;
            }
        }
        db_->update_room_owner(room_id, new_owner_id);
        spdlog::info("leave_room: owner transferred to {} in '{}'", new_owner_id, room_id);
    }

    broadcast_room_update(room_id);
    return true;
}

std::vector<Adapter::RoomRecord> RoomManager::list_rooms() {
    return db_->query_rooms();
}

bool RoomManager::update_room_state(const std::string& room_id, Adapter::RoomState state,
                                    int64_t position_ms) {
    auto room = db_->query_room_by_id(room_id);
    if (!room.has_value()) {
        spdlog::warn("update_room_state: room '{}' not found", room_id);
        return false;
    }

    if (!db_->update_room_state(room_id, state, room->current_media, position_ms)) {
        spdlog::warn("update_room_state: update '{}' failed", room_id);
        return false;
    }

    broadcast_room_update(room_id);
    return true;
}

void RoomManager::broadcast_room_update(const std::string& room_id) {
    auto room = db_->query_room_by_id(room_id);
    if (!room.has_value()) {
        return;
    }

    nlohmann::json message;
    message["type"] = MSG_ROOM_UPDATE;
    message["room_id"] = room_id;
    message["owner_id"] = room->owner_id;
    message["state"] = room_state_to_str(room->state);
    message["current_media"] = room->current_media;
    message["position_ms"] = room->position_ms;
    message["member_count"] = static_cast<int>(db_->query_room_members(room_id).size());
    notifier_->broadcast_to_room(room_id, message.dump());
}

} // namespace Prism::Server::Business
