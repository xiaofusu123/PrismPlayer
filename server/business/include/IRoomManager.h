#pragma once

#include "DBAdapter.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Prism::Server::Business {

/**
 * @class IRoomManager
 * @brief 房间管理抽象接口
 */
class IRoomManager {
  public:
    virtual ~IRoomManager() = default;

    /**
     * @brief 创建房间
     * @param room_id 房间 ID
     * @param name 房间名
     * @param owner_id 房主用户 ID
     * @return 成功返回 true，房间已存在返回 false
     */
    virtual bool create_room(const std::string& room_id, const std::string& name,
                             int64_t owner_id) = 0;

    /**
     * @brief 加入房间
     * @param room_id 房间 ID
     * @param user_id 用户 ID
     * @return 成功返回 true
     */
    virtual bool join_room(const std::string& room_id, int64_t user_id) = 0;

    /**
     * @brief 离开房间
     * @param room_id 房间 ID
     * @param user_id 用户 ID
     * @return 成功返回 true
     */
    virtual bool leave_room(const std::string& room_id, int64_t user_id) = 0;

    /**
     * @brief 列出全部房间
     * @return 房间列表
     */
    virtual std::vector<Adapter::RoomRecord> list_rooms() = 0;

    /**
     * @brief 更新房间播放状态（当前媒体与位置）并广播房间状态
     * @param room_id 房间 ID
     * @param state 目标播放状态
     * @param position_ms 当前播放位置（ms）
     * @return 成功返回 true，房间不存在返回 false
     */
    virtual bool update_room_state(const std::string& room_id, Adapter::RoomState state,
                                   int64_t position_ms) = 0;
};

} // namespace Prism::Server::Business
