#include "AdapterFactory.h"

#include "InMemoryDbAdapter.h"
#include "MySqlDbAdapter.h"
#include "OpenSSLCryptoAdapter.h"

namespace Prism::Server::Adapter {

std::unique_ptr<DBAdapter> create_db_adapter() {
    return std::make_unique<InMemoryDbAdapter>();
}

std::unique_ptr<DBAdapter> create_db_adapter(DbBackend backend, const MySqlConfig& config) {
    switch (backend) {
    case DbBackend::MYSQL:
        return std::make_unique<MySqlDbAdapter>(config);
    case DbBackend::MEMORY:
    default:
        return std::make_unique<InMemoryDbAdapter>();
    }
}

std::unique_ptr<CryptoAdapter> create_crypto_adapter() {
    return std::make_unique<OpenSSLCryptoAdapter>();
}

} // namespace Prism::Server::Adapter
