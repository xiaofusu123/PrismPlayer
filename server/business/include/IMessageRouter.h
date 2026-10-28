#pragma once

#include <cstdint>
#include <string>

namespace Prism::Server::Business {

/**
 * @class IMessageRouter
 * @brief 消息路由抽象接口：解析入站消息并按类型转发
 */
class IMessageRouter {
  public:
    virtual ~IMessageRouter() = default;

    /**
     * @brief 路由一条入站消息
     * @param sender_user_id 发送者用户 ID
     * @param json 原始 JSON 消息字符串
     */
    virtual void route(int64_t sender_user_id, const std::string& json) = 0;
};

} // namespace Prism::Server::Business
