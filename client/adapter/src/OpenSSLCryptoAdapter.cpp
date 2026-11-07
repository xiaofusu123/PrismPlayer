#include "OpenSSLCryptoAdapter.h"

#include <array>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <spdlog/spdlog.h>

namespace Prism::Adapter {
namespace {

// ==================== 参数常量 ====================

constexpr int SALT_BYTES = 16;
constexpr int DERIVED_KEY_BYTES = 32;
constexpr int PBKDF2_ITERATIONS = 100000;
constexpr int64_t MIN_ITERATIONS = 1;
constexpr int64_t MAX_ITERATIONS = 10000000;
constexpr size_t AES_KEY_BYTES = 32;
constexpr int GCM_IV_BYTES = 12;
constexpr int GCM_TAG_BYTES = 16;
constexpr int GCM_BLOCK_BYTES = 16;
constexpr size_t FILE_CHUNK_BYTES = 64 * 1024;

constexpr char HASH_ALGORITHM[] = "pbkdf2-sha256";

// ==================== 文件容器布局 ====================
//
// 偏移 0   8   magic "PRISMENC"
// 偏移 8   1   version
// 偏移 9   3   reserved（全 0）
// 偏移 12  12  IV
// 偏移 24  16  GCM tag
// 偏移 40  n   密文

constexpr char ENC_MAGIC[8] = {'P', 'R', 'I', 'S', 'M', 'E', 'N', 'C'};
constexpr uint8_t ENC_VERSION = 1;
constexpr size_t ENC_HEADER_SIZE = 40;
constexpr size_t ENC_VERSION_OFFSET = 8;
constexpr size_t ENC_IV_OFFSET = 12;
constexpr size_t ENC_TAG_OFFSET = 24;

// ==================== 编解码辅助 ====================

/**
 * @brief 字节流转小写 hex
 * @param data 字节缓冲区
 * @param size 字节数
 * @return 长度为 size * 2 的 hex 串
 */
std::string to_hex(const uint8_t* data, size_t size) {
    static constexpr char HEX_DIGITS[] = "0123456789abcdef";
    std::string text;
    text.reserve(size * 2);
    for (size_t i = 0; i < size; ++i) {
        text.push_back(HEX_DIGITS[(data[i] >> 4) & 0x0F]);
        text.push_back(HEX_DIGITS[data[i] & 0x0F]);
    }
    return text;
}

/**
 * @brief 单个 hex 字符转数值
 * @param ch 字符
 * @return 0..15；非法字符返回 -1
 */
int hex_digit(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

/**
 * @brief 小写 hex 转字节流
 * @param hex 十六进制文本，长度必须为偶数
 * @param out 输出缓冲区，失败时清空
 * @return 转换成功返回 true
 */
bool from_hex(const std::string& hex, std::vector<uint8_t>& out) {
    out.clear();
    if (hex.empty() || hex.size() % 2 != 0) {
        return false;
    }

    out.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        int high = hex_digit(hex[i]);
        int low = hex_digit(hex[i + 1]);
        if (high < 0 || low < 0) {
            out.clear();
            return false;
        }
        out.push_back(static_cast<uint8_t>((high << 4) | low));
    }
    return true;
}

/**
 * @brief 按分隔符切分文本
 * @param text 输入文本
 * @param delim 分隔符
 * @return 各段文本，至少一段
 */
std::vector<std::string> split_by(const std::string& text, char delim) {
    std::vector<std::string> parts;
    std::string part;
    for (char ch : text) {
        if (ch == delim) {
            parts.push_back(part);
            part.clear();
        } else {
            part.push_back(ch);
        }
    }
    parts.push_back(part);
    return parts;
}

/**
 * @brief 解析十进制整数
 * @param text 输入文本
 * @return 解析结果；文本非法返回 -1
 */
int64_t parse_int(const std::string& text) {
    if (text.empty()) {
        return -1;
    }

    char* end = nullptr;
    long long value = std::strtoll(text.c_str(), &end, 10);
    if (end == nullptr || *end != '\0') {
        return -1;
    }
    return static_cast<int64_t>(value);
}

// ==================== 哈希格式 ====================

/**
 * @struct ParsedHash
 * @brief 从落库字符串解析出的哈希参数
 */
struct ParsedHash {
    bool valid{false};            /**< 解析是否成功 */
    int64_t iterations{0};        /**< PBKDF2 迭代次数 */
    std::vector<uint8_t> salt;    /**< 盐 */
    std::vector<uint8_t> derived; /**< 派生密钥 */
};

/**
 * @brief 解析 `pbkdf2-sha256$<iter>$<salt_hex>$<dk_hex>` 形式的哈希串
 * @param hash 落库字符串
 * @param fallback_salt 盐不在串里时使用的备用盐（来自 HashResult::salt）
 * @return 解析结果；不合法时 valid 为 false
 *
 * 兼容三种形态：四段（带盐）、三段（盐靠 fallback）、单段（纯 hex 的历史数据）。
 */
ParsedHash parse_password_hash(const std::string& hash, const std::string& fallback_salt) {
    ParsedHash parsed;

    std::vector<std::string> parts = split_by(hash, '$');
    std::string salt_hex;
    std::string derived_hex;

    if (parts.size() == 4) {
        if (parts[0] != HASH_ALGORITHM) {
            spdlog::warn("[CryptoAdapter] unknown hash algorithm '{}'", parts[0]);
            return parsed;
        }
        parsed.iterations = parse_int(parts[1]);
        salt_hex = parts[2];
        derived_hex = parts[3];
    } else if (parts.size() == 3) {
        if (parts[0] != HASH_ALGORITHM) {
            spdlog::warn("[CryptoAdapter] unknown hash algorithm '{}'", parts[0]);
            return parsed;
        }
        parsed.iterations = parse_int(parts[1]);
        salt_hex = fallback_salt;
        derived_hex = parts[2];
    } else if (parts.size() == 1) {
        parsed.iterations = PBKDF2_ITERATIONS;
        salt_hex = fallback_salt;
        derived_hex = parts[0];
    } else {
        spdlog::warn("[CryptoAdapter] malformed password hash");
        return parsed;
    }

    if (parsed.iterations < MIN_ITERATIONS || parsed.iterations > MAX_ITERATIONS) {
        spdlog::warn("[CryptoAdapter] iteration count {} out of range", parsed.iterations);
        return parsed;
    }
    if (!from_hex(salt_hex, parsed.salt) || parsed.salt.size() != static_cast<size_t>(SALT_BYTES)) {
        spdlog::warn("[CryptoAdapter] invalid salt in password hash");
        return parsed;
    }
    if (!from_hex(derived_hex, parsed.derived) ||
        parsed.derived.size() != static_cast<size_t>(DERIVED_KEY_BYTES)) {
        spdlog::warn("[CryptoAdapter] invalid derived key in password hash");
        return parsed;
    }

    parsed.valid = true;
    return parsed;
}

// ==================== AES-GCM 辅助 ====================

/**
 * @brief 初始化 AES-256-GCM 上下文（加密或解密）
 * @param ctx 上下文
 * @param encrypt true 为加密，false 为解密
 * @param key 32 字节密钥
 * @param iv 12 字节 IV
 * @return 成功返回 true
 */
bool init_gcm(EVP_CIPHER_CTX* ctx, bool encrypt, const std::string& key, const uint8_t* iv) {
    int (*init_call)(EVP_CIPHER_CTX*, const EVP_CIPHER*, ENGINE*, const unsigned char*,
                     const unsigned char*) = encrypt ? EVP_EncryptInit_ex : EVP_DecryptInit_ex;

    if (init_call(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        return false;
    }
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, GCM_IV_BYTES, nullptr) != 1) {
        return false;
    }
    return init_call(ctx, nullptr, nullptr, reinterpret_cast<const unsigned char*>(key.data()),
                     iv) == 1;
}

/**
 * @brief 把输入流按块喂进上下文并写出
 * @param ctx 上下文
 * @param encrypt true 为加密，false 为解密
 * @param in 输入流
 * @param out 输出流
 * @return 成功返回 true
 */
bool pump_stream(EVP_CIPHER_CTX* ctx, bool encrypt, std::ifstream& in, std::ofstream& out) {
    std::vector<uint8_t> in_buf(FILE_CHUNK_BYTES);
    std::vector<uint8_t> out_buf(FILE_CHUNK_BYTES + GCM_BLOCK_BYTES);

    while (in) {
        in.read(reinterpret_cast<char*>(in_buf.data()),
                static_cast<std::streamsize>(in_buf.size()));
        auto got = in.gcount();
        if (got <= 0) {
            break;
        }

        int out_len = 0;
        int rc = 0;
        if (encrypt) {
            rc = EVP_EncryptUpdate(ctx, out_buf.data(), &out_len, in_buf.data(),
                                   static_cast<int>(got));
        } else {
            rc = EVP_DecryptUpdate(ctx, out_buf.data(), &out_len, in_buf.data(),
                                   static_cast<int>(got));
        }
        if (rc != 1) {
            return false;
        }

        out.write(reinterpret_cast<const char*>(out_buf.data()), out_len);
        if (!out) {
            return false;
        }
    }
    return true;
}

/**
 * @brief 加密收尾：冲刷最后一块并取出 GCM tag
 * @param ctx 上下文
 * @param out 输出流
 * @param tag 输出 tag（16 字节）
 * @return 成功返回 true
 */
bool finish_encrypt(EVP_CIPHER_CTX* ctx, std::ofstream& out, std::vector<uint8_t>& tag) {
    std::array<uint8_t, GCM_BLOCK_BYTES> final_buf{};
    int final_len = 0;

    if (EVP_EncryptFinal_ex(ctx, final_buf.data(), &final_len) != 1) {
        return false;
    }
    if (final_len > 0) {
        out.write(reinterpret_cast<const char*>(final_buf.data()), final_len);
        if (!out) {
            return false;
        }
    }

    tag.resize(GCM_TAG_BYTES);
    return EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, GCM_TAG_BYTES, tag.data()) == 1;
}

