#pragma once

#include "DBAdapter.h"

#include <map>
#include <mutex>
#include <vector>

namespace Prism::Server::Adapter {

/**
 * @class InMemoryDbAdapter
 * @brief 内存数据库实现
 */
class InMemoryDbAdapter : public DBAdapter {
  public:
    InMemoryDbAdapter() = default;
    ~InMemoryDbAdapter() override = default;

    bool connect() override;
    void close() override;
    bool is_connected() const override;

    bool insert_user(const UserRecord& user) override;
    std::optional<UserRecord> query_user_by_username(const std::string& username) override;
    std::optional<UserRecord> query_user_by_id(int64_t id) override;

    bool insert_room(const RoomRecord& room) override;
    std::optional<RoomRecord> query_room_by_id(const std::string& room_id) override;
    std::vector<RoomRecord> query_rooms() override;

    bool insert_room_member(const RoomMemberRecord& member) override;
    bool delete_room_member(const std::string& room_id, int64_t user_id) override;
    std::vector<RoomMemberRecord> query_room_members(const std::string& room_id) override;

  private:
    std::mutex mutex_;
    std::map<int64_t, UserRecord> users_;
    int64_t next_user_id_{1};
    std::map<std::string, RoomRecord> rooms_;
    std::map<std::string, std::vector<RoomMemberRecord>> room_members_;
};

} // namespace Prism::Server::Adapter
