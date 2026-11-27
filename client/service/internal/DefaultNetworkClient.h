#pragma once

#include "IServiceNetwork.h"

namespace Prism::Service {

/**
 * @class DefaultNetworkClient
 * @brief 网络抽象的默认桩实现
 *
 * 没有真实网络模块时兜底：记录登录与房间状态、返回 PRISM_OK，
 * 使 Service 在 client-network 尚未落地时仍可编译运行。
 * 真实实现由 prism_player_set_network_client() 注入替换。
 */
class DefaultNetworkClient : public IServiceNetwork {
  public:
    int login(const char* username, const char* password, const char* server_url) override;
    int logout() override;
    int join_room(const char* room_id) override;
    int leave_room() override;
    int get_room_info(PrismRoomInfo* info) override;
    [[nodiscard]] PrismRoomState get_room_state() const override;

  private:
    bool logged_in_{false};
    bool in_room_{false};
    PrismRoomInfo room_info_{};
};

} // namespace Prism::Service
