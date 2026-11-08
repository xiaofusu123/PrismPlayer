#pragma once

#include "IAuthService.h"

#include <chrono>
#include <mutex>
#include <unordered_map>

namespace Prism::Server::Adapter {
class DBAdapter;
class CryptoAdapter;
} // namespace Prism::Server::Adapter

namespace Prism::Server::Business {

/**
 * @class AuthService
 * @brief 用户认证实现（含会话令牌的签发 / 校验 / 过期 / 登出）
 */
class AuthService : public IAuthService {
  public:
    AuthService(Adapter::DBAdapter* db, Adapter::CryptoAdapter* crypto);
    ~AuthService() override = default;

    bool register_user(const std::string& username, const std::string& password) override;
    AuthResult login(const std::string& username, const std::string& password) override;
    AuthResult verify_token(const std::string& token) override;
    void logout(const std::string& token) override;

  private:
    struct TokenEntry {
        int64_t user_id{0};
        std::chrono::steady_clock::time_point expires_at;
    };

    Adapter::DBAdapter* db_;
    Adapter::CryptoAdapter* crypto_;

    std::mutex token_mutex_;
    std::unordered_map<std::string, TokenEntry> tokens_;
};

} // namespace Prism::Server::Business
