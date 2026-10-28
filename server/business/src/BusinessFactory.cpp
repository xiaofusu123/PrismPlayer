#include "BusinessFactory.h"

#include "AuthService.h"
#include "MessageRouter.h"
#include "RoomManager.h"

namespace Prism::Server::Business {

std::unique_ptr<IAuthService> create_auth_service(Adapter::DBAdapter* db,
                                                  Adapter::CryptoAdapter* crypto) {
    return std::make_unique<AuthService>(db, crypto);
}

std::unique_ptr<IRoomManager> create_room_manager(Adapter::DBAdapter* db,
                                                  IClientNotifier* notifier) {
    return std::make_unique<RoomManager>(db, notifier);
}

std::unique_ptr<IMessageRouter> create_message_router(Adapter::DBAdapter* db,
                                                      IClientNotifier* notifier) {
    return std::make_unique<MessageRouter>(db, notifier);
}

} // namespace Prism::Server::Business
