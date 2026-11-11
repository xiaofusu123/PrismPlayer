#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Prism::Adapter {

/**
 * @enum CipherSuite
 * @brief `encrypt_buffer()` / `decrypt_buffer()` 可用的 AEAD 套件
 *
 * 两个套件都是 256-bit 密钥、96-bit nonce、128-bit 认证标签，因此 `EncryptResult` 的
 * 形状与长度一致；解密必须与加密用同一套件，跨套件必然认证失败（不是「解出乱码」）。
 */
enum class CipherSuite {
    AES_256_GCM,      /**< AES-256-GCM */
    CHACHA20_POLY1305 /**< ChaCha20-Poly1305 */
};

/**
 * @struct HashResult
 * @brief 密码哈希结果
 *
 * `credential` 是落库与上协议用的自描述串 `sha256$<盐 hex>$<摘要 hex>`；`hash` / `salt`
 * 是同一份数据的两个分量（小写十六进制），供需要分列存储的场景使用。三者始终同源，
 * 由 `hash_password()` 一次产出，调用方不应自行拼装。
 */
struct HashResult {
    std::string hash;       /**< 摘要，小写十六进制 */
    std::string salt;       /**< 盐，小写十六进制 */
    std::string credential; /**< 自描述凭据串 `sha256$<盐 hex>$<摘要 hex>` */
};

/**
 * @struct EncryptResult
 * @brief AEAD 加密结果
 *
 * `iv` 是 12 字节 nonce，`tag` 是 16 字节认证标签；两者都必须与密文一起交给解密方。
 */
struct EncryptResult {
    std::vector<uint8_t> cipher_data; /**< 密文数据 */
    std::vector<uint8_t> iv;          /**< 12 字节 nonce */
    std::vector<uint8_t> tag;         /**< 16 字节认证标签 */
};

/**
 * @class CryptoAdapter
 * @brief 密码学操作抽象接口，包含口令哈希与 AEAD 内存加解密
 *
 * @note 文件级加密（分块 AEAD、临时文件 + 原子 rename）不在此接口内，见 ADR 0007 与
 * `FileCipher`（#166）。
 */
class CryptoAdapter {
  public:
    virtual ~CryptoAdapter() = default;

    /**
     * @brief 对明文口令做加盐哈希
     * @param password 明文口令
     * @return HashResult 摘要、盐与自描述凭据串
     */
    virtual HashResult hash_password(const std::string& password) = 0;

    /**
     * @brief 校验口令与已存储的自描述凭据串是否匹配
     * @param password 明文口令
     * @param credential 已存储的凭据串，格式 `sha256$<盐 hex>$<摘要 hex>`
     * @return 匹配返回 true；凭据串格式非法一律返回 false
     */
    virtual bool verify_password(const std::string& password, const std::string& credential) = 0;

    /**
     * @brief AEAD 加密内存数据
     * @param data 明文数据指针
     * @param size 数据大小
     * @param key 加密密钥（32 字节）
     * @param suite AEAD 套件
     * @return EncryptResult 加密结果（密文 + nonce + 认证标签）；失败返回全空结果
     *
     * 套件不给默认值：算法选择必须写在调用处，读代码时不用回头查默认值是哪一种。
     */
    virtual EncryptResult encrypt_buffer(const uint8_t* data, size_t size, const std::string& key,
                                         CipherSuite suite) = 0;

    /**
     * @brief AEAD 解密内存数据
     * @param data 密文数据指针
     * @param size 数据大小
     * @param key 加密密钥（32 字节）
     * @param iv 12 字节 nonce
     * @param tag 16 字节认证标签
     * @param suite 加密时使用的套件
     * @return 解密后的明文；认证失败或参数非法返回空向量
     */
    virtual std::vector<uint8_t> decrypt_buffer(const uint8_t* data, size_t size,
                                                const std::string& key,
                                                const std::vector<uint8_t>& iv,
                                                const std::vector<uint8_t>& tag,
                                                CipherSuite suite) = 0;
};

} // namespace Prism::Adapter
