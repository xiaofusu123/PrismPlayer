#pragma once

#include "IAccountManager.h"
#include "IRoomManager.h"
#include "ISignalingClient.h"

#include <memory>

namespace Prism::Business {

/**
 * @brief 创建账号管理器实例
 * @return IAccountManager 实例
 */
std::unique_ptr<IAccountManager> create_account_manager();

/**
 * @brief 创建房间管理器实例
 * @return IRoomManager 实例
 */
std::unique_ptr<IRoomManager> create_room_manager();

/**
 * @brief 创建信令客户端实例
 * @return ISignalingClient 实例
 */
std::unique_ptr<ISignalingClient> create_signaling_client();

} // namespace Prism::Business
