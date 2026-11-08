#pragma once

#include <cstdint>
#include <string>

namespace Prism::Server::Business {

/**
 * @enum AuthError
 * @brief 认证错误码
 *
 * - NONE: 成功
 * - INVALID_CREDENTIALS: 口令错（用户名不存在或密码不匹配）
 * - NOT_AUTHENTICATED: 未认证（缺 token 或 token 无效）
 * - TOKEN_EXPIRED: token 已过期
 */
enum class AuthError {
    NONE,
    INVALID_CREDENTIALS,
    NOT_AUTHENTICATED,
    TOKEN_EXPIRED
};

/**
 * @struct AuthResult
 * @brief 认证结果（登录 / 校验 token）
 */
struct AuthResult {
    AuthError error{AuthError::NONE}; /**< 错误码，NONE 表示成功 */
    int64_t user_id{0};               /**< 用户 ID（成功时有效） */
    std::string token;                /**< 会话令牌（登录成功时返回） */
};

/**
 * @class IAuthService
 * @brief 用户认证抽象接口
 */
class IAuthService {
  public:
    virtual ~IAuthService() = default;

    /**
     * @brief 注册用户
     * @param username 用户名
     * @param password 明文密码
     * @return 成功返回 true，用户名已存在返回 false
     */
    virtual bool register_user(const std::string& username, const std::string& password) = 0;

    /**
     * @brief 登录并签发会话令牌
     * @param username 用户名
     * @param password 明文密码
     * @return 成功返回 user_id 与 token（error = NONE）；口令错返回 error = INVALID_CREDENTIALS
     */
    virtual AuthResult login(const std::string& username, const std::string& password) = 0;

    /**
     * @brief 校验会话令牌
     * @param token 会话令牌
     * @return 有效返回 user_id（error = NONE）；无效返回 NOT_AUTHENTICATED；过期返回 TOKEN_EXPIRED
     */
    virtual AuthResult verify_token(const std::string& token) = 0;

    /**
     * @brief 登出并使会话令牌失效
     * @param token 会话令牌
     */
    virtual void logout(const std::string& token) = 0;
};

} // namespace Prism::Server::Business
