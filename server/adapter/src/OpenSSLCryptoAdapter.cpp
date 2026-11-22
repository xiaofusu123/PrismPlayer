#include "OpenSSLCryptoAdapter.h"

#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include <string>
#include <vector>

namespace Prism::Server::Adapter {

namespace {

constexpr int SALT_SIZE = 16;

std::string to_hex(const unsigned char* data, size_t size) {
    static const char HEX[] = "0123456789abcdef";
    std::string result;
    result.reserve(size * 2);
    for (size_t i = 0; i < size; ++i) {
        result.push_back(HEX[data[i] >> 4]);
        result.push_back(HEX[data[i] & 0x0f]);
    }
    return result;
}

std::vector<unsigned char> from_hex(const std::string& hex) {
    auto nibble = [](char c) -> unsigned char {
        if (c >= '0' && c <= '9') {
            return static_cast<unsigned char>(c - '0');
        }
        if (c >= 'a' && c <= 'f') {
            return static_cast<unsigned char>(c - 'a' + 10);
        }
        if (c >= 'A' && c <= 'F') {
            return static_cast<unsigned char>(c - 'A' + 10);
        }
        return 0;
    };
    std::vector<unsigned char> result;
    result.reserve(hex.size() / 2);
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        result.push_back(static_cast<unsigned char>((nibble(hex[i]) << 4) | nibble(hex[i + 1])));
    }
    return result;
}

} // namespace

HashResult OpenSSLCryptoAdapter::hash_password(const std::string& password) {
    unsigned char salt[SALT_SIZE] = {};
    if (RAND_bytes(salt, SALT_SIZE) != 1) {
        return {};
    }

    std::string salted(reinterpret_cast<char*>(salt), SALT_SIZE);
    salted += password;

    unsigned char digest[SHA256_DIGEST_LENGTH] = {};
    SHA256(reinterpret_cast<const unsigned char*>(salted.data()), salted.size(), digest);

    HashResult result;
    result.salt = to_hex(salt, SALT_SIZE);
    result.hash = to_hex(digest, SHA256_DIGEST_LENGTH);
    return result;
}

bool OpenSSLCryptoAdapter::verify_password(const std::string& password, const std::string& hash,
                                           const std::string& salt) {
    std::vector<unsigned char> salt_bytes = from_hex(salt);
    std::string salted(reinterpret_cast<char*>(salt_bytes.data()), salt_bytes.size());
    salted += password;

    unsigned char digest[SHA256_DIGEST_LENGTH] = {};
    SHA256(reinterpret_cast<const unsigned char*>(salted.data()), salted.size(), digest);

    std::string computed = to_hex(digest, SHA256_DIGEST_LENGTH);
    if (computed.size() != hash.size()) {
        return false;
    }
    return CRYPTO_memcmp(computed.data(), hash.data(), computed.size()) == 0;
}

} // namespace Prism::Server::Adapter
