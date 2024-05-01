/// @note To setup google test:
/// git submodule update --init --recursive --depth 1
/// cd vendor/googletest && mkdir build
/// cd build && cmake .. -G Xcode
#include "stdafx.h"
#include "gtest/gtest.h"
#include "cipher/aes.hpp"

#include <CommonCrypto/CommonCrypto.h>

#include <cstdlib>
#include <memory>
#include <vector>
#include <algorithm>

class AESFuncitoalityTest : public ::testing::Test {
protected:
    CCCryptorRef cryptor_ = NULL;
    std::vector<uint8_t> key_;
    std::vector<uint8_t> random_plain_;

    void SetUp() override {
        // Generate random key
        key_.resize(kCCKeySizeAES128);
        arc4random_buf(key_.data(), key_.size());

        // random plain text
        auto len = arc4random_uniform(128);
        len = (len + 10) % 128;
        random_plain_.resize(len);
        arc4random_buf(random_plain_.data(), random_plain_.size());
        decltype(random_plain_) _end = {'E', 'N', 'D', 'O', 'F', 'P', 'L', 'A', 'I', 'N'};
        random_plain_.insert(random_plain_.end(), _end.begin(), _end.end());

        // Initialize cryptor
        CCCryptorStatus status =
            CCCryptorCreate(kCCEncrypt, kCCAlgorithmAES, kCCOptionPKCS7Padding, key_.data(), key_.size(), NULL, &cryptor_);
        ASSERT_EQ(status, kCCSuccess);
    }

    void TearDown() override {
        // Release cryptor
        if (cryptor_ != NULL) {
            CCCryptorRelease(cryptor_);
            cryptor_ = NULL;
        }
    }

    void reset(const std::vector<uint8_t> &key, CCOptions options) {
        if (cryptor_ != NULL) {
            CCCryptorRelease(cryptor_);
            cryptor_ = NULL;
        }
        if (key.size()) {
            key_ = key;
        }
        CCCryptorStatus status = CCCryptorCreate(kCCEncrypt, kCCAlgorithmAES, options, key_.data(), key_.size(), NULL, &cryptor_);
        ASSERT_EQ(status, kCCSuccess);
    }
};

TEST_F(AESFuncitoalityTest, Basic) {
    ASSERT_NE(key_.size(), 0);
    uint64_t acc = 0;
    std::for_each(key_.begin(), key_.end(), [&acc](uint8_t c) { acc += c; });
    ASSERT_NE(acc, 0);
    decltype(key_) k1 = key_;
    reset({}, 0);
    decltype(key_) k2 = key_;
    decltype(key_) k3;
    k3.resize(kCCKeySizeAES256);
    arc4random_buf(k3.data(), k3.size());
    reset(k3, 0);

    ASSERT_EQ(k1, k2);
    ASSERT_EQ(k3, key_);
}

TEST_F(AESFuncitoalityTest, EncryptDecrypt) {
    using namespace fb2k_ncm::cipher;
    ASSERT_EQ(key_.size(), kCCKeySizeAES128);

    // STD encrypted data: <enc>
    std::vector<uint8_t> enc(aligned(random_plain_.size()));
    ASSERT_FALSE(enc.size() % AES_BLOCKSIZE);
    // STD decrypted data: <dec>
    decltype(enc) dec(enc.size());

    size_t enc_len = 0;
    {
        CCCryptorStatus status = CCCrypt(kCCEncrypt,
                                         kCCAlgorithmAES,
                                         kCCOptionPKCS7Padding,
                                         key_.data(),
                                         key_.size(),
                                         nullptr,
                                         random_plain_.data(),
                                         random_plain_.size(),
                                         enc.data(),
                                         enc.size(),
                                         &enc_len);
        ASSERT_EQ(status, kCCSuccess);
        ASSERT_EQ(enc_len, enc.size());
    }

    size_t dec_len = 0;
    {
        CCCryptorStatus status = CCCrypt(kCCDecrypt,
                                         kCCAlgorithmAES,
                                         kCCOptionPKCS7Padding,
                                         key_.data(),
                                         key_.size(),
                                         nullptr,
                                         enc.data(),
                                         enc.size(),
                                         dec.data(),
                                         dec.size(),
                                         &dec_len);
        ASSERT_EQ(status, kCCSuccess);
        ASSERT_EQ(dec_len, random_plain_.size());
        dec.resize(dec_len);
        ASSERT_EQ(dec, random_plain_);
    }

    {
        // <enc2>: encrypt with grouped operation
        decltype(enc) enc2(aligned(enc.size()));

        CCCryptorRef c;
        CCCryptorStatus status = CCCryptorCreate(kCCEncrypt, kCCAlgorithmAES, kCCOptionPKCS7Padding, key_.data(), key_.size(), nullptr, &c);
        ASSERT_EQ(status, kCCSuccess);
        size_t enc2_len = 0;
        // round1: process 1 chunk
        status = CCCryptorUpdate(c, random_plain_.data(), AES_BLOCKSIZE, enc2.data(), enc2.size(), &enc2_len);
        ASSERT_EQ(status, kCCSuccess);
        ASSERT_EQ(enc2_len, AES_BLOCKSIZE);
        // round2: process remaining
        status = CCCryptorUpdate(c,
                                 random_plain_.data() + AES_BLOCKSIZE,
                                 random_plain_.size() - AES_BLOCKSIZE,
                                 enc2.data() + enc2_len,
                                 enc2.size() - enc2_len,
                                 &enc_len);
        enc2_len += enc_len;
        ASSERT_EQ(status, kCCSuccess);
        // call final at the last chunk
        status = CCCryptorFinal(c, enc2.data() + enc2_len, enc2.size() - enc2_len, &enc_len);
        enc2_len += enc_len;
        enc2.resize(enc2_len);
        ASSERT_EQ(status, kCCSuccess);
        ASSERT_EQ(enc, enc2);
    }

    try {
        // context result of encrypted data: <_enc>
        decltype(enc) _dec(enc.size());
        // of decrypted: <_dec>
        decltype(enc) _enc(enc.size());
        {
            auto context = make_AES_context_with_key(key_);
            ASSERT_FALSE(context.is_done());
            ASSERT_EQ(context.key_bit_len(), key_.size() * 8);
            auto _enc_len = context.set_input(random_plain_)
                                .set_output(_enc)
                                .set_chain_mode(aes_chain_mode::CBC)
                                .encrypt_chunk(AES_BLOCKSIZE)
                                .outputted_len();
            ASSERT_EQ(_enc_len, AES_BLOCKSIZE);
            ASSERT_EQ(0, memcmp(_enc.data(), enc.data(), AES_BLOCKSIZE)); // encrypt chunk == SUCCESS
            _enc_len = context.encrypt_all().outputted_len();
            ASSERT_EQ(_enc_len, enc.size());
            ASSERT_EQ(_enc_len, _enc.size());
            ASSERT_EQ(0, memcmp(_enc.data(), enc.data(), _enc.size())); // encrypt all == SUCCESS
        }
        {
            auto context = make_AES_context_with_key(key_);
            auto _len =
                context.set_input(enc).set_output(_dec).set_chain_mode(aes_chain_mode::CBC).decrypt_chunk(7).decrypt_all().outputted_len();
            ASSERT_EQ(_len, random_plain_.size());
            ASSERT_EQ(memcmp(random_plain_.data(), _dec.data(), _len), 0);
        }

    } catch (const cipher_error &e) {
        FAIL() << e.what();
    }
}
