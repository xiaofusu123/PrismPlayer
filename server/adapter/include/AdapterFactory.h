#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace Prism::Server::Adapter {

class DBAdapter;
class CryptoAdapter;

/**
 * @struct MySqlConfig
 * @brief MySQL 连接配置
 */
struct MySqlConfig {
    std::string host;      /**< 主机地址 */
    uint16_t port{3306};   /**< 端口 */
    std::string user;      /**< 用户名 */
    std::string password;  /**< 密码 */
    std::string database;  /**< 数据库名 */
};

/**
 * @brief 创建数据库适配器实例（内存实现）
 * @return DBAdapter 实例
 */
std::unique_ptr<DBAdapter> create_db_adapter();

/**
 * @brief 创建 MySQL 数据库适配器实例
 *
 * 仅在编译期定义 PRISM_SERVER_WITH_MYSQL（CMake 选项 PRISM_SERVER_WITH_MYSQL=ON）时
 * 返回真实实现；否则返回 nullptr，由调用方回退到 create_db_adapter()。
 * @param config MySQL 连接配置
 * @return DBAdapter 实例，未启用 MySQL 时返回 nullptr
 */
std::unique_ptr<DBAdapter> create_mysql_db_adapter(const MySqlConfig& config);

/**
 * @brief 创建密码学适配器实例（OpenSSL 实现）
 * @return CryptoAdapter 实例
 */
std::unique_ptr<CryptoAdapter> create_crypto_adapter();

} // namespace Prism::Server::Adapter