/**
 * @brief 解密收尾：先设 tag 再 Final，返回值 > 0 表示认证通过
 * @param ctx 上下文
 * @param tag 密文附带的 tag
 * @return 认证通过返回 true
 */
bool finish_decrypt(EVP_CIPHER_CTX* ctx, const std::vector<uint8_t>& tag) {
    if (tag.size() != static_cast<size_t>(GCM_TAG_BYTES)) {
        return false;
    }

    std::vector<uint8_t> tag_copy(tag);
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, GCM_TAG_BYTES, tag_copy.data()) != 1) {
        return false;
    }

    std::array<uint8_t, GCM_BLOCK_BYTES> final_buf{};
    int final_len = 0;
    return EVP_DecryptFinal_ex(ctx, final_buf.data(), &final_len) > 0;
}

/**
 * @brief 写文件容器头
 * @param out 输出流
 * @param iv 12 字节 IV
 * @param tag 16 字节 tag（加密时可先写零值，完成后回填）
 * @return 成功返回 true
 */
bool write_header(std::ofstream& out, const uint8_t* iv, const std::vector<uint8_t>& tag) {
    std::array<uint8_t, ENC_HEADER_SIZE> header{};
    std::memcpy(header.data(), ENC_MAGIC, sizeof(ENC_MAGIC));
    header[ENC_VERSION_OFFSET] = ENC_VERSION;
    std::memcpy(header.data() + ENC_IV_OFFSET, iv, static_cast<size_t>(GCM_IV_BYTES));
    if (tag.size() == static_cast<size_t>(GCM_TAG_BYTES)) {
        std::memcpy(header.data() + ENC_TAG_OFFSET, tag.data(), tag.size());
    }

    out.write(reinterpret_cast<const char*>(header.data()),
              static_cast<std::streamsize>(header.size()));
    return static_cast<bool>(out);
}

