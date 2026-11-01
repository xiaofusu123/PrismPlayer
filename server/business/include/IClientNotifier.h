#pragma once

#include <cstdint>
#include <string>

namespace Prism::Server::Business {

/**
 * @class IClientNotifier
 * @brief 客户端消息推送抽象接口（由 Network 层实现）
 */
class IClientNotifier {
  public:
    virtual ~IClientNotifier() = default;

    /**
     * @brief 向指定用户发送消息
     * @param user_id 用户 ID
     * @param message 消息内容（JSON 字符串）
     * @return 用户在线且发送成功返回 true
     */
    virtual bool send_to_user(int64_t user_id, const std::string& message) = 0;

    /**
     * @brief 向房间内所有在线成员广播消息
     * @param room_id 房间 ID
     * @param message 消息内容（JSON 字符串）
     * @return 成功返回 true
     */
    virtual bool broadcast_to_room(const std::string& room_id, const std::string& message) = 0;
};

} // namespace Prism::Server::Business
