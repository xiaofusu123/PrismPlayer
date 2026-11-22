#include "OpenSSLCryptoAdapter.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

namespace Prism::Adapter {
namespace {

constexpr size_t SALT_SIZE = 16;
constexpr size_t KEY_SIZE = 32;
constexpr size_t IV_SIZE = 12;
constexpr size_t TAG_SIZE = 16;

/** @brief 自描述凭据串的算法前缀，格式为 `<前缀>$<盐 hex>$<摘要 hex>` */
constexpr const char* CREDENTIAL_ALGORITHM = "sha256";

/** @brief `EVP_CIPHER_CTX` 的 RAII 包装 */
using CipherCtxPtr = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;

[[nodiscard]] CipherCtxPtr make_cipher_ctx() {
    return CipherCtxPtr(EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);
}

/**
 * @brief 把字节序列编成小写十六进制
 * @param data 字节序列
 * @param size 字节数
 * @return 长度为 `size * 2` 的十六进制串
 */
std::string to_hex(const uint8_t* data, size_t size) {
    static constexpr char HEX_DIGITS[] = "0123456789abcdef";
    std::string hex;
    hex.reserve(size * 2);
    for (size_t i = 0; i < size; ++i) {
        hex.push_back(HEX_DIGITS[data[i] >> 4]);
        hex.push_back(HEX_DIGITS[data[i] & 0x0f]);
    }
    return hex;
}

/**
 * @brief 解析十六进制串，遇到非法字符或奇数长度即失败
 * @param hex 十六进制串
 * @param out 解析结果
 * @return 解析成功返回 true
 *
 * 与「尽力解析」不同，这里严格校验：盐/摘要来自数据库，格式不对说明数据已经坏了，
 * 静默按 0 处理只会把错误藏起来。
 */
[[nodiscard]] bool from_hex(const std::string& hex, std::vector<uint8_t>& out) {
    if (hex.size() % 2 != 0) {
        return false;
    }

    out.clear();
    out.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        uint8_t value = 0;
        for (size_t j = 0; j < 2; ++j) {
            char ch = hex[i + j];
            uint8_t nibble = 0;
            if (ch >= '0' && ch <= '9') {
                nibble = static_cast<uint8_t>(ch - '0');
            } else if (ch >= 'a' && ch <= 'f') {
                nibble = static_cast<uint8_t>(ch - 'a' + 10);
            } else if (ch >= 'A' && ch <= 'F') {
                nibble = static_cast<uint8_t>(ch - 'A' + 10);
            } else {
                return false;
            }
            value = static_cast<uint8_t>((value << 4) | nibble);
        }
        out.push_back(value);
    }
    return true;
}

/**
 * @brief 校验并取出 32 字节 AEAD 密钥
 * @param key 接口传入的密钥字符串
 * @param out 32 字节密钥
 * @return 长度正好 32 字节返回 true
 */
[[nodiscard]] bool aead_key_from(const std::string& key, std::array<uint8_t, KEY_SIZE>& out) {
    if (key.size() != KEY_SIZE) {
        spdlog::error("[CryptoAdapter] AEAD key must be {} bytes, got {}", KEY_SIZE, key.size());
        return false;
    }

    std::copy(key.begin(), key.end(), out.begin());
    return true;
}

/**
 * @brief 对 `salt || password` 求 SHA-256
 * @param salt 盐值
 * @param password 明文密码
 * @param digest 32 字节摘要
 * @return 成功返回 true
 */
[[nodiscard]] bool digest_salted(const std::vector<uint8_t>& salt, const std::string& password,
                                 std::array<uint8_t, 32>& digest) {
    // 盐在前、密码在后，与 server/adapter 的实现保持一致，两端哈希可互相校验。
    std::string salted(reinterpret_cast<const char*>(salt.data()), salt.size());
    salted += password;

    unsigned int length = 0;
    if (EVP_Digest(salted.data(), salted.size(), digest.data(), &length, EVP_sha256(), nullptr) !=
        1) {
        spdlog::error("[CryptoAdapter] SHA-256 failed");
        return false;
    }
    return length == digest.size();
}

/**
 * @brief 按套件取出 OpenSSL 的 `EVP_CIPHER` 描述
 * @param suite AEAD 套件
 * @return 该套件在当前 OpenSSL 构建里可用则返回描述符，否则返回 nullptr
 *
 * `EVP_chacha20_poly1305()` 在构建时未启用该算法会返回 nullptr —— 这就是「运行时探测」：
 * 不假定库里一定有两个套件，缺了就明确失败，不做静默降级。
 */
[[nodiscard]] const EVP_CIPHER* cipher_for(CipherSuite suite) {
    switch (suite) {
    case CipherSuite::AES_256_GCM:
        return EVP_aes_256_gcm();
    case CipherSuite::CHACHA20_POLY1305:
        return EVP_chacha20_poly1305();
    }
    return nullptr;
}

/**
 * @brief 套件的可读名字，仅用于日志
 * @param suite AEAD 套件
 * @return 名字字面量
 */
[[nodiscard]] const char* suite_name(CipherSuite suite) {
    switch (suite) {
    case CipherSuite::AES_256_GCM:
        return "AES-256-GCM";
    case CipherSuite::CHACHA20_POLY1305:
        return "ChaCha20-Poly1305";
    }
    return "unknown";
}

/**
 * @brief 以指定 AEAD 套件初始化一个 EVP 上下文
 * @param ctx 上下文
 * @param encrypt true 走加密，false 走解密
 * @param key 32 字节密钥
 * @param nonce 12 字节 nonce（GCM 文档里叫 IV）
 * @param suite AEAD 套件
 * @return 成功返回 true
 */
[[nodiscard]] bool init_aead_cipher(EVP_CIPHER_CTX* ctx, bool encrypt,
                                    const std::array<uint8_t, KEY_SIZE>& key, const uint8_t* nonce,
                                    CipherSuite suite) {
    const EVP_CIPHER* cipher = cipher_for(suite);
    if (cipher == nullptr) {
        spdlog::error("[CryptoAdapter] {} is not available in this OpenSSL build",
                      suite_name(suite));
        return false;
    }

    int ok = encrypt ? EVP_EncryptInit_ex(ctx, cipher, nullptr, nullptr, nullptr)
                     : EVP_DecryptInit_ex(ctx, cipher, nullptr, nullptr, nullptr);
    if (ok != 1) {
        return false;
    }

    // 默认 nonce 长度是 12，这里显式设置一次，换套件或改参数时不会默默沿用默认值。
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, static_cast<int>(IV_SIZE), nullptr) !=
        1) {
        return false;
    }

    ok = encrypt ? EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), nonce)
                 : EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), nonce);
    return ok == 1;
}

