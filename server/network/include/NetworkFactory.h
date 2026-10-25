#pragma once

#include "ISignalingServer.h"

#include <memory>

namespace Prism::Server::Network {

/**
 * @brief 创建 WebSocket 信令服务端实例
 * @return ISignalingServer 实例
 */
std::unique_ptr<ISignalingServer> create_signaling_server();

} // namespace Prism::Server::Network
