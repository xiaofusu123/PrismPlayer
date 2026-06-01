#pragma once
#include <string>
#include <cstdint>

namespace Prism::Persistence {

/**
 * @struct AccountInfo
 * @brief 账号信息结构体（项目要求全字段）
 *
 * 成员说明：
 * - id: 数据库主键ID（自增）
 * - username: 账号昵称/登录账号
 * - email: 绑定邮箱（用于找回密码）
 * - password_hash: 密码哈希值（不存储明文，保障安全）
 * - create_time: 账号创建时间戳（单位：秒）
 */
struct AccountInfo {
    int64_t id = 0;
    std::string username;
    std::string email;
    std::string password_hash;
    uint64_t create_time = 0;
};

/**
 * @class AccountDatabase
 * @brief 账号数据库抽象接口
 *
 * 负责账号信息的持久化存储，客户端采用SQLite实现
 * 提供账号的新增、查询、更新、删除核心能力
 */
class AccountDatabase {
public:
    virtual ~AccountDatabase() = default;

    /**
     * @brief 初始化账号数据库，创建账号数据表
     * @param db_path SQLite数据库文件本地路径
     * @return 初始化成功返回true，失败返回false
     */
    virtual bool Init(const std::string& db_path) = 0;

    /**
     * @brief 新增账号信息到数据库
     * @param account 待新增的账号信息结构体
     * @return 新增成功返回账号主键ID，失败返回-1
     */
    virtual int64_t AddAccount(const AccountInfo& account) = 0;

    /**
     * @brief 根据账号ID查询账号信息
     * @param id 账号主键ID
     * @return 查询到的账号信息，不存在则返回空结构体
     */
    virtual AccountInfo GetAccountById(int64_t id) = 0;

    /**
     * @brief 根据账号昵称查询账号信息
     * @param username 账号昵称/登录账号
     * @return 查询到的账号信息，不存在则返回空结构体
     */
    virtual AccountInfo GetAccountByUsername(const std::string& username) = 0;

    /**
     * @brief 更新数据库中的账号信息
     * @param account 待更新的账号信息（需包含有效主键ID）
     * @return 更新成功返回true，失败返回false
     */
    virtual bool UpdateAccount(const AccountInfo& account) = 0;

    /**
     * @brief 根据账号ID删除账号信息
     * @param id 账号主键ID
     * @return 删除成功返回true，失败返回false
     */
    virtual bool DeleteAccount(int64_t id) = 0;
};

} // namespace Prism::Persistence