/**
 * @brief 读并校验文件容器头
 * @param in 输入流
 * @param iv 输出 IV
 * @param tag 输出 tag
 * @return 头合法返回 true
 */
bool read_header(std::ifstream& in, std::array<uint8_t, GCM_IV_BYTES>& iv,
                 std::vector<uint8_t>& tag) {
    std::array<uint8_t, ENC_HEADER_SIZE> header{};
    in.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
    if (in.gcount() != static_cast<std::streamsize>(header.size())) {
        spdlog::error("[CryptoAdapter] file is too short to hold a header");
        return false;
    }
    if (std::memcmp(header.data(), ENC_MAGIC, sizeof(ENC_MAGIC)) != 0) {
        spdlog::error("[CryptoAdapter] bad magic: not a Prism encrypted file");
        return false;
    }
    if (header[ENC_VERSION_OFFSET] != ENC_VERSION) {
        spdlog::error("[CryptoAdapter] unsupported container version {}",
                      header[ENC_VERSION_OFFSET]);
        return false;
    }

    std::memcpy(iv.data(), header.data() + ENC_IV_OFFSET, iv.size());
    tag.resize(GCM_TAG_BYTES);
    std::memcpy(tag.data(), header.data() + ENC_TAG_OFFSET, tag.size());
    return true;
}

