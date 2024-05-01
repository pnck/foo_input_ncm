#include "stdafx.h"
#include "gtest/gtest.h"
#include "cipher/aes.hpp"
#include <memory>
#include <vector>

using namespace fb2k_ncm::cipher;

class AESFuncitoalityTest : public ::testing::Test {
protected:
    BCRYPT_ALG_HANDLE h_crypt_ = NULL;
    BCRYPT_KEY_HANDLE h_key_ = NULL;
    // make a random key so that we can test over it
    uint8_t random_plain_key_[32] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    // for correctness comparation
    uint8_t random_plain_data_[2048] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    uint8_t encrypted_data_ecb_[sizeof(random_plain_data_) + AES_BLOCKSIZE /*padding block*/];
    uint8_t encrypted_data_cbc_[sizeof(random_plain_data_) + AES_BLOCKSIZE /*padding block*/];

protected:
    void SetUp() override {
        // check bcrypt api available
        // feel sorry to hack the test framework but there is necessity
        ULONG cbOutput = 0;
        ULONG cbResult = 0;
        ASSERT_GE(BCryptGenRandom(NULL, random_plain_key_, 32, BCRYPT_USE_SYSTEM_PREFERRED_RNG), 0);
        ASSERT_GE(BCryptGenRandom(NULL, random_plain_data_, 2048, BCRYPT_USE_SYSTEM_PREFERRED_RNG), 0);
        ASSERT_GE(BCryptOpenAlgorithmProvider(&h_crypt_, BCRYPT_AES_ALGORITHM, NULL, 0), 0);
        ASSERT_GE(BCryptGenerateSymmetricKey(h_crypt_, &h_key_, NULL, 0, random_plain_key_, 32, 0), 0);
        ASSERT_TRUE((h_key_ != NULL));
        ASSERT_GE(BCryptGetProperty(h_crypt_, BCRYPT_BLOCK_LENGTH, (PUCHAR)&cbOutput, sizeof(cbOutput), &cbResult, 0), 0);
        ASSERT_EQ(16, cbOutput);
        // ecb encrypt/decrypt
        ASSERT_GE(BCryptSetProperty(h_key_, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_ECB, sizeof(BCRYPT_CHAIN_MODE_ECB), 0), 0);
        ASSERT_GE(BCryptEncrypt(h_key_,
                                (PUCHAR)random_plain_data_,
                                sizeof(random_plain_data_),
                                NULL,
                                NULL,
                                0,
                                encrypted_data_ecb_,
                                sizeof(encrypted_data_ecb_),
                                &cbResult,
                                BCRYPT_BLOCK_PADDING),
                  0);
        ASSERT_EQ(sizeof(encrypted_data_ecb_), cbResult);
        auto _p = std::make_unique<uint8_t[]>(sizeof(encrypted_data_ecb_));
        ASSERT_GE(BCryptDecrypt(h_key_,
                                (PUCHAR)encrypted_data_ecb_,
                                sizeof(encrypted_data_ecb_),
                                NULL,
                                NULL,
                                0,
                                (PUCHAR)_p.get(),
                                sizeof(encrypted_data_ecb_),
                                &cbResult,
                                BCRYPT_BLOCK_PADDING),
                  0);
        ASSERT_EQ(sizeof(random_plain_data_), cbResult);
        ASSERT_EQ(memcmp(random_plain_data_, _p.get(), sizeof(random_plain_data_)), 0);
        // cbc encrypt/decrypt
        ASSERT_GE(BCryptSetProperty(h_key_, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_CBC, sizeof(BCRYPT_CHAIN_MODE_CBC), 0), 0);
        ASSERT_GE(BCryptEncrypt(h_key_,
                                (PUCHAR)random_plain_data_,
                                sizeof(random_plain_data_),
                                NULL,
                                NULL,
                                0,
                                encrypted_data_cbc_,
                                sizeof(encrypted_data_cbc_),
                                &cbResult,
                                BCRYPT_BLOCK_PADDING),
                  0);
        ASSERT_EQ(sizeof(encrypted_data_cbc_), cbResult);
        ASSERT_NE(memcmp(encrypted_data_ecb_, encrypted_data_cbc_, sizeof(encrypted_data_cbc_)), 0);
        _p = std::make_unique<uint8_t[]>(sizeof(encrypted_data_cbc_));
        ASSERT_GE(BCryptDecrypt(h_key_,
                                (PUCHAR)encrypted_data_cbc_,
                                sizeof(encrypted_data_cbc_),
                                NULL,
                                NULL,
                                0,
                                (PUCHAR)_p.get(),
                                sizeof(encrypted_data_cbc_),
                                &cbResult,
                                BCRYPT_BLOCK_PADDING),
                  0);
        ASSERT_EQ(sizeof(random_plain_data_), cbResult);
        ASSERT_EQ(memcmp(random_plain_data_, _p.get(), sizeof(random_plain_data_)), 0);
    }
    void TearDown() override {
        if (h_key_) {
            BCryptDestroyKey(h_key_);
        }
        if (h_crypt_) {
            BCryptCloseAlgorithmProvider(h_crypt_, 0);
        }
    }
};

