#pragma once

#include "IMessageRouter.h"

namespace Prism::Server::Adapter {
class DBAdapter;
} // namespace Prism::Server::Adapter

namespace Prism::Server::Business {

class IClientNotifier;

/**
 * @class MessageRouter
 * @brief 消息路由实现
 */
class MessageRouter : public IMessageRouter {
  public:
    MessageRouter(Adapter::DBAdapter* db, IClientNotifier* notifier);
    ~MessageRouter() override = default;

    void route(int64_t sender_user_id, const std::string& json) override;

  private:
    Adapter::DBAdapter* db_;
    IClientNotifier* notifier_;
};

} // namespace Prism::Server::Business