/**
 * @brief 把盐与摘要拼成自描述凭据串 `sha256$<盐 hex>$<摘要 hex>`
 * @param salt_hex 盐，小写十六进制
 * @param hash_hex 摘要，小写十六进制
 * @return 自描述串
 */
[[nodiscard]] std::string make_credential(const std::string& salt_hex,
                                          const std::string& hash_hex) {
    std::string credential(CREDENTIAL_ALGORITHM);
    credential.push_back('$');
    credential += salt_hex;
    credential.push_back('$');
    credential += hash_hex;
    return credential;
}

/**
 * @brief 把自描述凭据串拆回盐与摘要两段
 * @param credential 自描述串
 * @param salt_hex 输出：盐，小写十六进制
 * @param hash_hex 输出：摘要，小写十六进制
 * @return 前缀与分隔符都合法返回 true
 *
 * 严格按「三段、两个 `$`、前缀等于 `sha256`」解析；不做「尽力解析」——凭据串来自
 * 数据库，格式不对说明数据已经坏了，按 0 处理只会把错误藏起来。
 */
[[nodiscard]] bool parse_credential(const std::string& credential, std::string& salt_hex,
                                    std::string& hash_hex) {
    size_t first = credential.find('$');
    if (first == std::string::npos) {
        return false;
    }
    size_t second = credential.find('$', first + 1);
    if (second == std::string::npos || credential.find('$', second + 1) != std::string::npos) {
        return false;
    }
    if (credential.compare(0, first, CREDENTIAL_ALGORITHM) != 0) {
        return false;
    }

    salt_hex = credential.substr(first + 1, second - first - 1);
    hash_hex = credential.substr(second + 1);
    return true;
}

