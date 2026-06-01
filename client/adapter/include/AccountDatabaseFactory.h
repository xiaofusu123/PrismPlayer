#pragma once
#include <memory>
#include "AccountDatabase.h"

namespace Prism::Persistence {

/**
 * @class AccountDatabaseFactory
 * @brief 账号数据库通用工厂类
 *
 * 遵循引擎层工厂设计模式，负责创建账号数据库实例
 * 上层模块通过该工厂获取数据库实例，解耦实现细节
 */
class AccountDatabaseFactory {
public:
    virtual ~AccountDatabaseFactory() = default;

    /**
     * @brief 创建账号数据库实例
     * @return 账号数据库实例的智能指针
     */
    virtual std::unique_ptr<AccountDatabase> CreateAccountDatabase() = 0;
};

} // namespace Prism::Persistence