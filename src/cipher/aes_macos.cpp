#include "stdafx.h"
#include "aes_macos.hpp"

using namespace fb2k_ncm::cipher::details;

void AES_context_macos::do_prepare() {}

void AES_context_macos::do_finish() {
    // clear and release bound crypt object
    std::visit([this](auto &&_t) { _t = std::decay_t<decltype(_t)>(); }, cipher_);
}

size_t AES_context_macos::do_decrypt(const aes_chain_mode M, uint8_t *dst, const size_t cb_dst, const uint8_t *src, const size_t cb_src) {

    if (!SUCCESS(status_ = std::visit([&](auto &&c) { return c.init(M, kCCDecrypt); }, cipher_))) {
        throw cipher_error("CCCryptorCreate failed", status_);
    }

    auto *cryptor = std::visit([&](auto &&c) { return c.cryptor_; }, cipher_);

    size_t out_size = 0;
    if (!SUCCESS(status_ = CCCryptorUpdate(cryptor, src, cb_src, dst, cb_dst, &out_size))) {
        throw cipher_error("CCCryptorUpdate failed", status_);
    }

    size_t final_size = 0;
    if (!SUCCESS(status_ = CCCryptorFinal(cryptor, dst + out_size, cb_dst - out_size, &final_size))) {
        throw cipher_error("CCCryptorFinal failed", status_);
    }

    return out_size + final_size;
}
