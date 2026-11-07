#pragma once

#include "CryptoAdapter.h"

namespace Prism::Adapter {

/**
 * @class OpenSSLCryptoAdapter
 * @brief 基于 OpenSSL 的密码学操作实现
 *
 * - **密码哈希**：PBKDF2-HMAC-SHA256，10 万次迭代 + 16 字节随机盐，派生 32 字节，
 *   小写 hex 编码。落库格式**自带参数**：`pbkdf2-sha256$<iter>$<salt_hex>$<dk_hex>`。
 *   这样 `AccountInfo` 只有一个 `password_hash` 字段也不会丢盐，且将来能在
 *   **不改表结构**的前提下提升迭代次数或换算法（验证时按各自的参数重算）。
 * - **对称加密**：AES-256-GCM，IV 12 字节、tag 16 字节。`key` 是**恰好 32 字节的原始二进制**
 *   （由 `std::string` 承载，以 `size()` 为准），不接受 hex 字符串 —— 否则 `size() == 64`
 *   的二进制键会被误判，语义不唯一。
 * - **文件容器**：`encrypt_file` / `decrypt_file` 的签名不带 iv / tag，因此自定义
 *   40 字节定长头（magic `PRISMENC` + 版本 + IV + tag），密文流式追加，不整文件读入内存。
 * - 私有区**不持有 OpenSSL 对象**：每次调用新建 `EVP_CIPHER_CTX` 并在同一次调用内释放。
 */
class OpenSSLCryptoAdapter : public CryptoAdapter {
  public:
    OpenSSLCryptoAdapter() = default;
    ~OpenSSLCryptoAdapter() override = default;

    HashResult hash_password(const std::string& password) override;
    bool verify_password(const std::string& password, const std::string& hash,
                         const std::string& salt) override;

    bool encrypt_file(const std::string& input_path, const std::string& output_path,
                      const std::string& key) override;
    bool decrypt_file(const std::string& input_path, const std::string& output_path,
                      const std::string& key) override;

    EncryptResult encrypt_buffer(const uint8_t* data, size_t size, const std::string& key) override;
    std::vector<uint8_t> decrypt_buffer(const uint8_t* data, size_t size, const std::string& key,
                                        const std::vector<uint8_t>& iv,
                                        const std::vector<uint8_t>& tag) override;
};

} // namespace Prism::Adapter
