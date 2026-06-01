#include "../Impl/AccountDatabaseSQLite.h"
#include <spdlog/spdlog.h>
#include <ctime>

namespace Prism::Persistence {

/**
 * @brief 内部辅助方法：执行SQL语句
 * @param sql 待执行的SQL语句
 * @return 执行成功返回true，失败返回false
 */
bool AccountDatabaseSQLite::ExecSql(const std::string& sql) {
    char* err = nullptr;
    int ret = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err);
    if (ret != SQLITE_OK) {
        spdlog::error("SQL错误: {}", err);
        sqlite3_free(err);
        return false;
    }
    return true;
}

/**
 * @brief 析构函数：关闭数据库连接、释放资源
 */
AccountDatabaseSQLite::~AccountDatabaseSQLite() {
    if (db_) sqlite3_close(db_);
}

/**
 * @brief 初始化SQLite账号数据库，创建账号表
 * @param db_path SQLite数据库文件路径
 * @return 初始化成功返回true，失败返回false
 */
bool AccountDatabaseSQLite::Init(const std::string& db_path) {
    if (sqlite3_open(db_path.c_str(), &db_) != SQLITE_OK)
        return false;

    const std::string sql = R"(
        CREATE TABLE IF NOT EXISTS accounts (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            email TEXT UNIQUE NOT NULL,
            password_hash TEXT NOT NULL,
            create_time INTEGER NOT NULL
        );
    )";
    return ExecSql(sql);
}

/**
 * @brief 新增账号信息到SQLite数据库
 * @param account 待新增的账号信息结构体
 * @return 新增成功返回账号主键ID，失败返回-1
 */
int64_t AccountDatabaseSQLite::AddAccount(const AccountInfo& account) {
    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO accounts VALUES(NULL,?,?,?,?);";
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);

    sqlite3_bind_text(stmt,1,account.username.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,2,account.email.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,3,account.password_hash.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt,4,time(nullptr));

    sqlite3_step(stmt);
    int64_t id = sqlite3_last_insert_rowid(db_);
    sqlite3_finalize(stmt);
    return id;
}

/**
 * @brief 根据账号ID查询账号信息
 * @param id 账号主键ID
 * @return 查询到的账号信息，不存在则返回空结构体
 */
AccountInfo AccountDatabaseSQLite::GetAccountById(int64_t id) {
    AccountInfo info{};
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, "SELECT * FROM accounts WHERE id=?", -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt,1,id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        info.id = sqlite3_column_int64(stmt,0);
        info.username = (const char*)sqlite3_column_text(stmt,1);
        info.email = (const char*)sqlite3_column_text(stmt,2);
        info.password_hash = (const char*)sqlite3_column_text(stmt,3);
        info.create_time = sqlite3_column_int64(stmt,4);
    }
    sqlite3_finalize(stmt);
    return info;
}

/**
 * @brief 根据账号昵称查询账号信息
 * @param username 账号昵称/登录账号
 * @return 查询到的账号信息，不存在则返回空结构体
 */
AccountInfo AccountDatabaseSQLite::GetAccountByUsername(const std::string& username) {
    AccountInfo info{};
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, "SELECT * FROM accounts WHERE username=?", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt,1,username.c_str(),-1,SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        info.id = sqlite3_column_int64(stmt,0);
        info.username = (const char*)sqlite3_column_text(stmt,1);
        info.email = (const char*)sqlite3_column_text(stmt,2);
        info.password_hash = (const char*)sqlite3_column_text(stmt,3);
        info.create_time = sqlite3_column_int64(stmt,4);
    }
    sqlite3_finalize(stmt);
    return info;
}

/**
 * @brief 更新SQLite数据库中的账号信息
 * @param account 待更新的账号信息（需包含有效主键ID）
 * @return 更新成功返回true，失败返回false
 */
bool AccountDatabaseSQLite::UpdateAccount(const AccountInfo& account) {
    sqlite3_stmt* stmt;
    const char* sql = "UPDATE accounts SET username=?,email=?,password_hash=? WHERE id=?";
    sqlite3_prepare_v2(db_,sql,-1,&stmt,nullptr);

    sqlite3_bind_text(stmt,1,account.username.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,2,account.email.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,3,account.password_hash.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt,4,account.id);

    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

/**
 * @brief 根据账号ID删除账号信息
 * @param id 账号主键ID
 * @return 删除成功返回true，失败返回false
 */
bool AccountDatabaseSQLite::DeleteAccount(int64_t id) {
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, "DELETE FROM accounts WHERE id=?", -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt,1,id);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

} // namespace Prism::Persistence