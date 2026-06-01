#include "../include/AccountDatabaseSQLiteFactory.h"
#include "../Impl/AccountDatabaseSQLite.h"

namespace Prism::Persistence {

/**
 * @brief 创建SQLite账号数据库实例
 * @return SQLite账号数据库实例的智能指针
 */
std::unique_ptr<AccountDatabase> AccountDatabaseSQLiteFactory::CreateAccountDatabase() {
    return std::make_unique<AccountDatabaseSQLite>();
}

} // namespace Prism::Persistence