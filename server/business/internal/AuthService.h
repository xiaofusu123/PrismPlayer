#pragma once

#include "IAuthService.h"

namespace Prism::Server::Adapter {
class DBAdapter;
class CryptoAdapter;
} // namespace Prism::Server::Adapter

namespace Prism::Server::Business {

/**
 * @class AuthService
 * @brief 用户认证实现
 */
class AuthService : public IAuthService {
  public:
    AuthService(Adapter::DBAdapter* db, Adapter::CryptoAdapter* crypto);
    ~AuthService() override = default;

    bool register_user(const std::string& username, const std::string& password) override;
    std::optional<int64_t> login(const std::string& username, const std::string& password) override;

  private:
    Adapter::DBAdapter* db_;
    Adapter::CryptoAdapter* crypto_;
};

} // namespace Prism::Server::Business
