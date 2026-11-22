#pragma once

#include "CryptoAdapter.h"
#include "DBAdapter.h"
#include "IAuthService.h"
#include "IClientNotifier.h"
#include "IMessageRouter.h"
#include "IRoomManager.h"

#include <memory>

namespace Prism::Server::Business {

/**
 * @brief 创建认证服务实例
 * @param db 数据库适配器（不持有所有权）
 * @param crypto 密码学适配器（不持有所有权）
 */
std::unique_ptr<IAuthService> create_auth_service(Adapter::DBAdapter* db,
                                                  Adapter::CryptoAdapter* crypto);

/**
 * @brief 创建房间管理器实例
 * @param db 数据库适配器（不持有所有权）
 * @param notifier 客户端推送器（不持有所有权）
 */
std::unique_ptr<IRoomManager> create_room_manager(Adapter::DBAdapter* db,
                                                  IClientNotifier* notifier);

/**
 * @brief 创建消息路由器实例
 * @param db 数据库适配器（不持有所有权）
 * @param notifier 客户端推送器（不持有所有权）
 */
std::unique_ptr<IMessageRouter> create_message_router(Adapter::DBAdapter* db,
                                                      IClientNotifier* notifier);

} // namespace Prism::Server::Business
