#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace Prism::Server::Business {

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
     * @brief 登录
     * @param username 用户名
     * @param password 明文密码
     * @return 成功返回用户 ID，否则 nullopt
     */
    virtual std::optional<int64_t> login(const std::string& username,
                                         const std::string& password) = 0;
};

} // namespace Prism::Server::Business
