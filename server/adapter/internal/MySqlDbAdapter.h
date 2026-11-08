#pragma once

#ifdef PRISM_SERVER_WITH_MYSQL

#include "DBAdapter.h"

#include "AdapterFactory.h"

#include <mysql.h>
#include <mutex>

namespace Prism::Server::Adapter {

/**
 * @class MySqlDbAdapter
 * @brief 基于 MySQL C API 的数据库实现
 *
 * 仅在编译期定义 PRISM_SERVER_WITH_MYSQL 时参与构建（见 adapter/CMakeLists.txt）。
 * 与内存实现共享 DBAdapter 接口；字段映射见 schema.sql 表头注释。
 */
class MySqlDbAdapter : public DBAdapter {
  public:
    /**
     * @brief 构造 MySQL 适配器
     * @param config MySQL 连接配置
     */
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

  private:
    MySqlConfig config_;
    MYSQL* mysql_{nullptr};
    std::mutex mutex_;
};

} // namespace Prism::Server::Adapter

#endif // PRISM_SERVER_WITH_MYSQL
