#pragma once

#include "AdapterFactory.h"
#include "DBAdapter.h"

#include <mysql.h>

#include <mutex>

namespace Prism::Server::Adapter {

/**
 * @class MySqlDbAdapter
 * @brief 基于 libmysql 的 MySQL 数据库实现
 *
 * 表结构见 server/adapter/sql/schema.sql；查询见 server/adapter/sql/queries.sql。
 * 所有带参数的查询均使用 prepared statement，避免 SQL 注入。
 */
class MySqlDbAdapter : public DBAdapter {
  public:
    explicit MySqlDbAdapter(const MySqlConfig& config);
    ~MySqlDbAdapter() override;

    bool connect() override;
    void close() override;
    bool is_connected() override;

    bool insert_user(const UserRecord& user) override;
    std::optional<UserRecord> query_user_by_username(const std::string& username) override;
    std::optional<UserRecord> query_user_by_id(int64_t id) override;

    bool insert_room(const RoomRecord& room) override;
    std::optional<RoomRecord> query_room_by_id(const std::string& room_id) override;
    std::vector<RoomRecord> query_rooms() override;

    bool insert_room_member(const RoomMemberRecord& member) override;
    bool delete_room_member(const std::string& room_id, int64_t user_id) override;
    std::vector<RoomMemberRecord> query_room_members(const std::string& room_id) override;

    bool update_room_state(const std::string& room_id, RoomState state,
                           const std::string& current_media, int64_t position_ms) override;
    bool update_room_owner(const std::string& room_id, int64_t owner_id) override;
    bool delete_room(const std::string& room_id) override;

  private:
    MySqlConfig config_;
    MYSQL* mysql_{nullptr};
    std::mutex mutex_;
};

} // namespace Prism::Server::Adapter
