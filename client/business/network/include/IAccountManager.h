#pragma once

#include "INetworkObserver.h"

namespace Prism::Business {

/**
 * @class IAccountManager
 * @brief 账号认证管理抽象接口
 *
 * 负责用户登录/登出及登录状态查询。
 * 登录为异步操作，结果通过 INetworkObserver::on_login_result() 回调。
 */
class IAccountManager {
public:
    virtual ~IAccountManager() = default;

    /**
     * @brief 发起异步登录请求
     * @param username 用户名
     * @param password 密码
     * @param server_url 服务器地址
     * @return true 请求已发出
     */
    virtual bool login(const char* username, const char* password,
                       const char* server_url) = 0;

    /**
     * @brief 登出并断开与服务器的连接
     * @return true 操作成功
     */
    virtual bool logout() = 0;

    /**
     * @brief 查询当前登录状态
     * @return true 已登录
     */
    virtual bool is_logged_in() const = 0;

    /**
     * @brief 注册事件观察者
     * @param observer 观察者指针（不持有所有权）
     */
    virtual void set_observer(INetworkObserver* observer) = 0;
};

} // namespace Prism::Business