TEST_F(AESFuncitoalityTest, BCryptAPI) {
    // Actually do nothing here
    ASSERT_EQ(0, 0);
    SUCCEED() << "BCrypt API test passed in fixture setup.";
}

TEST_F(AESFuncitoalityTest, AESClasses) {
    try {
        auto context = make_AES_context_with_key(random_plain_key_);
        ASSERT_FALSE(context.is_done());
        // ASSERT_EQ(context.buffer_capacity(), 0);
        ASSERT_EQ(context.key_bit_len(), sizeof(random_plain_key_) * 8);
        ASSERT_EQ(context.set_chain_mode(aes_chain_mode::CBC).chain_mode(), aes_chain_mode::CBC);
        ASSERT_EQ(context.set_chain_mode(aes_chain_mode::ECB).chain_mode(), aes_chain_mode::ECB);
        context.set_input(encrypted_data_ecb_, sizeof(encrypted_data_ecb_));

        // use internal buffer

        // decrypt with sized chunk
        ASSERT_FALSE(context.decrypt_chunk(64).is_done());
        auto _p = context.copy_buffer_as_ptr();
        ASSERT_EQ(memcmp(_p.get(), random_plain_data_, 64), 0);

        // decrypt_next() and output with pointer
        _p = std::make_unique<uint8_t[]>(64);
        ASSERT_EQ(context.set_output(_p.get(), 64).output_remain(), 64);
        ASSERT_FALSE(context.decrypt_next().is_done());
        ASSERT_EQ(memcmp(_p.get(), random_plain_data_ + 64, 64), 0);

        // output to another vector
        std::vector<uint8_t> buffer;
        ASSERT_EQ(context.outputted_len(), 64);
        // outputted length should reset after change destination
        ASSERT_EQ(context.set_output(buffer).outputted_len(), 0);
        ASSERT_FALSE(context.decrypt_next().is_done());
        ASSERT_EQ(buffer.size(), 64);
        ASSERT_EQ(memcmp(buffer.data(), random_plain_data_ + 128, 64), 0);

        // decrypt_all()
        ASSERT_EQ(context.input_remain(), sizeof(encrypted_data_ecb_) - 3 * 64);
        ASSERT_TRUE(context.decrypt_all().is_done());
        ASSERT_FALSE(context.input_remain());
        ASSERT_FALSE(context.output_remain());
        ASSERT_EQ(memcmp(buffer.data(), random_plain_data_ + 128, sizeof(random_plain_data_) - 128), 0);
        ASSERT_EQ(guess_padding(buffer.data() + buffer.size()), AES_BLOCKSIZE);
        ASSERT_EQ(context.inputted_len(), sizeof(encrypted_data_ecb_));
        ASSERT_EQ(context.outputted_len(), sizeof(encrypted_data_ecb_) - 128);
    } catch (const fb2k_ncm::cipher::cipher_error &e) {
        FAIL() << e.what();
    }
}