/**
 * @brief 把缓冲区大小限制在 EVP 的 int 入参范围内
 * @param size 字节数
 * @return 未超限返回 true
 */
[[nodiscard]] bool fits_in_int(size_t size) {
    return size <= static_cast<size_t>(std::numeric_limits<int>::max());
}

} // namespace

HashResult OpenSSLCryptoAdapter::hash_password(const std::string& password) {
    std::vector<uint8_t> salt(SALT_SIZE);
    if (RAND_bytes(salt.data(), static_cast<int>(salt.size())) != 1) {
        spdlog::error("[CryptoAdapter] cannot generate a random salt");
        return {};
    }

    std::array<uint8_t, 32> digest{};
    if (!digest_salted(salt, password, digest)) {
        return {};
    }

    HashResult result;
    result.salt = to_hex(salt.data(), salt.size());
    result.hash = to_hex(digest.data(), digest.size());
    result.credential = make_credential(result.salt, result.hash);
    return result;
}

bool OpenSSLCryptoAdapter::verify_password(const std::string& password,
                                           const std::string& credential) {
    std::string salt_hex;
    std::string hash_hex;
    if (!parse_credential(credential, salt_hex, hash_hex)) {
        spdlog::warn("[CryptoAdapter] stored credential is not a {} string",
                     "sha256$<salt>$<hash>");
        return false;
    }

    std::vector<uint8_t> salt_bytes;
    if (!from_hex(salt_hex, salt_bytes) || salt_bytes.size() != SALT_SIZE) {
        spdlog::warn("[CryptoAdapter] stored salt is not a {} byte hex string", SALT_SIZE);
        return false;
    }

    std::vector<uint8_t> expected;
    if (!from_hex(hash_hex, expected) || expected.size() != 32) {
        spdlog::warn("[CryptoAdapter] stored hash is not a 32 byte hex string");
        return false;
    }

    std::array<uint8_t, 32> digest{};
    if (!digest_salted(salt_bytes, password, digest)) {
        return false;
    }

    // 常量时间比较：普通 memcmp 会按首个不同字节的位置提前返回，泄漏匹配前缀长度。
    return CRYPTO_memcmp(digest.data(), expected.data(), expected.size()) == 0;
}

