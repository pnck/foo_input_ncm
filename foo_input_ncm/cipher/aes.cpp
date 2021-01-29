#include "aes.h"

using namespace fb2k_ncm::cipher;


// validate buffers, ensure state flags
void fb2k_ncm::cipher::AES_context::prepare() {
    is_done_ = false;
    status_ = STATUS_UNSUCCESSFUL;
    internal_buffer_.clear();

    if (std::holds_alternative<const uint8_t*>(input_buffer_)) {    // input by pointer
        auto input_buffer = std::get<const uint8_t*>(input_buffer_);
        if (!input_buffer) {
            throw cipher_error("Invalid input", status_);
        }
        if (!input_buffer_size_) {
            throw cipher_error("Invalid input size", status_);
        }
        if (!input_head_) {
            input_head_ = input_buffer;
        }
    }
    else { // input by vector
        auto *p = std::get<const std::vector<uint8_t>*>(input_buffer_);
        if (!p) {
            throw cipher_error("Invalid input", status_);
        }
        input_head_ = p->data();
        input_buffer_size_ = p->size();
    }
    if (std::holds_alternative<uint8_t*>(output_buffer_)) { // output to pointed
        auto output_buffer = std::get<uint8_t*>(output_buffer_);
        if (!output_buffer) {
            throw cipher_error("Invalid output", status_);
        }
        if (!output_buffer_size_) {
            throw cipher_error("Invalid output size", status_);
        }
        if (!output_head_) {
            output_head_ = output_buffer;
        }
    }
    else {    // output to vector
        auto *p = std::get<std::vector<uint8_t>*>(output_buffer_);
        if (!p) {
            throw cipher_error("Invalid output", status_);
        }
        p->reserve(aligned(input_buffer_size_));
        output_head_ = p->data();
        output_buffer_size_ = p->size();
    }
    in_progress_ = true;
}

auto fb2k_ncm::cipher::AES_context::set_input(const std::vector<uint8_t>& input) -> decltype(*this) & {
    input_buffer_ = &input;
    input_buffer_size_ = input.size();
    input_head_ = input.data();
    return *this;
}

auto fb2k_ncm::cipher::AES_context::set_input(const uint8_t * input, size_t size) -> decltype(*this) & {
    input_buffer_ = input;
    input_buffer_size_ = size;
    input_head_ = input;
    return *this;
}

auto fb2k_ncm::cipher::AES_context::set_output(std::vector<uint8_t>& output) -> decltype(*this) & {
    output_buffer_ = &output;
    output_head_ = output.data();
    output_buffer_size_ = 0;
    return *this;
}

auto fb2k_ncm::cipher::AES_context::set_output(uint8_t * output, size_t size) -> decltype(*this) & {
    output_buffer_ = output;
    output_buffer_size_ = size;
    output_head_ = output;
    return *this;
}

auto fb2k_ncm::cipher::AES_context::set_chain_mode(aes_chain_mode mode) -> decltype(*this) & {
    chain_mode_ = mode;
    return *this;
}

auto fb2k_ncm::cipher::AES_context::decrypt_all() -> decltype(*this) & {
    if (is_done_) {
        throw cipher_error("Context finished", status_);
    }
    if (!in_progress_) {
        prepare();
    }
    return decrypt_chunk(input_remain());
}

auto fb2k_ncm::cipher::AES_context::decrypt_chunk(size_t chunk_size) -> decltype(*this) & {
    if (is_done_) {
        throw cipher_error("Context finished", status_);
    }
    if (!in_progress_) {
        prepare();
    }
    size_t original_output_buffer_size = output_buffer_size_;
    if (std::holds_alternative<std::vector<uint8_t>*>(output_buffer_)) {
        auto p = std::get <std::vector<uint8_t>*>(output_buffer_);
        auto head_off = output_head_ - p->data();
        p->resize(p->size() + aligned(chunk_size));
        output_buffer_size_ = p->size();
        // resize vector may realloc the space, thus record the offset of output head and reset head from it
        output_head_ = p->data() + head_off;
    }
    if (auto remain = input_remain(); chunk_size > remain) {
        chunk_size = remain;
    }
    size_t result_size = 0;
    switch (chain_mode_)
    {
    case aes_chain_mode::CBC:
        result_size = do_decrypt<aes_chain_mode::CBC>(output_head_, output_remain(), input_head_, chunk_size);
        break;
    case aes_chain_mode::ECB:
        result_size = do_decrypt<aes_chain_mode::ECB>(output_head_, output_remain(), input_head_, chunk_size);
        break;
    }
    last_chunk_size_ = result_size;
    if (std::holds_alternative<std::vector<uint8_t>*>(output_buffer_)) {
        // shrink output vector to exact size
        auto p = std::get<std::vector<uint8_t>*>(output_buffer_);
        p->resize(original_output_buffer_size + result_size);
        output_buffer_size_ = p->size();
    }
    input_head_ += chunk_size;
    output_head_ += result_size;
    if (!input_remain()) {
        finish();
    }
    return *this;
}

auto fb2k_ncm::cipher::AES_context::decrypt_next() -> decltype(*this) & {
    if (!in_progress_ || is_done_) {
        throw cipher_error("Can't repeat decryption", status_);
    }
    return decrypt_chunk(last_chunk_size_);
}


void fb2k_ncm::cipher::AES_context::finish() {
    finished_inputted_ = inputted_len();
    finished_outputted_ = outputted_len();

    is_done_ = true;
    in_progress_ = false;

    input_buffer_ = (const uint8_t*)nullptr;
    input_head_ = nullptr;
    input_buffer_size_ = 0;

    output_buffer_ = &internal_buffer_;
    output_head_ = nullptr;
    output_buffer_size_ = 0;

    last_chunk_size_ = 0;

    // clear and release bound crypt object
    std::visit([this](auto & _t) {_t = std::decay_t<decltype(_t)>(); }, crypt_);
}

// get internal buffer as results
std::vector<uint8_t> fb2k_ncm::cipher::AES_context::copy_buffer_as_vector() {
    auto tmp = std::move(internal_buffer_);
    internal_buffer_.clear();
    return tmp;
}

std::unique_ptr<uint8_t[]> fb2k_ncm::cipher::AES_context::copy_buffer_as_ptr() {
    auto tmp = std::make_unique<uint8_t[]>(internal_buffer_.size());
    std::copy(internal_buffer_.begin(), internal_buffer_.end(), tmp.get());
    return tmp;
}
