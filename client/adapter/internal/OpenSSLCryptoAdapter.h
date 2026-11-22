#pragma once

#include "CryptoAdapter.h"

namespace Prism::Adapter {

/**
 * @class OpenSSLCryptoAdapter
 * @brief 基于 OpenSSL 的密码学操作实现
 *
 * **口令哈希**：`SHA-256(salt || password)`，16 字节随机盐（`RAND_bytes`），盐与摘要
 * 都以小写十六进制表示，并拼成自描述串 `sha256$<盐 hex>$<摘要 hex>` —— 与
 * `server/adapter` 的同名实现用同一算法与同一编码：把凭据串拆回盐与摘要两段即可交给
 * 服务端按同一口径重算校验，一致性由 #122 的跨侧夹具钉住。
 *
 * **AEAD 内存加解密**：密钥必须是 32 字节（接口契约），nonce 12 字节、认证标签 16 字节，
 * nonce 随机生成。支持 AES-256-GCM 与 ChaCha20-Poly1305 两个套件；套件在 OpenSSL 构建
 * 里不可用时直接失败，不做静默降级。
 *
 * @note 文件级加密不在本类，见 ADR 0007 与 `FileCipher`（#166）。
 * @note 本类无可变状态，所有方法可并发调用；`EVP_CIPHER_CTX` 每次调用各自新建。
 */
class OpenSSLCryptoAdapter : public CryptoAdapter {
  public:
    OpenSSLCryptoAdapter() = default;
    ~OpenSSLCryptoAdapter() override = default;

    HashResult hash_password(const std::string& password) override;
    bool verify_password(const std::string& password, const std::string& credential) override;

    EncryptResult encrypt_buffer(const uint8_t* data, size_t size, const std::string& key,
                                 CipherSuite suite) override;
    std::vector<uint8_t> decrypt_buffer(const uint8_t* data, size_t size, const std::string& key,
                                        const std::vector<uint8_t>& iv,
                                        const std::vector<uint8_t>& tag,
                                        CipherSuite suite) override;
};

} // namespace Prism::Adapter
