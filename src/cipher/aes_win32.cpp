#include "stdafx.h"
#include "aes_win32.hpp"

using namespace fb2k_ncm::cipher::details;

// validate buffers, ensure state flags
void AES_context_win32::do_prepare() {
    is_done_ = false;
    status_ = STATUS_UNSUCCESSFUL;
    internal_buffer_.clear();

    if (std::holds_alternative<const uint8_t *>(input_buffer_)) { // input by pointer
        auto input_buffer = std::get<const uint8_t *>(input_buffer_);
        if (!input_buffer) {
            throw cipher_error("Invalid input", status_);
        }
        if (!input_buffer_size_) {
            throw cipher_error("Invalid input size", status_);
        }
        if (!input_head_) {
            input_head_ = input_buffer;
        }
    } else { // input by vector
        auto *p = std::get<const std::vector<uint8_t> *>(input_buffer_);
        if (!p) {
            throw cipher_error("Invalid input", status_);
        }
        input_head_ = p->data();
        input_buffer_size_ = p->size();
    }
    if (std::holds_alternative<uint8_t *>(output_buffer_)) { // output to pointed
        auto output_buffer = std::get<uint8_t *>(output_buffer_);
        if (!output_buffer) {
            throw cipher_error("Invalid output", status_);
        }
        if (!output_buffer_size_) {
            throw cipher_error("Invalid output size", status_);
        }
        if (!output_head_) {
            output_head_ = output_buffer;
        }
    } else { // output to vector
        auto *p = std::get<std::vector<uint8_t> *>(output_buffer_);
        if (!p) {
            throw cipher_error("Invalid output", status_);
        }
        p->reserve(aligned(input_buffer_size_));
        output_head_ = p->data();
        output_buffer_size_ = p->size();
    }
    in_progress_ = true;
}

size_t AES_context_win32::do_decrypt(const aes_chain_mode M, uint8_t *dst, const size_t cb_dst, const uint8_t *src, const size_t cb_src) {
    auto h_key = std::visit([this](auto &&_) { return internal_get_key_handle(_); }, cipher_);
    // M is refactored from template argument to function argument
    // so a runtime lookup is needed to replace the compile-time constant, though it's really dumb
    auto get_mapped_str = [M]() {
        switch (M) {
        case aes_chain_mode::ECB:
            return BCRYPT_CHAIN_MODE_ECB;
        case aes_chain_mode::CBC:
            return BCRYPT_CHAIN_MODE_CBC;
        default:
            return BCRYPT_CHAIN_MODE_CBC;
        }
    };
    auto mode_str = get_mapped_str();
    if (!SUCCESS(status_ = BCryptSetProperty(h_key, BCRYPT_CHAINING_MODE, (PUCHAR)mode_str, sizeof(mode_str), 0))) {
        throw cipher_error("BCryptSetProperty set chain mode failed", status_);
    }
    ULONG required_size = 0;
    if (!SUCCESS(status_ = BCryptDecrypt(h_key, (PUCHAR)src, (ULONG)cb_src, NULL, NULL, 0, NULL, 0, &required_size,
                                         0 /*BCRYPT_BLOCK_PADDING*/))) {
        throw cipher_error("Cannot get required size", status_);
    }
    if (!SUCCESS(status_ = BCryptDecrypt(h_key, (PUCHAR)src, (ULONG)cb_src, NULL, NULL, 0, (PUCHAR)dst, (ULONG)cb_dst, &required_size,
                                         0 /*BCRYPT_BLOCK_PADDING*/))) {
        throw cipher_error("BCryptDecrypt failed", status_);
    }
    return required_size;
}

void AES_context_win32::do_finish() {
    // clear and release bound crypt object
    std::visit([this](auto &&_t) { _t = std::decay_t<decltype(_t)>(); }, cipher_);
}