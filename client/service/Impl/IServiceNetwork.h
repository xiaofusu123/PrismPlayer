#pragma once

#include "Types.h"

namespace Prism::Service {

/**
 * @class IServiceNetwork
 * @brief 网络操作抽象接口
 *
 * 隔离 Service 层与具体网络实现（client-network 由吴圹钛负责）。
 * Service 层持有此接口指针，在 login/room 操作时委托调用。
 * 当前阶段提供默认桩实现，后续替换为真实网络模块。
 */
class IServiceNetwork {
public:
    virtual ~IServiceNetwork() = default;

    /**
     * @brief 登录服务器
     * @param username 用户名
     * @param password 密码
     * @param server_url 服务器地址
     * @return PRISM_OK 成功，否则返回错误码
     */
    virtual int login(const char* username, const char* password, const char* server_url) = 0;

    /**
     * @brief 登出
     * @return PRISM_OK
     */
    virtual int logout() = 0;

    /**
     * @brief 加入房间
     * @param room_id 房间 ID
     * @return PRISM_OK 成功
     */
    virtual int join_room(const char* room_id) = 0;

    /**
     * @brief 离开房间
     * @return PRISM_OK
     */
    virtual int leave_room() = 0;

    /**
     * @brief 获取房间信息
     * @param info [out] 房间信息
     * @return PRISM_OK
     */
    virtual int get_room_info(PrismRoomInfo* info) = 0;

    /**
     * @brief 获取房间连接状态
     * @return PrismRoomState
     */
    virtual PrismRoomState get_room_state() const = 0;
};

} // namespace Prism::Service
