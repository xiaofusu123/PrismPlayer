#pragma once

#include "IMessageRouter.h"

#include <nlohmann/json.hpp>

namespace Prism::Server::Adapter {
class DBAdapter;
} // namespace Prism::Server::Adapter

namespace Prism::Server::Business {

class IClientNotifier;
class IRoomManager;

/**
 * @class MessageRouter
 * @brief 消息路由实现（播放控制落库并广播、聊天转发）
 */
class MessageRouter : public IMessageRouter {
  public:
    MessageRouter(Adapter::DBAdapter* db, IRoomManager* rooms, IClientNotifier* notifier);
    ~MessageRouter() override = default;

    void route(int64_t sender_user_id, const std::string& json) override;

  private:
    void update_playback_state(const std::string& room_id, const nlohmann::json& message);

    Adapter::DBAdapter* db_;
    IRoomManager* rooms_;
    IClientNotifier* notifier_;
};

} // namespace Prism::Server::Business
