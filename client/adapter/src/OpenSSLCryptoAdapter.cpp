#include "OpenSSLCryptoAdapter.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include <spdlog/spdlog.h>

namespace Prism::Adapter {
namespace {

constexpr size_t SALT_SIZE = 16;
constexpr size_t KEY_SIZE = 32;
constexpr size_t IV_SIZE = 12;
constexpr size_t TAG_SIZE = 16;

/** @brief 密文文件的最小长度：IV + tag；再短的必然是坏文件 */
constexpr uintmax_t FILE_MIN_SIZE = IV_SIZE + TAG_SIZE;

/** @brief 流式加解密的块大小，避免把整个文件读进内存 */
constexpr size_t FILE_CHUNK_SIZE = 64 * 1024;

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
 * @brief 校验并取出 32 字节 AES 密钥
 * @param key 接口传入的密钥字符串
 * @param out 32 字节密钥
 * @return 长度正好 32 字节返回 true
 */
[[nodiscard]] bool aes_key_from(const std::string& key, std::array<uint8_t, KEY_SIZE>& out) {
    if (key.size() != KEY_SIZE) {
        spdlog::error("[CryptoAdapter] AES-256-GCM key must be {} bytes, got {}", KEY_SIZE,
                      key.size());
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
 * @brief 以 AES-256-GCM 初始化一个 EVP 上下文
 * @param ctx 上下文
 * @param encrypt true 走加密，false 走解密
 * @param key 32 字节密钥
 * @param iv 12 字节 IV
 * @return 成功返回 true
 */
[[nodiscard]] bool init_gcm_cipher(EVP_CIPHER_CTX* ctx, bool encrypt,
                                   const std::array<uint8_t, KEY_SIZE>& key, const uint8_t* iv) {
    int ok = encrypt ? EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr)
                     : EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    if (ok != 1) {
        return false;
    }

    // 默认 IV 长度是 12，这里显式设置一次，改算法参数时不会默默沿用默认值。
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(IV_SIZE), nullptr) != 1) {
        return false;
    }

    ok = encrypt ? EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), iv)
                 : EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), iv);
    return ok == 1;
}

/**
 * @brief 把缓冲区大小限制在 EVP 的 int 入参范围内
 * @param size 字节数
 * @return 未超限返回 true
 */
[[nodiscard]] bool fits_in_int(size_t size) {
    return size <= static_cast<size_t>(std::numeric_limits<int>::max());
}

/**
 * @brief 往输出流写一段字节
 * @param out 输出流
 * @param data 字节
 * @param size 字节数
 * @return 写入成功返回 true
 */
[[nodiscard]] bool write_bytes(std::ofstream& out, const uint8_t* data, size_t size) {
    out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
    return static_cast<bool>(out);
}

/**
 * @brief 把输入流的剩余内容加密后写入输出流
 * @param in 明文输入流（已定位到起点）
 * @param out 密文输出流（IV 由调用方先写）
 * @param key 32 字节密钥
 * @param iv 12 字节 IV
 * @param input_path 输入路径，仅用于日志
 * @return 成功返回 true（不含 IV / tag，两者由调用方与本文末尾各自写）
 */
[[nodiscard]] bool encrypt_stream(std::ifstream& in, std::ofstream& out,
                                  const std::array<uint8_t, KEY_SIZE>& key, const uint8_t* iv,
                                  const std::string& input_path) {
    CipherCtxPtr ctx = make_cipher_ctx();
    if (!ctx || !init_gcm_cipher(ctx.get(), true, key, iv)) {
        spdlog::error("[CryptoAdapter] cannot initialise AES-256-GCM");
        return false;
    }

    std::vector<uint8_t> plain(FILE_CHUNK_SIZE);
    std::vector<uint8_t> cipher(FILE_CHUNK_SIZE + TAG_SIZE);
    while (in) {
        in.read(reinterpret_cast<char*>(plain.data()), static_cast<std::streamsize>(plain.size()));
        std::streamsize read_bytes = in.gcount();
        if (read_bytes <= 0) {
            break;
        }

        int written = 0;
        if (EVP_EncryptUpdate(ctx.get(), cipher.data(), &written, plain.data(),
                              static_cast<int>(read_bytes)) != 1) {
            spdlog::error("[CryptoAdapter] AES-256-GCM encrypt failed on '{}'", input_path);
            return false;
        }
        if (!write_bytes(out, cipher.data(), static_cast<size_t>(written))) {
            spdlog::error("[CryptoAdapter] cannot write ciphertext for '{}'", input_path);
            return false;
        }
    }

    int final_len = 0;
    if (EVP_EncryptFinal_ex(ctx.get(), cipher.data(), &final_len) != 1) {
        spdlog::error("[CryptoAdapter] AES-256-GCM encrypt finalisation failed on '{}'",
                      input_path);
        return false;
    }
    if (!write_bytes(out, cipher.data(), static_cast<size_t>(final_len))) {
        spdlog::error("[CryptoAdapter] cannot write the final block for '{}'", input_path);
        return false;
    }

    std::array<uint8_t, TAG_SIZE> tag{};
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, static_cast<int>(tag.size()),
                            tag.data()) != 1) {
        spdlog::error("[CryptoAdapter] cannot read the GCM tag for '{}'", input_path);
        return false;
    }
    return write_bytes(out, tag.data(), tag.size());
}

