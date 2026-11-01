#include "InMemoryDbAdapter.h"

#include <spdlog/spdlog.h>

namespace Prism::Server::Adapter {

bool InMemoryDbAdapter::connect() {
    std::lock_guard<std::mutex> lock(mutex_);
    users_.clear();
    rooms_.clear();
    room_members_.clear();
    next_user_id_ = 1;
    spdlog::info("InMemoryDbAdapter: connected (in-memory store initialized)");
    return true;
}

void InMemoryDbAdapter::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    users_.clear();
    rooms_.clear();
    room_members_.clear();
    next_user_id_ = 1;
    spdlog::info("InMemoryDbAdapter: closed (in-memory store cleared)");
}

bool InMemoryDbAdapter::is_connected() {
    std::lock_guard<std::mutex> lock(mutex_);
    return true;
}

bool InMemoryDbAdapter::insert_user(const UserRecord& user) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& entry : users_) {
        if (entry.second.username == user.username) {
            spdlog::warn("insert_user: username '{}' already exists", user.username);
            return false;
        }
    }
    UserRecord record = user;
    record.id = next_user_id_++;
    users_.emplace(record.id, record);
    return true;
}

std::optional<UserRecord> InMemoryDbAdapter::query_user_by_username(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& entry : users_) {
        if (entry.second.username == username) {
            return entry.second;
        }
    }
    return std::nullopt;
}

std::optional<UserRecord> InMemoryDbAdapter::query_user_by_id(int64_t id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = users_.find(id);
    if (it == users_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool InMemoryDbAdapter::insert_room(const RoomRecord& room) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (rooms_.count(room.room_id) > 0) {
        return false;
    }
    rooms_.emplace(room.room_id, room);
    return true;
}

std::optional<RoomRecord> InMemoryDbAdapter::query_room_by_id(const std::string& room_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = rooms_.find(room_id);
    if (it == rooms_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<RoomRecord> InMemoryDbAdapter::query_rooms() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<RoomRecord> result;
    result.reserve(rooms_.size());
    for (const auto& entry : rooms_) {
        result.push_back(entry.second);
    }
    return result;
}

bool InMemoryDbAdapter::insert_room_member(const RoomMemberRecord& member) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& members = room_members_[member.room_id];
    for (const auto& m : members) {
        if (m.user_id == member.user_id) {
            return false;
        }
    }
    members.push_back(member);
    return true;
}

bool InMemoryDbAdapter::delete_room_member(const std::string& room_id, int64_t user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = room_members_.find(room_id);
    if (it == room_members_.end()) {
        return false;
    }
    auto& members = it->second;
    for (auto member_it = members.begin(); member_it != members.end(); ++member_it) {
        if (member_it->user_id == user_id) {
            members.erase(member_it);
            return true;
        }
    }
    return false;
}

std::vector<RoomMemberRecord> InMemoryDbAdapter::query_room_members(const std::string& room_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = room_members_.find(room_id);
    if (it == room_members_.end()) {
        return {};
    }
    return it->second;
}

} // namespace Prism::Server::Adapter
