#pragma once
#include "../include/AccountDatabase.h"
#include <sqlite3.h>

namespace Prism::Persistence {

/**
 * @class AccountDatabaseSQLite
 * @brief 账号数据库SQLite实现类
 *
 * 基于SQLite3实现账号数据库的增删改查功能
 * 封装数据库连接、SQL执行、资源释放逻辑
 */
class AccountDatabaseSQLite final : public AccountDatabase {
private:
    sqlite3* db_ = nullptr; /**< SQLite数据库连接句柄 */

    /**
     * @brief 内部辅助方法：执行SQL语句
     * @param sql 待执行的SQL语句字符串
     * @return 执行成功返回true，失败返回false
     */
    bool ExecSql(const std::string& sql);

public:
    /**
     * @brief 析构函数：关闭数据库连接、释放资源
     */
    ~AccountDatabaseSQLite() override;

    /**
     * @brief 初始化SQLite账号数据库
     */
    bool Init(const std::string& db_path) override;

    /**
     * @brief 新增账号信息（SQLite实现）
     */
    int64_t AddAccount(const AccountInfo& account) override;

    /**
     * @brief 根据ID查询账号信息（SQLite实现）
     */
    AccountInfo GetAccountById(int64_t id) override;

    /**
     * @brief 根据昵称查询账号信息（SQLite实现）
     */
    AccountInfo GetAccountByUsername(const std::string& username) override;

    /**
     * @brief 更新账号信息（SQLite实现）
     */
    bool UpdateAccount(const AccountInfo& account) override;

    /**
     * @brief 删除账号信息（SQLite实现）
     */
    bool DeleteAccount(int64_t id) override;
};

} // namespace Prism::Persistence