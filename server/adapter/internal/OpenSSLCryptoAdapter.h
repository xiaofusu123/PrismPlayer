#pragma once

#include "CryptoAdapter.h"

namespace Prism::Server::Adapter {

/**
 * @class OpenSSLCryptoAdapter
 * @brief 基于 OpenSSL 的密码哈希实现（SHA-256 + 随机盐）
 */
class OpenSSLCryptoAdapter : public CryptoAdapter {
  public:
    OpenSSLCryptoAdapter() = default;
    ~OpenSSLCryptoAdapter() override = default;

    HashResult hash_password(const std::string& password) override;
    bool verify_password(const std::string& password, const std::string& hash,
                         const std::string& salt) override;
};

} // namespace Prism::Server::Adapter
