#pragma once

#include <string>

namespace Prism::Server::Adapter {

/**
 * @struct HashResult
 * @brief 密码哈希结果
 */
struct HashResult {
    std::string hash; /**< 哈希值（十六进制） */
    std::string salt; /**< 随机盐（十六进制） */
};

/**
 * @class CryptoAdapter
 * @brief 密码学操作抽象接口（密码哈希）
 */
class CryptoAdapter {
  public:
    virtual ~CryptoAdapter() = default;

    /**
     * @brief 对明文密码做哈希（SHA-256 + 随机盐）
     * @param password 明文密码
     * @return HashResult 哈希与盐值
     */
    virtual HashResult hash_password(const std::string& password) = 0;

    /**
     * @brief 校验密码是否匹配
     * @param password 明文密码
     * @param hash 已存哈希（十六进制）
     * @param salt 已存盐值（十六进制）
     * @return 匹配返回 true
     */
    virtual bool verify_password(const std::string& password, const std::string& hash,
                                 const std::string& salt) = 0;
};

} // namespace Prism::Server::Adapter
