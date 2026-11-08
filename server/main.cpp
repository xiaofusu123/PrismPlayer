#include "AdapterFactory.h"
#include "BusinessFactory.h"
#include "NetworkFactory.h"

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <memory>
#include <string>

namespace {

/**
 * @brief 从环境变量读取 MySQL 连接配置
 *
 * 与 scripts/verify-db-queries.sh 的 PRISM_MYSQL_* 约定保持一致。
 * @return MySqlConfig 连接配置
 */
Prism::Server::Adapter::MySqlConfig mysql_config_from_env() {
    Prism::Server::Adapter::MySqlConfig config;
    if (const char* host = std::getenv("PRISM_MYSQL_HOST")) {
        config.host = host;
    }
    if (const char* port = std::getenv("PRISM_MYSQL_PORT")) {
        config.port = static_cast<uint16_t>(std::atoi(port));
    }
    if (const char* user = std::getenv("PRISM_MYSQL_USER")) {
        config.user = user;
    }
    if (const char* password = std::getenv("PRISM_MYSQL_PASSWORD")) {
        config.password = password;
    }
    if (const char* database = std::getenv("PRISM_MYSQL_DATABASE")) {
        config.database = database;
    }
    return config;
}

} // namespace

int main() {
    spdlog::set_level(spdlog::level::info);

    std::unique_ptr<Prism::Server::Adapter::DBAdapter> db;
    if (std::getenv("PRISM_MYSQL_HOST") != nullptr) {
        db = Prism::Server::Adapter::create_mysql_db_adapter(mysql_config_from_env());
    }
    if (!db) {
        db = Prism::Server::Adapter::create_db_adapter();
    }

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
