#include "AuthService.h"

#include "CryptoAdapter.h"
#include "DBAdapter.h"

#include <spdlog/spdlog.h>

namespace Prism::Server::Business {

namespace {

// 会话令牌有效期：24 小时
constexpr auto TOKEN_TTL = std::chrono::hours(24);

} // namespace

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

AuthResult AuthService::login(const std::string& username, const std::string& password) {
    auto user = db_->query_user_by_username(username);
    if (!user.has_value()) {
        spdlog::warn("login: username '{}' not found", username);
        AuthResult result;
        result.error = AuthError::INVALID_CREDENTIALS;
        return result;
    }

    if (!crypto_->verify_password(password, user->password_hash, user->salt)) {
        spdlog::warn("login: password mismatch for '{}'", username);
        AuthResult result;
        result.error = AuthError::INVALID_CREDENTIALS;
        return result;
    }

    std::string token = crypto_->generate_token();
    if (token.empty()) {
        spdlog::error("login: generate_token failed for '{}'", username);
        AuthResult result;
        result.error = AuthError::INVALID_CREDENTIALS;
        return result;
    }

    {
        std::lock_guard<std::mutex> lock(token_mutex_);
        tokens_[token] = TokenEntry{user->id, std::chrono::steady_clock::now() + TOKEN_TTL};
    }

    spdlog::info("login: '{}' logged in (id={})", username, user->id);
    AuthResult result;
    result.user_id = user->id;
    result.token = std::move(token);
    return result;
}

AuthResult AuthService::verify_token(const std::string& token) {
    std::lock_guard<std::mutex> lock(token_mutex_);
    auto it = tokens_.find(token);
    if (it == tokens_.end()) {
        AuthResult result;
        result.error = AuthError::NOT_AUTHENTICATED;
        return result;
    }
    if (std::chrono::steady_clock::now() >= it->second.expires_at) {
        tokens_.erase(it);
        AuthResult result;
        result.error = AuthError::TOKEN_EXPIRED;
        return result;
    }
    AuthResult result;
    result.user_id = it->second.user_id;
    return result;
}

void AuthService::logout(const std::string& token) {
    std::lock_guard<std::mutex> lock(token_mutex_);
    tokens_.erase(token);
    spdlog::info("logout: token revoked");
}

} // namespace Prism::Server::Business
