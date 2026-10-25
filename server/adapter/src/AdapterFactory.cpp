#include "AdapterFactory.h"

#include "InMemoryDbAdapter.h"
#include "OpenSSLCryptoAdapter.h"

namespace Prism::Server::Adapter {

std::unique_ptr<DBAdapter> create_db_adapter() {
    return std::make_unique<InMemoryDbAdapter>();
}

std::unique_ptr<CryptoAdapter> create_crypto_adapter() {
    return std::make_unique<OpenSSLCryptoAdapter>();
}

} // namespace Prism::Server::Adapter
