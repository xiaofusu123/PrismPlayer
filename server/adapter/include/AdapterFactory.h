#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace Prism::Server::Adapter {

class DBAdapter;
class CryptoAdapter;

/**
 * @enum DbBackend
 * @brief 数据库后端类型
 *
 * - MEMORY: 内存实现（缺省）
 * - MYSQL: MySQL 实现
 */
enum class DbBackend {
    MEMORY,
    MYSQL
};

/**
 * @struct MySqlConfig
 * @brief MySQL 连接配置
 */
struct MySqlConfig {
    std::string host{"127.0.0.1"};      /**< 主机地址 */
    uint16_t port{3306};                /**< 端口 */
    std::string user{"prism"};          /**< 用户名 */
    std::string password;               /**< 密码 */
    std::string database{"prism_player"}; /**< 数据库名 */
};

/**
 * @brief 创建数据库适配器实例（缺省为内存实现）
 *
 * 显式选择、缺省内存、不自动回退：后端由调用方显式指定；未指定时返回内存实现；
 * 指定 MySQL 但连接失败时不会回退到内存（由调用方决定如何处理连接失败）。
 *
 * @return DBAdapter 实例
 */
std::unique_ptr<DBAdapter> create_db_adapter();

/**
 * @brief 按后端类型创建数据库适配器实例
 * @param backend 后端类型
 * @param config MySQL 连接配置（仅 MYSQL 后端使用）
 * @return DBAdapter 实例
 */
std::unique_ptr<DBAdapter> create_db_adapter(DbBackend backend,
                                             const MySqlConfig& config = {});

/**
 * @brief 创建密码学适配器实例（OpenSSL 实现）
 * @return CryptoAdapter 实例
 */
std::unique_ptr<CryptoAdapter> create_crypto_adapter();

} // namespace Prism::Server::Adapter
