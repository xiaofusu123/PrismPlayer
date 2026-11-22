#pragma once

#include "IClientNotifier.h"

#include <ixwebsocket/IXWebSocket.h>

#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>

namespace Prism::Server::Network {

/**
 * @class SessionManager
 * @brief 会话管理：维护连接、用户、房间在线成员映射，实现 IClientNotifier
 */
class SessionManager : public Business::IClientNotifier {
  public:
    SessionManager() = default;
    ~SessionManager() override = default;

    bool send_to_user(int64_t user_id, const std::string& message) override;
    bool broadcast_to_room(const std::string& room_id, const std::string& message) override;

    /**
     * @brief 注册一条连接
     * @param conn_id 连接 ID
     * @param socket WebSocket 指针（不持有所有权）
     */
    void register_connection(const std::string& conn_id, ix::WebSocket* socket);

    /**
     * @brief 注销一条连接（并清理其用户绑定与房间在线状态）
     * @param conn_id 连接 ID
     */
    void unregister_connection(const std::string& conn_id);

    /**
     * @brief 绑定用户到连接
     * @param user_id 用户 ID
     * @param conn_id 连接 ID
     */
    void bind_user(int64_t user_id, const std::string& conn_id);

    /**
     * @brief 查询连接对应的用户 ID
     * @param conn_id 连接 ID
     * @return 已绑定返回用户 ID，否则 nullopt
     */
    std::optional<int64_t> user_of(const std::string& conn_id);

    /**
     * @brief 记录用户加入房间（在线状态）
     * @param room_id 房间 ID
     * @param user_id 用户 ID
     */
    void add_room_member(const std::string& room_id, int64_t user_id);

    /**
     * @brief 记录用户离开房间（在线状态）
     * @param room_id 房间 ID
     * @param user_id 用户 ID
     */
    void remove_room_member(const std::string& room_id, int64_t user_id);

  private:
    std::mutex mutex_;
    std::map<std::string, ix::WebSocket*> conn_id_to_socket_;
    std::map<int64_t, std::string> user_id_to_conn_id_;
    std::map<std::string, int64_t> conn_id_to_user_id_;
    std::map<std::string, std::set<int64_t>> room_id_to_members_;
};

} // namespace Prism::Server::Network
