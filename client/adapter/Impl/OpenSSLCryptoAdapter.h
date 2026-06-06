#pragma once

#include "CryptoAdapter.h"

namespace Prism::Adapter {

/**
 * @class OpenSSLCryptoAdapter
 * @brief 基于 OpenSSL 的密码学操作实现
 */
class OpenSSLCryptoAdapter : public CryptoAdapter {
public:
    OpenSSLCryptoAdapter() = default;
    ~OpenSSLCryptoAdapter() override = default;

    HashResult hash_password(const std::string& password) override;
    bool verify_password(const std::string& password,
                         const std::string& hash,
                         const std::string& salt) override;

    bool encrypt_file(const std::string& input_path,
                      const std::string& output_path,
                      const std::string& key) override;
    bool decrypt_file(const std::string& input_path,
                      const std::string& output_path,
                      const std::string& key) override;

    EncryptResult encrypt_buffer(const uint8_t* data, size_t size, const std::string& key) override;
    std::vector<uint8_t> decrypt_buffer(const uint8_t* data,
                                        size_t size,
                                        const std::string& key,
                                        const std::vector<uint8_t>& iv,
                                        const std::vector<uint8_t>& tag) override;

private:
    // TODO: 添加 OpenSSL 相关私有成员
};

} // namespace Prism::Adapter
