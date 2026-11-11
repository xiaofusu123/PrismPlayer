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
 * @enum RoomState
 * @brief 房间播放状态
 *
 * - IDLE: 空闲
 * - PLAYING: 播放中
 * - PAUSED: 暂停
 */
enum class RoomState { IDLE, PLAYING, PAUSED };

/**
 * @struct RoomRecord
 * @brief 房间记录
 */
struct RoomRecord {
    std::string room_id;              /**< 房间 ID */
    std::string name;                 /**< 房间名 */
    int64_t owner_id{0};              /**< 房主用户 ID */
    int32_t max_members{0};           /**< 最大成员数 */
    RoomState state{RoomState::IDLE}; /**< 房间播放状态 */
    std::string current_media;        /**< 当前媒体 URI */
    int64_t position_ms{0};           /**< 当前播放位置（ms） */
};

/**
 * @struct RoomMemberRecord
 * @brief 房间成员记录
 */
struct RoomMemberRecord {
    std::string room_id;  /**< 房间 ID */
    int64_t user_id{0};   /**< 用户 ID */
    int64_t joined_at{0}; /**< 加入时间（ms，越小越早加入） */
};

/**
 * @class DBAdapter
 * @brief 数据库操作抽象接口（内存实现 + MySQL 实现，由 AdapterFactory 显式选择）
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
     * @return 成员列表（按加入时间升序）
     */
    virtual std::vector<RoomMemberRecord> query_room_members(const std::string& room_id) = 0;

    /**
     * @brief 更新房间播放状态（含当前媒体与位置）
     * @param room_id 房间 ID
     * @param state 目标播放状态
     * @param current_media 当前媒体 URI
     * @param position_ms 当前播放位置（ms）
     * @return 成功返回 true，房间不存在返回 false
     */
    virtual bool update_room_state(const std::string& room_id, RoomState state,
                                   const std::string& current_media, int64_t position_ms) = 0;

    /**
     * @brief 移交房主
     * @param room_id 房间 ID
     * @param owner_id 新房主用户 ID
     * @return 成功返回 true，房间不存在返回 false
     */
    virtual bool update_room_owner(const std::string& room_id, int64_t owner_id) = 0;

    /**
     * @brief 删除房间
     * @param room_id 房间 ID
     * @return 成功返回 true
     */
    virtual bool delete_room(const std::string& room_id) = 0;
};

} // namespace Prism::Server::Adapter