/**
 * @brief 解密输入流中从当前位置起的若干字节，写入输出流
 * @param in 密文输入流（已定位到密文起点）
 * @param out 明文输出流
 * @param cipher_length 要读取的密文字节数
 * @param key 32 字节密钥
 * @param iv 12 字节 IV
 * @param tag 16 字节认证标签
 * @param input_path 输入路径，仅用于日志
 * @return 认证通过且写入成功返回 true
 */
[[nodiscard]] bool decrypt_stream(std::ifstream& in, std::ofstream& out, uintmax_t cipher_length,
                                  const std::array<uint8_t, KEY_SIZE>& key,
                                  const std::array<uint8_t, IV_SIZE>& iv,
                                  const std::array<uint8_t, TAG_SIZE>& tag,
                                  const std::string& input_path) {
    CipherCtxPtr ctx = make_cipher_ctx();
    if (!ctx || !init_gcm_cipher(ctx.get(), false, key, iv.data())) {
        spdlog::error("[CryptoAdapter] cannot initialise AES-256-GCM");
        return false;
    }

    std::vector<uint8_t> cipher(FILE_CHUNK_SIZE);
    std::vector<uint8_t> plain(FILE_CHUNK_SIZE + TAG_SIZE);
    uintmax_t remaining = cipher_length;
    while (remaining > 0) {
        auto want = static_cast<size_t>(
            std::min<uintmax_t>(remaining, static_cast<uintmax_t>(cipher.size())));
        in.read(reinterpret_cast<char*>(cipher.data()), static_cast<std::streamsize>(want));
        if (in.gcount() != static_cast<std::streamsize>(want)) {
            spdlog::error("[CryptoAdapter] unexpected end of '{}'", input_path);
            return false;
        }
        remaining -= want;

        int written = 0;
        if (EVP_DecryptUpdate(ctx.get(), plain.data(), &written, cipher.data(),
                              static_cast<int>(want)) != 1) {
            spdlog::error("[CryptoAdapter] AES-256-GCM decrypt failed on '{}'", input_path);
            return false;
        }
        if (!write_bytes(out, plain.data(), static_cast<size_t>(written))) {
            spdlog::error("[CryptoAdapter] cannot write plaintext for '{}'", input_path);
            return false;
        }
    }

    // tag 必须在 final 之前设置：final 既解密最后一个分组，也校验整段密文的认证标签。
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, static_cast<int>(tag.size()),
                            const_cast<uint8_t*>(tag.data())) != 1) {
        spdlog::error("[CryptoAdapter] cannot set the GCM tag for '{}'", input_path);
        return false;
    }

    int final_len = 0;
    if (EVP_DecryptFinal_ex(ctx.get(), plain.data(), &final_len) != 1) {
        spdlog::error("[CryptoAdapter] AES-256-GCM authentication failed on '{}' "
                      "(wrong key or corrupt data)",
                      input_path);
        return false;
    }
    return write_bytes(out, plain.data(), static_cast<size_t>(final_len));
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
    return result;
}

