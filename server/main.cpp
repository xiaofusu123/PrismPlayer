#include "AdapterFactory.h"
#include "BusinessFactory.h"
#include "NetworkFactory.h"

#include <spdlog/spdlog.h>

int main() {
    spdlog::set_level(spdlog::level::info);

    auto db = Prism::Server::Adapter::create_db_adapter();
    auto crypto = Prism::Server::Adapter::create_crypto_adapter();

    if (!db->connect()) {
        spdlog::error("Prism-server: failed to connect to database");
        return 1;
    }

    auto server = Prism::Server::Network::create_signaling_server();
    auto notifier = server->notifier();

    auto auth = Prism::Server::Business::create_auth_service(db.get(), crypto.get());
    auto rooms = Prism::Server::Business::create_room_manager(db.get(), notifier);
    auto router = Prism::Server::Business::create_message_router(db.get(), notifier);

    server->set_handlers(auth.get(), rooms.get(), router.get());
    server->start("0.0.0.0", 8080);

    spdlog::info("Prism-server running on 0.0.0.0:8080");
    server->wait();
    return 0;
}
