#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Prism::Adapter {

/**
 * @struct HashResult
 * @brief 密码哈希结果
 */
struct HashResult {
    std::string hash;  /**< 哈希值 */
    std::string salt;  /**< 随机盐值 */
};

/**
 * @struct EncryptResult
 * @brief AES 加密结果
 */
struct EncryptResult {
    std::vector<uint8_t> cipher_data;  /**< 密文数据 */
    std::vector<uint8_t> iv;           /**< 初始化向量 */
    std::vector<uint8_t> tag;          /**< GCM 认证标签 */
};

/**
 * @class CryptoAdapter
 * @brief 密码学操作抽象接口，包含密码哈希与 AES 文件加密
 */
class CryptoAdapter {
public:
    virtual ~CryptoAdapter() = default;

    /**
     * @brief 对明文密码进行哈希（SHA-256 + 随机 Salt）
     * @param password 明文密码
     * @return HashResult 哈希与盐值
     */
    virtual HashResult hash_password(const std::string& password) = 0;

    /**
     * @brief 验证密码是否匹配哈希值
     * @param password 明文密码
     * @param hash 已存储的哈希值
     * @param salt 已存储的盐值
     * @return 密码匹配返回 true
     */
    virtual bool verify_password(const std::string& password,
                                 const std::string& hash,
                                 const std::string& salt) = 0;

    /**
     * @brief AES-256-GCM 加密文件
     * @param input_path 明文文件路径
     * @param output_path 密文输出路径
     * @param key 加密密钥（32 字节）
     * @return 成功返回 true
     */
    virtual bool encrypt_file(const std::string& input_path,
                              const std::string& output_path,
                              const std::string& key) = 0;

    /**
     * @brief AES-256-GCM 解密文件
     * @param input_path 密文文件路径
     * @param output_path 明文输出路径
     * @param key 加密密钥（32 字节）
     * @return 成功返回 true
     */
    virtual bool decrypt_file(const std::string& input_path,
                              const std::string& output_path,
                              const std::string& key) = 0;

    /**
     * @brief AES-256-GCM 加密内存数据
     * @param data 明文数据指针
     * @param size 数据大小
     * @param key 加密密钥（32 字节）
     * @return EncryptResult 加密结果（密文 + IV + Tag）
     */
    virtual EncryptResult encrypt_buffer(const uint8_t* data, size_t size, const std::string& key) = 0;

    /**
     * @brief AES-256-GCM 解密内存数据
     * @param data 密文数据指针
     * @param size 数据大小
     * @param key 加密密钥（32 字节）
     * @param iv 初始化向量
     * @param tag GCM 认证标签
     * @return 解密后的明文数据
     */
    virtual std::vector<uint8_t> decrypt_buffer(const uint8_t* data,
                                                size_t size,
                                                const std::string& key,
                                                const std::vector<uint8_t>& iv,
                                                const std::vector<uint8_t>& tag) = 0;
};

} // namespace Prism::Adapter
