#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Prism::Server::Adapter {
/**
 * @struct UserRecord
 * @brief 用户记录
 */
struct UserRecord {
    int64_t id{0};             /**< 用户 ID */
    std::string username;      /**< 用户名 */
    std::string password_hash; /**< 密码哈希（十六进制） */
    std::string salt;          /**< 密码盐（十六进制） */
};

/**
 * @struct RoomRecord
 * @brief 房间记录
 */
struct RoomRecord {
    std::string room_id;    /**< 房间 ID */
    std::string name;       /**< 房间名 */
    int64_t owner_id{0};    /**< 房主用户 ID */
    int32_t max_members{0}; /**< 最大成员数 */
};

/**
 * @struct RoomMemberRecord
 * @brief 房间成员记录
 */
struct RoomMemberRecord {
    std::string room_id; /**< 房间 ID */
    int64_t user_id{0};  /**< 用户 ID */
};

/**
 * @class DBAdapter
 * @brief 数据库操作抽象接口（当前内存实现，MySQL 实现延后到 #83）
 */
class DBAdapter {
  public:
    virtual ~DBAdapter() = default;

    /**
     * @brief 建立数据库连接
     * @return 成功返回 true
     */
    virtual bool connect() = 0;

    /**
     * @brief 关闭数据库连接
     */
    virtual void close() = 0;

    /**
     * @brief 检查数据库连接是否有效
     * @return 有效返回 true
     */
    virtual bool is_connected() = 0;

    /**
     * @brief 插入用户
     * @param user 用户记录
     * @return 成功返回 true，用户名重复返回 false
     */
    virtual bool insert_user(const UserRecord& user) = 0;

    /**
     * @brief 按用户名查询用户
     * @param username 用户名
     * @return 找到返回 UserRecord，否则 nullopt
     */
    virtual std::optional<UserRecord> query_user_by_username(const std::string& username) = 0;

    /**
     * @brief 按 ID 查询用户
     * @param id 用户 ID
     * @return 找到返回 UserRecord，否则 nullopt
     */
    virtual std::optional<UserRecord> query_user_by_id(int64_t id) = 0;

    /**
     * @brief 插入房间
     * @param room 房间记录
     * @return 成功返回 true，房间 ID 重复返回 false
     */
    virtual bool insert_room(const RoomRecord& room) = 0;

    /**
     * @brief 按 ID 查询房间
     * @param room_id 房间 ID
     * @return 找到返回 RoomRecord，否则 nullopt
     */
    virtual std::optional<RoomRecord> query_room_by_id(const std::string& room_id) = 0;

    /**
     * @brief 查询全部房间
     * @return 房间列表
     */
    virtual std::vector<RoomRecord> query_rooms() = 0;

    /**
     * @brief 插入房间成员
     * @param member 房间成员记录
     * @return 成功返回 true，已是成员返回 false
     */
    virtual bool insert_room_member(const RoomMemberRecord& member) = 0;

    /**
     * @brief 删除房间成员
     * @param room_id 房间 ID
     * @param user_id 用户 ID
     * @return 成功返回 true
     */
    virtual bool delete_room_member(const std::string& room_id, int64_t user_id) = 0;

    /**
     * @brief 查询房间全部成员
     * @param room_id 房间 ID
     * @return 成员列表
     */
    virtual std::vector<RoomMemberRecord> query_room_members(const std::string& room_id) = 0;
};

} // namespace Prism::Server::Adapter
