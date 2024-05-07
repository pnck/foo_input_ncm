#include "stdafx.h"
#include "aes_macos.hpp"

using namespace fb2k_ncm::cipher::details;

template <size_t KEYLEN>
STATUS AES_cipher_macos<KEYLEN>::init(aes_chain_mode M, CCOperation op) {
    if (cryptor_) {
        return kCCSuccess; // don't init twice (only release when destruct)
    }
    CCOptions option;
    switch (M) {
    case aes_chain_mode::ECB:
        option = kCCOptionECBMode;
        break;
    case aes_chain_mode::CBC:
        option = kCCOptionPKCS7Padding;
        break;
    default:
        return kCCParamError;
    }

    if (key_.size() != key_len()) {
        return kCCKeySizeError;
    }
    auto status = CCCryptorCreate(op, kCCAlgorithmAES, option, key_.data(), key_len(), NULL, &cryptor_);
    return status;
}

void AES_context_macos::do_prepare() {}

void AES_context_macos::do_finish() {
    // clear and release bound crypt object
    std::visit([this](auto &&_t) { _t = std::decay_t<decltype(_t)>(); }, cipher_);
}

size_t AES_context_macos::do_cryptor_update(uint8_t *dst, const size_t cb_dst, const uint8_t *src, const size_t cb_src) {
    auto *cryptor = std::visit([&](auto &&c) { return c.cryptor_; }, cipher_);

    size_t out_size = 0;
    if (status_ = CCCryptorUpdate(cryptor, src, cb_src, dst, cb_dst, &out_size); !SUCCESS(status_)) {
        throw cipher_error("CCCryptorUpdate failed", status_);
    }
    size_t _input_remain = input_remain() - out_size;
    size_t _src_remain = cb_src - out_size;
    if (_src_remain != _input_remain) {
        return out_size;
    }

    // src is containing last chunk

    size_t final_size = 0;
    if (status_ = CCCryptorFinal(cryptor, dst + out_size, cb_dst - out_size, &final_size); !SUCCESS(status_)) {
        throw cipher_error("CCCryptorFinal failed", status_);
    }
    out_size += final_size;
    return out_size;
}

size_t AES_context_macos::do_decrypt(const aes_chain_mode M, uint8_t *dst, const size_t cb_dst, const uint8_t *src, const size_t cb_src) {
    if (!SUCCESS(status_ = std::visit([&](auto &&c) { return c.init(M, kCCDecrypt); }, cipher_))) {
        throw cipher_error("CCCryptorCreate failed", status_);
    }
    return do_cryptor_update(dst, cb_dst, src, cb_src);
}

size_t AES_context_macos::do_encrypt(const aes_chain_mode M, uint8_t *dst, const size_t cb_dst, const uint8_t *src, const size_t cb_src) {
    if (!SUCCESS(status_ = std::visit([&](auto &&c) { return c.init(M, kCCEncrypt); }, cipher_))) {
        throw cipher_error("CCCryptorCreate failed", status_);
    }
    return do_cryptor_update(dst, cb_dst, src, cb_src);
}
