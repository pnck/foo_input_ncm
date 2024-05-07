#include "stdafx.h"
#include "aes_common.hpp"

using namespace fb2k_ncm::cipher::details;
using namespace fb2k_ncm::cipher;

// get internal buffer as results
std::vector<uint8_t> AES_context_common::copy_buffer_as_vector() {
    auto tmp = std::move(internal_buffer_);
    internal_buffer_.clear();
    return tmp;
}

std::unique_ptr<uint8_t[]> AES_context_common::copy_buffer_as_ptr() {
    auto tmp = std::make_unique<uint8_t[]>(internal_buffer_.size());
    std::copy(internal_buffer_.begin(), internal_buffer_.end(), tmp.get());
    return tmp;
}

// properties
aes_chain_mode AES_context_common::chain_mode() const {
    return chain_mode_;
}
bool AES_context_common::is_done() const {
    return is_done_;
}
size_t AES_context_common::inputted_len() const {
    if (is_done_) {
        return finished_inputted_;
    }
    if (std::holds_alternative<const uint8_t *>(input_buffer_)) {
        return input_head_ - std::get<const uint8_t *>(input_buffer_);
    } else {
        return input_head_ - std::get<const std::vector<uint8_t> *>(input_buffer_)->data();
    }
}
size_t AES_context_common::input_remain() const {
    if (is_done_) {
        return 0;
    } else {
        return input_buffer_size_ - inputted_len();
    }
}
size_t AES_context_common::outputted_len() const {
    if (is_done_) {
        return finished_outputted_;
    }
    if (std::holds_alternative<uint8_t *>(output_buffer_)) {
        return std::distance(std::get<uint8_t *>(output_buffer_), output_head_);
    } else {
        return std::distance(std::get<std::vector<uint8_t> *>(output_buffer_)->data(), output_head_);
    }
}
size_t AES_context_common::output_remain() const {
    if (is_done_) {
        return 0;
    } else {
        return output_buffer_size_ - outputted_len();
    }
}

// chain ops
void AES_context_common::set_input(const std::vector<uint8_t> &input) {
    input_buffer_ = &input;
    input_buffer_size_ = input.size();
    input_head_ = input.data();
}

void AES_context_common::set_input(const uint8_t *input, size_t size) {
    input_buffer_ = input;
    input_buffer_size_ = size;
    input_head_ = input;
}

void AES_context_common::set_output(std::vector<uint8_t> &output) {
    output_buffer_ = &output;
    output_head_ = output.data();
    output_buffer_size_ = 0;
}

void AES_context_common::set_output(uint8_t *output, size_t size) {
    output_buffer_ = output;
    output_buffer_size_ = size;
    output_head_ = output;
}

void AES_context_common::set_chain_mode(aes_chain_mode mode) {
    chain_mode_ = mode;
}

void AES_context_common::decrypt_next() {
    if (!in_progress_ || is_done_) {
        throw cipher_error("Can't repeat decryption", status_);
    }
    return decrypt_chunk(last_chunk_size_);
}

void AES_context_common::decrypt_all() {
    if (is_done_) {
        throw cipher_error("Context finished", status_);
    }
    if (!in_progress_) {
        do_prepare();
    }
    do {
        decrypt_chunk(input_remain());
    } while (input_remain());
}

void AES_context_common::encrypt_next() {
    if (!in_progress_ || is_done_) {
        throw cipher_error("Can't repeat encryption", status_);
    }
    return encrypt_chunk(last_chunk_size_);
}

void AES_context_common::encrypt_all() {
    if (is_done_) {
        throw cipher_error("Context finished", status_);
    }
    if (!in_progress_) {
        do_prepare();
    }
    do {
        encrypt_chunk(input_remain());
    } while (input_remain());
}

template <AES_context_common::OP op>
void AES_context_common::universal_chunk_op(size_t chunk_size) {
    if (is_done_) {
        throw cipher_error("Context finished", status_);
    }
    if (!in_progress_) {
        do_prepare();
        in_progress_ = true;
    }
    size_t stashed_output_buffer_size = output_buffer_size_;
    if (std::holds_alternative<std::vector<uint8_t> *>(output_buffer_)) {
        auto p = std::get<std::vector<uint8_t> *>(output_buffer_);
        auto output_head_n = std::distance(p->data(), output_head_);
        p->resize(outputted_len() + aligned(chunk_size));
        output_buffer_size_ = p->size();
        // resize vector may realloc the space, thus record the offset of output head and reset head from it
        output_head_ = p->data() + output_head_n;
    }
    if (auto remain = input_remain(); chunk_size > remain) {
        chunk_size = remain;
    }
    size_t result_size = 0;
    switch (op) {
    case OP::DEC:
        result_size = do_decrypt(chain_mode_, output_head_, output_remain(), input_head_, chunk_size);
        break;
    case OP::ENC:
        result_size = do_encrypt(chain_mode_, output_head_, output_remain(), input_head_, chunk_size);
        break;
    }
    last_chunk_size_ = result_size;
    if (std::holds_alternative<std::vector<uint8_t> *>(output_buffer_)) {
        // shrink output vector to exact size
        auto p = std::get<std::vector<uint8_t> *>(output_buffer_);
        p->resize(stashed_output_buffer_size + result_size);
        output_buffer_size_ = p->size();
    }
    input_head_ += chunk_size;
    output_head_ += result_size;
    if (!input_remain()) {
        finish();
    }
}

void AES_context_common::decrypt_chunk(size_t chunk_size) {
    universal_chunk_op<OP::DEC>(chunk_size);
}

void AES_context_common::encrypt_chunk(size_t chunk_size) {
    universal_chunk_op<OP::ENC>(chunk_size);
}

void AES_context_common::finish() {
    finished_inputted_ = inputted_len();
    finished_outputted_ = outputted_len();

    is_done_ = true;
    in_progress_ = false;

    input_buffer_ = static_cast<const uint8_t *>(nullptr);
    input_head_ = nullptr;
    input_buffer_size_ = 0;

    output_buffer_ = &internal_buffer_;
    output_head_ = nullptr;
    output_buffer_size_ = 0;

    last_chunk_size_ = 0;

    // call inherited finish
    do_finish();
}