/**
 * @brief 校验密钥长度
 * @param key 调用方传入的密钥
 * @param caller 调用方名字，仅用于日志
 * @return 长度合法返回 true
 */
bool check_key(const std::string& key, const char* caller) {
    if (key.size() == AES_KEY_BYTES) {
        return true;
    }
    spdlog::error("[CryptoAdapter] {}(): key must be {} raw bytes, got {}", caller, AES_KEY_BYTES,
                  key.size());
    return false;
}

} // namespace

// ==================== 密码哈希 ====================

HashResult OpenSSLCryptoAdapter::hash_password(const std::string& password) {
    HashResult result;

    std::vector<uint8_t> salt(static_cast<size_t>(SALT_BYTES));
    if (RAND_bytes(salt.data(), SALT_BYTES) != 1) {
        spdlog::error("[CryptoAdapter] hash_password(): RAND_bytes() failed");
        return result;
    }

    std::vector<uint8_t> derived(static_cast<size_t>(DERIVED_KEY_BYTES));
    if (PKCS5_PBKDF2_HMAC(password.data(), static_cast<int>(password.size()), salt.data(),
                          SALT_BYTES, PBKDF2_ITERATIONS, EVP_sha256(), DERIVED_KEY_BYTES,
                          derived.data()) != 1) {
        spdlog::error("[CryptoAdapter] hash_password(): PKCS5_PBKDF2_HMAC() failed");
        return result;
    }

    result.salt = to_hex(salt.data(), salt.size());
    result.hash = std::string(HASH_ALGORITHM) + "$" + std::to_string(PBKDF2_ITERATIONS) + "$" +
                  result.salt + "$" + to_hex(derived.data(), derived.size());
    spdlog::debug("[CryptoAdapter] hash_password(): {} iterations, {} byte salt", PBKDF2_ITERATIONS,
                  SALT_BYTES);
    return result;
}

bool OpenSSLCryptoAdapter::verify_password(const std::string& password, const std::string& hash,
                                           const std::string& salt) {
    ParsedHash parsed = parse_password_hash(hash, salt);
    if (!parsed.valid) {
        return false;
    }

    std::vector<uint8_t> derived(parsed.derived.size());
    if (PKCS5_PBKDF2_HMAC(password.data(), static_cast<int>(password.size()), parsed.salt.data(),
                          static_cast<int>(parsed.salt.size()), static_cast<int>(parsed.iterations),
                          EVP_sha256(), static_cast<int>(derived.size()), derived.data()) != 1) {
        spdlog::error("[CryptoAdapter] verify_password(): PKCS5_PBKDF2_HMAC() failed");
        return false;
    }

    // 长度已在解析阶段固定为 DERIVED_KEY_BYTES，这里只做常量时间比较。
    return CRYPTO_memcmp(derived.data(), parsed.derived.data(), derived.size()) == 0;
}

// ==================== 内存加解密 ====================