bool OpenSSLCryptoAdapter::verify_password(const std::string& password, const std::string& hash,
                                           const std::string& salt) {
    std::vector<uint8_t> salt_bytes;
    if (!from_hex(salt, salt_bytes) || salt_bytes.size() != SALT_SIZE) {
        spdlog::warn("[CryptoAdapter] stored salt is not a {} byte hex string", SALT_SIZE);
        return false;
    }

    std::vector<uint8_t> expected;
    if (!from_hex(hash, expected) || expected.size() != 32) {
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
                                                   const std::string& key) {
    std::array<uint8_t, KEY_SIZE> aes_key{};
    if (!aes_key_from(key, aes_key)) {
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
        spdlog::error("[CryptoAdapter] cannot generate a random IV");
        return {};
    }

    CipherCtxPtr ctx = make_cipher_ctx();
    if (!ctx || !init_gcm_cipher(ctx.get(), true, aes_key, result.iv.data())) {
        spdlog::error("[CryptoAdapter] cannot initialise AES-256-GCM");
        return {};
    }

    // +TAG_SIZE：GCM 的 final 块最多再吐一个分组，先留出位置，事后按实际长度收缩。
    result.cipher_data.resize(size + TAG_SIZE);
    int update_len = 0;
    size_t total = 0;
    if (size > 0) {
        if (EVP_EncryptUpdate(ctx.get(), result.cipher_data.data(), &update_len, data,
                              static_cast<int>(size)) != 1) {
            spdlog::error("[CryptoAdapter] AES-256-GCM encrypt failed");
            return {};
        }
        total = static_cast<size_t>(update_len);
    }

    int final_len = 0;
    if (EVP_EncryptFinal_ex(ctx.get(), result.cipher_data.data() + total, &final_len) != 1) {
        spdlog::error("[CryptoAdapter] AES-256-GCM encrypt finalisation failed");
        return {};
    }
    total += static_cast<size_t>(final_len);
    result.cipher_data.resize(total);

    result.tag.resize(TAG_SIZE);
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, static_cast<int>(TAG_SIZE),
                            result.tag.data()) != 1) {
        spdlog::error("[CryptoAdapter] cannot read the GCM tag");
        return {};
    }
    return result;
}

std::vector<uint8_t> OpenSSLCryptoAdapter::decrypt_buffer(const uint8_t* data, size_t size,
                                                          const std::string& key,
                                                          const std::vector<uint8_t>& iv,
                                                          const std::vector<uint8_t>& tag) {
    std::array<uint8_t, KEY_SIZE> aes_key{};
    if (!aes_key_from(key, aes_key)) {
        return {};
    }
    if (data == nullptr && size > 0) {
        spdlog::error("[CryptoAdapter] decrypt_buffer() called with a null buffer");
        return {};
    }
    if (iv.size() != IV_SIZE || tag.size() != TAG_SIZE) {
        spdlog::error("[CryptoAdapter] unexpected IV ({} B) or tag ({} B) length", iv.size(),
                      tag.size());
        return {};
    }
    if (!fits_in_int(size)) {
        spdlog::error("[CryptoAdapter] decrypt_buffer() size {} exceeds the EVP limit", size);
        return {};
    }

    CipherCtxPtr ctx = make_cipher_ctx();
    if (!ctx || !init_gcm_cipher(ctx.get(), false, aes_key, iv.data())) {
        spdlog::error("[CryptoAdapter] cannot initialise AES-256-GCM");
        return {};
    }

    std::vector<uint8_t> plain(size + TAG_SIZE);
    int update_len = 0;
    size_t total = 0;
    if (size > 0) {
        if (EVP_DecryptUpdate(ctx.get(), plain.data(), &update_len, data, static_cast<int>(size)) !=
            1) {
            spdlog::error("[CryptoAdapter] AES-256-GCM decrypt failed");
            return {};
        }
        total = static_cast<size_t>(update_len);
    }

    // tag 必须在 final 之前设置：final 既解密最后一个分组，也校验整段密文的认证标签。
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, static_cast<int>(TAG_SIZE),
                            const_cast<uint8_t*>(tag.data())) != 1) {
        spdlog::error("[CryptoAdapter] cannot set the GCM tag");
        return {};
    }

    int final_len = 0;
    if (EVP_DecryptFinal_ex(ctx.get(), plain.data() + total, &final_len) != 1) {
        spdlog::error(
            "[CryptoAdapter] AES-256-GCM authentication failed (wrong key or corrupt data)");
        return {};
    }
    total += static_cast<size_t>(final_len);
    plain.resize(total);
    return plain;
}

