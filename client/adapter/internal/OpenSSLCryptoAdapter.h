#pragma once

#include "CryptoAdapter.h"

namespace Prism::Adapter {

/**
 * @class OpenSSLCryptoAdapter
 * @brief 基于 OpenSSL 的密码学操作实现
 *
 * **密码哈希**：`SHA-256(salt || password)`，16 字节随机盐（`RAND_bytes`），盐与摘要
 * 都以小写十六进制字符串返回 —— 与 `server/adapter` 的同名实现用同一算法与同一编码，
 * 客户端的哈希可以由服务端直接 `verify_password()`，反之亦然。
 *
 * **AES-256-GCM**：密钥必须是 32 字节（接口契约），IV 12 字节、认证标签 16 字节，
 * 均随机生成。`encrypt_file()` 的落盘格式为 `[12B IV][密文][16B tag]`，顺序固定的
 * 原因是 GCM 的 tag 只能在明文全部处理完之后拿到，写在尾部才不必把整个文件读进内存。
 *
 * @note 本类无可变状态，所有方法可并发调用；`EVP_CIPHER_CTX` 每次调用各自新建。
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
