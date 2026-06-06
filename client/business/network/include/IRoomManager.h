#pragma once

#include "INetworkObserver.h"

namespace Prism::Business {

/**
 * @class IRoomManager
 * @brief 房间管理抽象接口
 *
 * 负责房间加入/离开及房间信息查询。
 * 加入/离开为异步操作，结果通过 INetworkObserver 回调通知。
 */
class IRoomManager {
public:
    virtual ~IRoomManager() = default;

    /**
     * @brief 加入指定房间
     * @param room_id 房间 ID
     * @return true 请求已发出，结果通过 on_room_joined 回调
     */
    virtual bool join_room(const char* room_id) = 0;

    /**
     * @brief 离开当前房间
     * @return true 操作成功，结果通过 on_room_left 回调
     */
    virtual bool leave_room() = 0;

    /**
     * @brief 获取当前房间信息
     * @param info [out] 房间信息输出参数
     * @return true 获取成功
     */
    virtual bool get_room_info(RoomInfo* info) const = 0;

    /**
     * @brief 获取当前房间连接状态
     * @return NetworkState 当前状态
     */
    virtual NetworkState get_room_state() const = 0;

    /**
     * @brief 注册事件观察者
     * @param observer 观察者指针（不持有所有权）
     */
    virtual void set_observer(INetworkObserver* observer) = 0;
};

} // namespace Prism::Business
