#pragma once
#include "AccountDatabaseFactory.h"

namespace Prism::Persistence {

/**
 * @class AccountDatabaseSQLiteFactory
 * @brief SQLite账号数据库专属工厂类
 *
 * 继承通用工厂，负责创建SQLite实现的账号数据库实例
 * 与引擎层专属工厂设计完全对齐
 */
class AccountDatabaseSQLiteFactory : public AccountDatabaseFactory {
public:
    /**
     * @brief 创建SQLite账号数据库实例
     * @return SQLite账号数据库实例的智能指针
     */
    std::unique_ptr<AccountDatabase> CreateAccountDatabase() override;
};

} // namespace Prism::Persistence