TEST_F(AESFuncitoalityTest, AES128AllIn1) {
    BCRYPT_KEY_HANDLE h_key;
    ULONG cbResult = 0;
    const size_t data_size = 256;
    auto data = std::make_unique<uint8_t[]>(data_size * 3);
    ASSERT_GE(BCryptGenRandom(NULL, data.get(), 256, BCRYPT_USE_SYSTEM_PREFERRED_RNG), 0);
    ASSERT_GE(BCryptGenerateSymmetricKey(h_crypt_, &h_key, NULL, 0, random_plain_key_, 16, 0), 0);
    ASSERT_GE(BCryptSetProperty(h_key, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_ECB, sizeof(BCRYPT_CHAIN_MODE_ECB), 0), 0);
    ASSERT_GE(BCryptEncrypt(h_key, (PUCHAR)data.get(), data_size, NULL, NULL, 0, data.get() + data_size, data_size, &cbResult, 0), 0);
    ASSERT_GE(BCryptSetProperty(h_key, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_CBC, sizeof(BCRYPT_CHAIN_MODE_CBC), 0), 0);
    ASSERT_GE(BCryptEncrypt(h_key, (PUCHAR)data.get(), data_size, NULL, NULL, 0, data.get() + data_size * 2, data_size, &cbResult, 0), 0);

    try {
        auto context = make_AES_context_with_key(random_plain_key_, 16);
        ASSERT_TRUE(context.set_chain_mode(aes_chain_mode::ECB)
                        .set_input(data.get() + data_size, data_size)
                        .decrypt_chunk(16)
                        .decrypt_all()
                        .is_done());
        auto _p = context.copy_buffer_as_ptr();
        ASSERT_EQ(memcmp(_p.get(), data.get(), data_size), 0);

        context = make_AES_context_with_key(random_plain_key_, 16);
        auto _v = std::vector(data.get() + data_size * 2, data.get() + data_size * 3);
        ASSERT_TRUE(context
                        .set_input(_v)
                        // in-place
                        .set_output(_v.data(), _v.size())
                        .set_chain_mode(aes_chain_mode::CBC)
                        .decrypt_all()
                        .is_done());
        ASSERT_EQ(memcmp(data.get(), _v.data(), data_size), 0);

    } catch (const fb2k_ncm::cipher::cipher_error &e) {
        FAIL() << e.what();
    }
    BCryptDestroyKey(h_key);
}

TEST_F(AESFuncitoalityTest, AES192AllIn1) {
    BCRYPT_KEY_HANDLE h_key;
    ULONG cbResult = 0;
    const size_t data_size = 256;
    auto data = std::make_unique<uint8_t[]>(data_size * 3);
    ASSERT_GE(BCryptGenRandom(NULL, data.get(), 256, BCRYPT_USE_SYSTEM_PREFERRED_RNG), 0);
    ASSERT_GE(BCryptGenerateSymmetricKey(h_crypt_, &h_key, NULL, 0, random_plain_key_, 24, 0), 0);
    ASSERT_GE(BCryptSetProperty(h_key, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_ECB, sizeof(BCRYPT_CHAIN_MODE_ECB), 0), 0);
    ASSERT_GE(BCryptEncrypt(h_key, (PUCHAR)data.get(), data_size, NULL, NULL, 0, data.get() + data_size, data_size, &cbResult, 0), 0);
    ASSERT_GE(BCryptSetProperty(h_key, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_CBC, sizeof(BCRYPT_CHAIN_MODE_CBC), 0), 0);
    ASSERT_GE(BCryptEncrypt(h_key, (PUCHAR)data.get(), data_size, NULL, NULL, 0, data.get() + data_size * 2, data_size, &cbResult, 0), 0);

    try {
        auto context = make_AES_context_with_key(random_plain_key_, 24);
        ASSERT_TRUE(context.set_chain_mode(aes_chain_mode::ECB)
                        .set_input(data.get() + data_size, data_size)
                        .decrypt_chunk(16)
                        .decrypt_all()
                        .is_done());
        auto _p = context.copy_buffer_as_ptr();
        ASSERT_EQ(memcmp(_p.get(), data.get(), data_size), 0);

        context = make_AES_context_with_key(random_plain_key_, 24);
        auto _v = std::vector(data.get() + data_size * 2, data.get() + data_size * 3);
        ASSERT_TRUE(context
                        .set_input(_v)
                        // in-place
                        .set_output(_v.data(), _v.size())
                        .set_chain_mode(aes_chain_mode::CBC)
                        .decrypt_all()
                        .is_done());
        ASSERT_EQ(memcmp(data.get(), _v.data(), data_size), 0);

    } catch (const fb2k_ncm::cipher::cipher_error &e) {
        FAIL() << e.what();
    }
    BCryptDestroyKey(h_key);
}