bool OpenSSLCryptoAdapter::encrypt_file(const std::string& input_path,
                                        const std::string& output_path, const std::string& key) {
    std::array<uint8_t, KEY_SIZE> aes_key{};
    if (!aes_key_from(key, aes_key)) {
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::is_regular_file(input_path, ec)) {
        spdlog::error("[CryptoAdapter] '{}' is not a regular file", input_path);
        return false;
    }

    std::ifstream in(input_path, std::ios::binary);
    if (!in) {
        spdlog::error("[CryptoAdapter] cannot open '{}' for reading", input_path);
        return false;
    }

    std::filesystem::path out_path = std::filesystem::path(output_path);
    std::ofstream out(out_path, std::ios::binary | std::ios::trunc);
    if (!out) {
        spdlog::error("[CryptoAdapter] cannot open '{}' for writing", output_path);
        return false;
    }

    // 失败路径统一「关流 + 删半成品」：留下一个解不开的密文文件比什么都不留更糟。
    auto fail = [&out, &out_path]() {
        out.close();
        std::error_code remove_ec;
        std::filesystem::remove(out_path, remove_ec);
        return false;
    };

    std::array<uint8_t, IV_SIZE> iv{};
    if (RAND_bytes(iv.data(), static_cast<int>(iv.size())) != 1) {
        spdlog::error("[CryptoAdapter] cannot generate a random IV");
        return fail();
    }
    if (!write_bytes(out, iv.data(), iv.size()) ||
        !encrypt_stream(in, out, aes_key, iv.data(), input_path)) {
        return fail();
    }

    out.flush();
    if (!out) {
        spdlog::error("[CryptoAdapter] failed to write '{}'", output_path);
        return fail();
    }

    spdlog::debug("[CryptoAdapter] encrypted '{}' -> '{}'", input_path, output_path);
    return true;
}

bool OpenSSLCryptoAdapter::decrypt_file(const std::string& input_path,
                                        const std::string& output_path, const std::string& key) {
    std::array<uint8_t, KEY_SIZE> aes_key{};
    if (!aes_key_from(key, aes_key)) {
        return false;
    }

    std::error_code ec;
    uintmax_t file_size = std::filesystem::file_size(input_path, ec);
    if (ec) {
        spdlog::error("[CryptoAdapter] cannot stat '{}': {}", input_path, ec.message());
        return false;
    }
    if (file_size < FILE_MIN_SIZE) {
        spdlog::error("[CryptoAdapter] '{}' is too short ({} bytes) to be a cipher file",
                      input_path, file_size);
        return false;
    }

    std::ifstream in(input_path, std::ios::binary);
    if (!in) {
        spdlog::error("[CryptoAdapter] cannot open '{}' for reading", input_path);
        return false;
    }

    std::filesystem::path out_path = std::filesystem::path(output_path);
    std::ofstream out(out_path, std::ios::binary | std::ios::trunc);
    if (!out) {
        spdlog::error("[CryptoAdapter] cannot open '{}' for writing", output_path);
        return false;
    }

    auto fail = [&out, &out_path]() {
        out.close();
        std::error_code remove_ec;
        std::filesystem::remove(out_path, remove_ec);
        return false;
    };

    // 容器布局：[12B IV][密文][16B tag]，tag 在尾部，所以先读头、再跳到尾部读 tag。
    std::array<uint8_t, IV_SIZE> iv{};
    if (!in.read(reinterpret_cast<char*>(iv.data()), static_cast<std::streamsize>(iv.size()))) {
        spdlog::error("[CryptoAdapter] cannot read the IV from '{}'", input_path);
        return fail();
    }

    std::array<uint8_t, TAG_SIZE> tag{};
    in.seekg(static_cast<std::streamoff>(file_size - TAG_SIZE), std::ios::beg);
    if (!in.read(reinterpret_cast<char*>(tag.data()), static_cast<std::streamsize>(tag.size()))) {
        spdlog::error("[CryptoAdapter] cannot read the tag from '{}'", input_path);
        return fail();
    }

    in.seekg(static_cast<std::streamoff>(IV_SIZE), std::ios::beg);
    if (!decrypt_stream(in, out, file_size - FILE_MIN_SIZE, aes_key, iv, tag, input_path)) {
        return fail();
    }

    out.flush();
    if (!out) {
        spdlog::error("[CryptoAdapter] failed to write '{}'", output_path);
        return fail();
    }

    spdlog::debug("[CryptoAdapter] decrypted '{}' -> '{}'", input_path, output_path);
    return true;
}

} // namespace Prism::Adapter
