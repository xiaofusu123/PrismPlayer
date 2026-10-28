#include "AuthService.h"

#include "CryptoAdapter.h"
#include "DBAdapter.h"

#include <spdlog/spdlog.h>

namespace Prism::Server::Business {

AuthService::AuthService(Adapter::DBAdapter* db, Adapter::CryptoAdapter* crypto)
    : db_(db), crypto_(crypto) {}

bool AuthService::register_user(const std::string& username, const std::string& password) {
    if (db_->query_user_by_username(username).has_value()) {
        spdlog::warn("register_user: username '{}' already exists", username);
        return false;
    }

    Adapter::HashResult hash = crypto_->hash_password(password);
    if (hash.hash.empty()) {
        spdlog::error("register_user: hash_password failed for '{}'", username);
        return false;
    }

    Adapter::UserRecord user;
    user.username = username;
    user.password_hash = hash.hash;
    user.salt = hash.salt;

    bool ok = db_->insert_user(user);
    if (ok) {
        spdlog::info("register_user: '{}' registered", username);
    }
    return ok;
}

std::optional<int64_t> AuthService::login(const std::string& username,
                                          const std::string& password) {
    auto user = db_->query_user_by_username(username);
    if (!user.has_value()) {
        spdlog::warn("login: username '{}' not found", username);
        return std::nullopt;
    }

    if (!crypto_->verify_password(password, user->password_hash, user->salt)) {
        spdlog::warn("login: password mismatch for '{}'", username);
        return std::nullopt;
    }

    spdlog::info("login: '{}' logged in (id={})", username, user->id);
    return user->id;
}

} // namespace Prism::Server::Business