TEST_F(AESFuncitoalityTest, AES256AllIn1) {
    BCRYPT_KEY_HANDLE h_key;
    ULONG cbResult = 0;
    const size_t data_size = 256;
    auto data = std::make_unique<uint8_t[]>(data_size * 3);
    ASSERT_GE(BCryptGenRandom(NULL, data.get(), 256, BCRYPT_USE_SYSTEM_PREFERRED_RNG), 0);
    ASSERT_GE(BCryptGenerateSymmetricKey(h_crypt_, &h_key, NULL, 0, random_plain_key_, 32, 0), 0);
    ASSERT_GE(BCryptSetProperty(h_key, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_ECB, sizeof(BCRYPT_CHAIN_MODE_ECB), 0), 0);
    ASSERT_GE(BCryptEncrypt(h_key, (PUCHAR)data.get(), data_size, NULL, NULL, 0, data.get() + data_size, data_size, &cbResult, 0), 0);
    ASSERT_GE(BCryptSetProperty(h_key, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_CBC, sizeof(BCRYPT_CHAIN_MODE_CBC), 0), 0);
    ASSERT_GE(BCryptEncrypt(h_key, (PUCHAR)data.get(), data_size, NULL, NULL, 0, data.get() + data_size * 2, data_size, &cbResult, 0), 0);

    try {
        auto context = make_AES_context_with_key(random_plain_key_, 32);
        ASSERT_TRUE(context.set_chain_mode(aes_chain_mode::ECB)
                        .set_input(data.get() + data_size, data_size)
                        .decrypt_chunk(16)
                        .decrypt_all()
                        .is_done());
        auto _p = context.copy_buffer_as_ptr();
        ASSERT_EQ(memcmp(_p.get(), data.get(), data_size), 0);

        context = make_AES_context_with_key(random_plain_key_, 32);
        auto _v = std::vector(data.get() + data_size * 2, data.get() + data_size * 3);
        ASSERT_TRUE(context
                        .set_input(_v)
                        // in-place
                        .set_output(_v.data(), _v.size())
                        .set_chain_mode(aes_chain_mode::CBC)
                        .decrypt_all()
                        .is_done());
        ASSERT_EQ(memcmp(data.get(), _v.data(), data_size), 0);

    } catch (const fb2k_ncm::cipher::cipher_error &e) {
        FAIL() << e.what();
    }
    BCryptDestroyKey(h_key);
}

TEST_F(AESFuncitoalityTest, Encryption) {
#if 0
    auto buffer = std::make_unique<uint8_t[]>(36);
    auto &_plain_data = *reinterpret_cast<uint8_t(*)[36]>((uint8_t *)(buffer.get()));
    memcpy(buffer.get(), "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890", 36);
#endif
    auto &_plain_data = random_plain_data_;
    std::vector<uint8_t> enc_ecb(aligned(sizeof(random_plain_data_)));
    std::vector<uint8_t> enc_cbc(enc_ecb.size());
    std::vector<uint8_t> dec_ecb(enc_ecb.size());
    std::vector<uint8_t> dec_cbc(enc_ecb.size());

    try {
        auto context1 = make_AES_context_with_key(random_plain_key_);
        auto context2 = make_AES_context_with_key(random_plain_key_);
        auto context3 = make_AES_context_with_key(random_plain_key_);
        auto context4 = make_AES_context_with_key(random_plain_key_);
        ASSERT_TRUE(context1.set_chain_mode(aes_chain_mode::ECB)
                        .set_output(enc_ecb)
                        .set_input(_plain_data, sizeof(_plain_data))
                        .encrypt_all()
                        .is_done());
        ASSERT_TRUE(context2.set_chain_mode(aes_chain_mode::CBC)
                        .set_output(enc_cbc)
                        .set_input(_plain_data, sizeof(_plain_data))
                        .encrypt_all()
                        .is_done());
        ASSERT_TRUE(context3.set_chain_mode(aes_chain_mode::CBC).set_input(enc_cbc).set_output(dec_cbc).decrypt_all().is_done());
        ASSERT_TRUE(context4.set_chain_mode(aes_chain_mode::ECB).set_input(enc_ecb).set_output(dec_ecb).decrypt_all().is_done());
        ASSERT_EQ(0, memcmp(_plain_data, dec_cbc.data(), sizeof(_plain_data)));
        ASSERT_EQ(0, memcmp(_plain_data, dec_ecb.data(), sizeof(_plain_data)));

    } catch (const fb2k_ncm::cipher::cipher_error &e) {
        FAIL() << e.what();
    }
}

TEST_F(AESFuncitoalityTest, Falses) {
    std::vector<uint8_t> invalid_key(10);
    try {
        AES128 _t;
        _t.load_key(invalid_key);
        FAIL() << "Not throwing exception.";
    } catch (const fb2k_ncm::cipher::cipher_error &e) {
        SUCCEED() << e.what();
    }
    try {
        AES192 _t;
        _t.load_key(invalid_key);
        FAIL() << "Not throwing exception.";
    } catch (const fb2k_ncm::cipher::cipher_error &e) {
        SUCCEED() << e.what();
    }
    try {
        AES256 _t;
        _t.load_key(invalid_key);
        FAIL() << "Not throwing exception.";
    } catch (const fb2k_ncm::cipher::cipher_error &e) {
        SUCCEED() << e.what();
    }
    try {
        auto _c = make_AES_context_with_key(invalid_key);
        FAIL() << "Not throwing exception.";
    } catch (const fb2k_ncm::cipher::cipher_error &e) {
        SUCCEED() << e.what();
    }
}