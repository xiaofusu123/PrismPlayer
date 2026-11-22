#include "RoomManager.h"

#include "DBAdapter.h"
#include "IClientNotifier.h"
#include "ServerTypes.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace Prism::Server::Business {

RoomManager::RoomManager(Adapter::DBAdapter* db, IClientNotifier* notifier)
    : db_(db), notifier_(notifier) {}

bool RoomManager::create_room(const std::string& room_id, const std::string& name,
                              int64_t owner_id) {
    Adapter::RoomRecord room;
    room.room_id = room_id;
    room.name = name;
    room.owner_id = owner_id;

    if (!db_->insert_room(room)) {
        spdlog::warn("create_room: room '{}' already exists", room_id);
        return false;
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

    nlohmann::json message;
    message["type"] = MSG_ROOM_UPDATE;
    message["room_id"] = room_id;
    message["member_count"] = static_cast<int>(db_->query_room_members(room_id).size());
    notifier_->broadcast_to_room(room_id, message.dump());
    return true;
}

bool RoomManager::leave_room(const std::string& room_id, int64_t user_id) {
    if (!db_->delete_room_member(room_id, user_id)) {
        spdlog::warn("leave_room: user {} not in room '{}'", user_id, room_id);
        return false;
    }
    spdlog::info("leave_room: user {} left '{}'", user_id, room_id);
    return true;
}

std::vector<Adapter::RoomRecord> RoomManager::list_rooms() {
    return db_->query_rooms();
}

} // namespace Prism::Server::Business
