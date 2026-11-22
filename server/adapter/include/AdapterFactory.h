#pragma once

#include <memory>

namespace Prism::Server::Adapter {

class DBAdapter;
class CryptoAdapter;

/**
 * @brief 创建数据库适配器实例（内存实现）
 * @return DBAdapter 实例
 */
std::unique_ptr<DBAdapter> create_db_adapter();

/**
 * @brief 创建密码学适配器实例（OpenSSL 实现）
 * @return CryptoAdapter 实例
 */
std::unique_ptr<CryptoAdapter> create_crypto_adapter();

} // namespace Prism::Server::Adapter