EncryptResult OpenSSLCryptoAdapter::encrypt_buffer(const uint8_t* data, size_t size,
                                                   const std::string& key, CipherSuite suite) {
    std::array<uint8_t, KEY_SIZE> aead_key{};
    if (!aead_key_from(key, aead_key)) {
        return {};
    }
    if (data == nullptr && size > 0) {
        spdlog::error("[CryptoAdapter] encrypt_buffer() called with a null buffer");
        return {};
    }
    if (!fits_in_int(size)) {
        spdlog::error("[CryptoAdapter] encrypt_buffer() size {} exceeds the EVP limit", size);
        return {};
    }

    EncryptResult result;
    result.iv.resize(IV_SIZE);
    if (RAND_bytes(result.iv.data(), static_cast<int>(result.iv.size())) != 1) {
        spdlog::error("[CryptoAdapter] cannot generate a random nonce");
        return {};
    }

    CipherCtxPtr ctx = make_cipher_ctx();
    if (!ctx || !init_aead_cipher(ctx.get(), true, aead_key, result.iv.data(), suite)) {
        spdlog::error("[CryptoAdapter] cannot initialise {}", suite_name(suite));
        return {};
    }

    // +TAG_SIZE：final 块最多再吐一个分组，先留出位置，事后按实际长度收缩。
    // 两个套件的密文长度都等于明文长度，这块余量对它们都是够的。
    result.cipher_data.resize(size + TAG_SIZE);
    int update_len = 0;
    size_t total = 0;
    if (size > 0) {
        if (EVP_EncryptUpdate(ctx.get(), result.cipher_data.data(), &update_len, data,
                              static_cast<int>(size)) != 1) {
            spdlog::error("[CryptoAdapter] {} encrypt failed", suite_name(suite));
            return {};
        }
        total = static_cast<size_t>(update_len);
    }

    int final_len = 0;
    if (EVP_EncryptFinal_ex(ctx.get(), result.cipher_data.data() + total, &final_len) != 1) {
        spdlog::error("[CryptoAdapter] {} encrypt finalisation failed", suite_name(suite));
        return {};
    }
    total += static_cast<size_t>(final_len);
    result.cipher_data.resize(total);

    result.tag.resize(TAG_SIZE);
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_GET_TAG, static_cast<int>(TAG_SIZE),
                            result.tag.data()) != 1) {
        spdlog::error("[CryptoAdapter] cannot read the {} tag", suite_name(suite));
        return {};
    }
    return result;
}

std::vector<uint8_t> OpenSSLCryptoAdapter::decrypt_buffer(const uint8_t* data, size_t size,
                                                          const std::string& key,
                                                          const std::vector<uint8_t>& iv,
                                                          const std::vector<uint8_t>& tag,
                                                          CipherSuite suite) {
    std::array<uint8_t, KEY_SIZE> aead_key{};
    if (!aead_key_from(key, aead_key)) {
        return {};
    }
    if (data == nullptr && size > 0) {
        spdlog::error("[CryptoAdapter] decrypt_buffer() called with a null buffer");
        return {};
    }
    if (iv.size() != IV_SIZE || tag.size() != TAG_SIZE) {
        spdlog::error("[CryptoAdapter] unexpected nonce ({} B) or tag ({} B) length", iv.size(),
                      tag.size());
        return {};
    }
    if (!fits_in_int(size)) {
        spdlog::error("[CryptoAdapter] decrypt_buffer() size {} exceeds the EVP limit", size);
        return {};
    }

    CipherCtxPtr ctx = make_cipher_ctx();
    if (!ctx || !init_aead_cipher(ctx.get(), false, aead_key, iv.data(), suite)) {
        spdlog::error("[CryptoAdapter] cannot initialise {}", suite_name(suite));
        return {};
    }

    std::vector<uint8_t> plain(size + TAG_SIZE);
    int update_len = 0;
    size_t total = 0;
    if (size > 0) {
        if (EVP_DecryptUpdate(ctx.get(), plain.data(), &update_len, data, static_cast<int>(size)) !=
            1) {
            spdlog::error("[CryptoAdapter] {} decrypt failed", suite_name(suite));
            return {};
        }
        total = static_cast<size_t>(update_len);
    }

    // tag 必须在 final 之前设置：final 既解密最后一个分组，也校验整段密文的认证标签。
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_TAG, static_cast<int>(TAG_SIZE),
                            const_cast<uint8_t*>(tag.data())) != 1) {
        spdlog::error("[CryptoAdapter] cannot set the {} tag", suite_name(suite));
        return {};
    }

    int final_len = 0;
    if (EVP_DecryptFinal_ex(ctx.get(), plain.data() + total, &final_len) != 1) {
        spdlog::error("[CryptoAdapter] {} authentication failed (wrong key, wrong suite or "
                      "corrupt data)",
                      suite_name(suite));
        return {};
    }
    total += static_cast<size_t>(final_len);
    plain.resize(total);
    return plain;
}

} // namespace Prism::Adapter
