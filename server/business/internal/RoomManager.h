#pragma once

#include "IRoomManager.h"

namespace Prism::Server::Adapter {
class DBAdapter;
} // namespace Prism::Server::Adapter

namespace Prism::Server::Business {

class IClientNotifier;

/**
 * @class RoomManager
 * @brief 房间管理实现
 */
class RoomManager : public IRoomManager {
  public:
    RoomManager(Adapter::DBAdapter* db, IClientNotifier* notifier);
    ~RoomManager() override = default;

    bool create_room(const std::string& room_id, const std::string& name,
                     int64_t owner_id) override;
    bool join_room(const std::string& room_id, int64_t user_id) override;
    bool leave_room(const std::string& room_id, int64_t user_id) override;
    std::vector<Adapter::RoomRecord> list_rooms() override;

  private:
    Adapter::DBAdapter* db_;
    IClientNotifier* notifier_;
};

} // namespace Prism::Server::Business