EncryptResult OpenSSLCryptoAdapter::encrypt_buffer(const uint8_t* data, size_t size,
                                                   const std::string& key) {
    EncryptResult result;
    if (!check_key(key, "encrypt_buffer")) {
        return result;
    }
    if (data == nullptr && size > 0) {
        spdlog::error("[CryptoAdapter] encrypt_buffer(): null data with non-zero size");
        return result;
    }
    if (size > static_cast<size_t>(INT_MAX)) {
        spdlog::error("[CryptoAdapter] encrypt_buffer(): buffer too large ({} bytes)", size);
        return result;
    }

    std::vector<uint8_t> iv(static_cast<size_t>(GCM_IV_BYTES));
    if (RAND_bytes(iv.data(), GCM_IV_BYTES) != 1) {
        spdlog::error("[CryptoAdapter] encrypt_buffer(): RAND_bytes() failed");
        return result;
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (ctx == nullptr) {
        spdlog::error("[CryptoAdapter] encrypt_buffer(): EVP_CIPHER_CTX_new() failed");
        return result;
    }

    std::vector<uint8_t> cipher(size + static_cast<size_t>(GCM_BLOCK_BYTES));
    int out_len = 0;
    int final_len = 0;
    bool ok = init_gcm(ctx, true, key, iv.data());
    if (ok && size > 0) {
        ok = EVP_EncryptUpdate(ctx, cipher.data(), &out_len, data, static_cast<int>(size)) == 1;
    }
    if (ok) {
        ok = EVP_EncryptFinal_ex(ctx, cipher.data() + out_len, &final_len) == 1;
    }

    std::vector<uint8_t> tag(static_cast<size_t>(GCM_TAG_BYTES));
    if (ok) {
        ok = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, GCM_TAG_BYTES, tag.data()) == 1;
    }
    EVP_CIPHER_CTX_free(ctx);

    if (!ok) {
        spdlog::error("[CryptoAdapter] encrypt_buffer(): AES-256-GCM encryption failed");
        return result;
    }

    cipher.resize(static_cast<size_t>(out_len + final_len));
    result.cipher_data = std::move(cipher);
    result.iv = std::move(iv);
    result.tag = std::move(tag);
    return result;
}

std::vector<uint8_t> OpenSSLCryptoAdapter::decrypt_buffer(const uint8_t* data, size_t size,
                                                          const std::string& key,
                                                          const std::vector<uint8_t>& iv,
                                                          const std::vector<uint8_t>& tag) {
    std::vector<uint8_t> plain;
    if (!check_key(key, "decrypt_buffer")) {
        return plain;
    }
    if (iv.size() != static_cast<size_t>(GCM_IV_BYTES) ||
        tag.size() != static_cast<size_t>(GCM_TAG_BYTES)) {
        spdlog::error("[CryptoAdapter] decrypt_buffer(): iv must be {} bytes and tag {} bytes",
                      GCM_IV_BYTES, GCM_TAG_BYTES);
        return plain;
    }
    if (data == nullptr && size > 0) {
        spdlog::error("[CryptoAdapter] decrypt_buffer(): null data with non-zero size");
        return plain;
    }
    if (size > static_cast<size_t>(INT_MAX)) {
        spdlog::error("[CryptoAdapter] decrypt_buffer(): buffer too large ({} bytes)", size);
        return plain;
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (ctx == nullptr) {
        spdlog::error("[CryptoAdapter] decrypt_buffer(): EVP_CIPHER_CTX_new() failed");
        return plain;
    }

    plain.assign(size + static_cast<size_t>(GCM_BLOCK_BYTES), 0);
    int out_len = 0;
    int final_len = 0;
    bool ok = init_gcm(ctx, false, key, iv.data());
    if (ok && size > 0) {
        ok = EVP_DecryptUpdate(ctx, plain.data(), &out_len, data, static_cast<int>(size)) == 1;
    }
    if (ok) {
        // tag 必须在 Final 之前设置，否则认证永远失败。
        std::vector<uint8_t> tag_copy(tag);
        ok = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, GCM_TAG_BYTES, tag_copy.data()) == 1;
    }
    if (ok) {
        ok = EVP_DecryptFinal_ex(ctx, plain.data() + out_len, &final_len) > 0;
    }
    EVP_CIPHER_CTX_free(ctx);

    if (!ok) {
        // 不区分「tag 错」和「数据损坏」，避免成为解密 oracle。
        spdlog::warn("[CryptoAdapter] decrypt_buffer(): authentication failed");
        return {};
    }

    plain.resize(static_cast<size_t>(out_len + final_len));
    return plain;
}

