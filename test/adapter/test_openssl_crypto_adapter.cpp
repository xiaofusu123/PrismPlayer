#include <gtest/gtest.h>

#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "OpenSSLCryptoAdapter.h"

namespace {

using Prism::Adapter::CipherSuite;
using Prism::Adapter::EncryptResult;
using Prism::Adapter::OpenSSLCryptoAdapter;

/** @brief 256 bit 的十六进制密钥，正好 32 字节 */
constexpr const char* TEST_KEY = "0123456789abcdef0123456789abcdef";

/** @brief 测试用明文密码（与参考向量绑定，改这里必须同时重算 REFERENCE_HASH） */
constexpr const char* TEST_PASSWORD = "Prism-demo-passphrase-01";

/** @brief 与 TEST_PASSWORD 只差最后两位的口令，用于「密码不匹配」的反向用例 */
constexpr const char* OTHER_PASSWORD = "Prism-demo-passphrase-02";

/**
 * @brief 参考向量：salt = 00 01 ... 0f，password = TEST_PASSWORD，取 SHA-256(salt || password)
 *
 * 由外部工具独立算出（对 `bytes(range(16)) + 密码的 UTF-8 字节` 求 SHA-256），
 * 用来钉住「盐在前、密码在后」以及小写十六进制编码这两条约定；换成别的顺序或编码这里会红。
 */
constexpr const char* REFERENCE_SALT = "000102030405060708090a0b0c0d0e0f";
constexpr const char* REFERENCE_HASH =
    "fedbe6aa64a3e55cd70c8ce053afa048b99c5b547ea4a0913216475f0297b020";

/**
 * @brief 生成一段可复现的伪随机数据
 * @param size 字节数
 * @return 数据
 */
std::vector<uint8_t> make_payload(size_t size) {
    std::mt19937 engine{20261107};
    std::vector<uint8_t> data(size);
    for (uint8_t& byte : data) {
        byte = static_cast<uint8_t>(engine() & 0xff);
    }
    return data;
}

/**
 * @brief 判断字符串是否是给定长度的十六进制串
 * @param text 待检查的字符串
 * @param length 期望的字符数
 * @return 全部由十六进制字符组成且长度相符返回 true
 */
bool is_hex_of_length(const std::string& text, size_t length) {
    if (text.size() != length) {
        return false;
    }
    for (char ch : text) {
        bool digit = ch >= '0' && ch <= '9';
        bool lower = ch >= 'a' && ch <= 'f';
        if (!digit && !lower) {
            return false;
        }
    }
    return true;
}

TEST(OpenSSLCryptoAdapter, HashPasswordProducesSaltAndDigest) {
    OpenSSLCryptoAdapter adapter;
    auto result = adapter.hash_password(TEST_PASSWORD);

    EXPECT_TRUE(is_hex_of_length(result.salt, 32));
    EXPECT_TRUE(is_hex_of_length(result.hash, 64));
    EXPECT_EQ(result.credential, "sha256$" + result.salt + "$" + result.hash);
}

TEST(OpenSSLCryptoAdapter, HashPasswordUsesRandomSalt) {
    OpenSSLCryptoAdapter adapter;
    auto first = adapter.hash_password(TEST_PASSWORD);
    auto second = adapter.hash_password(TEST_PASSWORD);

    EXPECT_NE(first.salt, second.salt);
    EXPECT_NE(first.hash, second.hash);
}

TEST(OpenSSLCryptoAdapter, VerifyPasswordAcceptsMatchingPassword) {
    OpenSSLCryptoAdapter adapter;
    auto result = adapter.hash_password(TEST_PASSWORD);

    EXPECT_TRUE(adapter.verify_password(TEST_PASSWORD, result.credential));
    EXPECT_FALSE(adapter.verify_password(OTHER_PASSWORD, result.credential));
}

TEST(OpenSSLCryptoAdapter, VerifyPasswordRejectsMalformedCredential) {
    OpenSSLCryptoAdapter adapter;

    EXPECT_FALSE(adapter.verify_password(TEST_PASSWORD, ""));
    EXPECT_FALSE(adapter.verify_password(TEST_PASSWORD, "zz"));
    // 缺分隔符、前缀不对、段数不对，一律拒绝，不做「尽力而为」的解析。
    EXPECT_FALSE(
        adapter.verify_password(TEST_PASSWORD, std::string(REFERENCE_SALT) + REFERENCE_HASH));
    EXPECT_FALSE(adapter.verify_password(TEST_PASSWORD, std::string("sha512$") + REFERENCE_SALT +
                                                            "$" + REFERENCE_HASH));
    EXPECT_FALSE(adapter.verify_password(TEST_PASSWORD, std::string("sha256$") + REFERENCE_SALT));
    // 盐长度不对（16 个字符 = 8 字节）同样拒绝。
    EXPECT_FALSE(adapter.verify_password(TEST_PASSWORD,
                                         std::string("sha256$0001020304050607$") + REFERENCE_HASH));
}

TEST(OpenSSLCryptoAdapter, VerifyPasswordMatchesReferenceVector) {
    OpenSSLCryptoAdapter adapter;
    std::string credential = std::string("sha256$") + REFERENCE_SALT + "$" + REFERENCE_HASH;

    EXPECT_TRUE(adapter.verify_password(TEST_PASSWORD, credential));
    EXPECT_FALSE(adapter.verify_password(OTHER_PASSWORD, credential));
}

TEST(OpenSSLCryptoAdapter, BufferRoundTrip) {
    OpenSSLCryptoAdapter adapter;
    std::vector<uint8_t> plain = make_payload(4096);

    EncryptResult encrypted =
        adapter.encrypt_buffer(plain.data(), plain.size(), TEST_KEY, CipherSuite::AES_256_GCM);
    ASSERT_EQ(encrypted.cipher_data.size(), plain.size());
    EXPECT_EQ(encrypted.iv.size(), 12U);
    EXPECT_EQ(encrypted.tag.size(), 16U);
    EXPECT_NE(encrypted.cipher_data, plain);

    auto decrypted =
        adapter.decrypt_buffer(encrypted.cipher_data.data(), encrypted.cipher_data.size(), TEST_KEY,
                               encrypted.iv, encrypted.tag, CipherSuite::AES_256_GCM);
    EXPECT_EQ(decrypted, plain);
}

TEST(OpenSSLCryptoAdapter, BufferRoundTripOfEmptyPayload) {
    OpenSSLCryptoAdapter adapter;

    EncryptResult encrypted =
        adapter.encrypt_buffer(nullptr, 0, TEST_KEY, CipherSuite::AES_256_GCM);
    EXPECT_TRUE(encrypted.cipher_data.empty());
    ASSERT_EQ(encrypted.tag.size(), 16U);

    auto decrypted = adapter.decrypt_buffer(nullptr, 0, TEST_KEY, encrypted.iv, encrypted.tag,
                                            CipherSuite::AES_256_GCM);
    EXPECT_TRUE(decrypted.empty());
}

TEST(OpenSSLCryptoAdapter, BufferDecryptRejectsTamperedData) {
    OpenSSLCryptoAdapter adapter;
    std::vector<uint8_t> plain = make_payload(1024);

    EncryptResult encrypted =
        adapter.encrypt_buffer(plain.data(), plain.size(), TEST_KEY, CipherSuite::AES_256_GCM);
    ASSERT_FALSE(encrypted.cipher_data.empty());

    auto tampered = encrypted.cipher_data;
    tampered[0] = static_cast<uint8_t>(tampered[0] ^ 0x01);
    EXPECT_TRUE(adapter
                    .decrypt_buffer(tampered.data(), tampered.size(), TEST_KEY, encrypted.iv,
                                    encrypted.tag, CipherSuite::AES_256_GCM)
                    .empty());

    auto bad_tag = encrypted.tag;
    bad_tag[0] = static_cast<uint8_t>(bad_tag[0] ^ 0x01);
    EXPECT_TRUE(adapter
                    .decrypt_buffer(encrypted.cipher_data.data(), encrypted.cipher_data.size(),
                                    TEST_KEY, encrypted.iv, bad_tag, CipherSuite::AES_256_GCM)
                    .empty());
}

TEST(OpenSSLCryptoAdapter, RejectsKeyThatIsNot32Bytes) {
    OpenSSLCryptoAdapter adapter;
    std::vector<uint8_t> plain = make_payload(16);

    EXPECT_TRUE(
        adapter
            .encrypt_buffer(plain.data(), plain.size(), "too-short-key", CipherSuite::AES_256_GCM)
            .cipher_data.empty());
    EXPECT_TRUE(adapter
                    .decrypt_buffer(plain.data(), plain.size(), "too-short-key", {}, {},
                                    CipherSuite::AES_256_GCM)
                    .empty());
}

TEST(OpenSSLCryptoAdapter, BufferRoundTripWithChaCha20Poly1305) {
    OpenSSLCryptoAdapter adapter;
    std::vector<uint8_t> plain = make_payload(4096);

    EncryptResult encrypted = adapter.encrypt_buffer(plain.data(), plain.size(), TEST_KEY,
                                                     CipherSuite::CHACHA20_POLY1305);
    ASSERT_EQ(encrypted.cipher_data.size(), plain.size());
    EXPECT_EQ(encrypted.iv.size(), 12U);
    EXPECT_EQ(encrypted.tag.size(), 16U);
    EXPECT_NE(encrypted.cipher_data, plain);

    auto decrypted =
        adapter.decrypt_buffer(encrypted.cipher_data.data(), encrypted.cipher_data.size(), TEST_KEY,
                               encrypted.iv, encrypted.tag, CipherSuite::CHACHA20_POLY1305);
    EXPECT_EQ(decrypted, plain);
}

TEST(OpenSSLCryptoAdapter, ChaCha20Poly1305RejectsTamperedData) {
    OpenSSLCryptoAdapter adapter;
    std::vector<uint8_t> plain = make_payload(1024);

    EncryptResult encrypted = adapter.encrypt_buffer(plain.data(), plain.size(), TEST_KEY,
                                                     CipherSuite::CHACHA20_POLY1305);
    ASSERT_FALSE(encrypted.cipher_data.empty());

    auto tampered = encrypted.cipher_data;
    tampered[0] = static_cast<uint8_t>(tampered[0] ^ 0x01);
    EXPECT_TRUE(adapter
                    .decrypt_buffer(tampered.data(), tampered.size(), TEST_KEY, encrypted.iv,
                                    encrypted.tag, CipherSuite::CHACHA20_POLY1305)
                    .empty());
}

TEST(OpenSSLCryptoAdapter, CrossSuiteDecryptFails) {
    OpenSSLCryptoAdapter adapter;
    std::vector<uint8_t> plain = make_payload(512);

    EncryptResult encrypted =
        adapter.encrypt_buffer(plain.data(), plain.size(), TEST_KEY, CipherSuite::AES_256_GCM);
    ASSERT_FALSE(encrypted.cipher_data.empty());

    // 同一把密钥、同一个 nonce、同一个 tag，只把套件换掉：认证必须失败，不能解出乱码。
    EXPECT_TRUE(adapter
                    .decrypt_buffer(encrypted.cipher_data.data(), encrypted.cipher_data.size(),
                                    TEST_KEY, encrypted.iv, encrypted.tag,
                                    CipherSuite::CHACHA20_POLY1305)
                    .empty());
}

} // namespace
