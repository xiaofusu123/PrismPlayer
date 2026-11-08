#include "AdapterFactory.h"

#include "InMemoryDbAdapter.h"
#include "OpenSSLCryptoAdapter.h"

#ifdef PRISM_SERVER_WITH_MYSQL
#include "MySqlDbAdapter.h"
#endif

namespace Prism::Server::Adapter {

std::unique_ptr<DBAdapter> create_db_adapter() {
    return std::make_unique<InMemoryDbAdapter>();
}

std::unique_ptr<DBAdapter> create_mysql_db_adapter(const MySqlConfig& config) {
#ifdef PRISM_SERVER_WITH_MYSQL
    return std::make_unique<MySqlDbAdapter>(config);
#else
    (void)config;
    return nullptr;
#endif
}

std::unique_ptr<CryptoAdapter> create_crypto_adapter() {
    return std::make_unique<OpenSSLCryptoAdapter>();
}

} // namespace Prism::Server::Adapter