// ==================== 文件加解密 ====================

bool OpenSSLCryptoAdapter::encrypt_file(const std::string& input_path,
                                        const std::string& output_path, const std::string& key) {
    if (!check_key(key, "encrypt_file")) {
        return false;
    }

    std::ifstream in(input_path, std::ios::binary);
    if (!in) {
        spdlog::error("[CryptoAdapter] encrypt_file(): cannot open '{}'", input_path);
        return false;
    }

    std::ofstream out(output_path, std::ios::binary | std::ios::trunc);
    if (!out) {
        spdlog::error("[CryptoAdapter] encrypt_file(): cannot create '{}'", output_path);
        return false;
    }

    std::vector<uint8_t> iv(static_cast<size_t>(GCM_IV_BYTES));
    if (RAND_bytes(iv.data(), GCM_IV_BYTES) != 1) {
        spdlog::error("[CryptoAdapter] encrypt_file(): RAND_bytes() failed");
        return false;
    }

    // 先写零 tag 的头，密文写完后回填
    std::vector<uint8_t> tag(static_cast<size_t>(GCM_TAG_BYTES), 0);
    if (!write_header(out, iv.data(), tag)) {
        spdlog::error("[CryptoAdapter] encrypt_file(): cannot write header to '{}'", output_path);
        return false;
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (ctx == nullptr) {
        spdlog::error("[CryptoAdapter] encrypt_file(): EVP_CIPHER_CTX_new() failed");
        return false;
    }

    bool ok = init_gcm(ctx, true, key, iv.data());
    if (ok) {
        ok = pump_stream(ctx, true, in, out);
    }
    if (ok) {
        ok = finish_encrypt(ctx, out, tag);
    }
    EVP_CIPHER_CTX_free(ctx);
    if (!ok) {
        spdlog::error("[CryptoAdapter] encrypt_file(): encryption failed for '{}'", input_path);
        return false;
    }

    out.seekp(static_cast<std::streamoff>(ENC_TAG_OFFSET));
    out.write(reinterpret_cast<const char*>(tag.data()), static_cast<std::streamsize>(tag.size()));
    if (!out) {
        spdlog::error("[CryptoAdapter] encrypt_file(): cannot backfill tag in '{}'", output_path);
        return false;
    }

    spdlog::debug("[CryptoAdapter] encrypt_file(): '{}' -> '{}'", input_path, output_path);
    return true;
}

bool OpenSSLCryptoAdapter::decrypt_file(const std::string& input_path,
                                        const std::string& output_path, const std::string& key) {
    if (!check_key(key, "decrypt_file")) {
        return false;
    }

    std::ifstream in(input_path, std::ios::binary);
    if (!in) {
        spdlog::error("[CryptoAdapter] decrypt_file(): cannot open '{}'", input_path);
        return false;
    }

    std::array<uint8_t, GCM_IV_BYTES> iv{};
    std::vector<uint8_t> tag;
    if (!read_header(in, iv, tag)) {
        return false;
    }

    std::ofstream out(output_path, std::ios::binary | std::ios::trunc);
    if (!out) {
        spdlog::error("[CryptoAdapter] decrypt_file(): cannot create '{}'", output_path);
        return false;
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (ctx == nullptr) {
        spdlog::error("[CryptoAdapter] decrypt_file(): EVP_CIPHER_CTX_new() failed");
        return false;
    }

    bool ok = init_gcm(ctx, false, key, iv.data());
    if (ok) {
        ok = pump_stream(ctx, false, in, out);
    }
    if (ok) {
        ok = finish_decrypt(ctx, tag);
    }
    EVP_CIPHER_CTX_free(ctx);
    if (!ok) {
        spdlog::warn("[CryptoAdapter] decrypt_file(): authentication failed for '{}'", input_path);
        return false;
    }

    spdlog::debug("[CryptoAdapter] decrypt_file(): '{}' -> '{}'", input_path, output_path);
    return true;
}

} // namespace Prism::Adapter
