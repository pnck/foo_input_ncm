#pragma once

#include <vector>
#include <sstream>
#include <iomanip>
#include <variant>
#include <memory>

#include "common/platform.hpp"
#include "common/helpers.hpp"
#include "cipher/exception.hpp"

#include "aes_win32.hpp"

namespace fb2k_ncm::cipher
{

    enum { AES_BLOCKSIZE = 16 };
    inline size_t aligned(size_t v) {
        return 1 + (v | (AES_BLOCKSIZE - 1));
    }

    enum class aes_chain_mode {
        ECB,
        CBC,
    };


    // indicates the decrypting (currently encrypt not needed) context and buffer
    class AES_context {
        template <aes_chain_mode>
        struct enum_mapping_impl;
        template <>
        struct enum_mapping_impl<aes_chain_mode::ECB> {
            constexpr inline static wchar_t value[] = BCRYPT_CHAIN_MODE_ECB;
        };
        template <>
        struct enum_mapping_impl<aes_chain_mode::CBC> {
            constexpr inline static wchar_t value[] = BCRYPT_CHAIN_MODE_CBC;
        };
        template <aes_chain_mode M>
        static inline auto mapped_str = make_mapping_with_impl<M, enum_mapping_impl>::value;

    public:
        template <size_t KEYLEN>
        explicit AES_context(
            AES<KEYLEN> &&aes_crypt /* to consturct the context, the crypt object must not be standalone */)
            : crypt_(std::move(aes_crypt)) {
            output_buffer_ = &internal_buffer_; // ensure output pointing to internal buffer at initial state
        }
        AES_context(AES_context &) = delete;
        void operator=(AES_context &) = delete;
        AES_context(AES_context &&tmp) noexcept { operator=(std::move(tmp)); }
        void operator=(AES_context &&tmp) noexcept {
            crypt_ = std::move(tmp.crypt_);
            chain_mode_ = tmp.chain_mode_;
            internal_buffer_ = std::move(tmp.internal_buffer_);
            input_buffer_ = std::move(tmp.input_buffer_);
            output_buffer_ = std::move(tmp.output_buffer_);
            input_head_ = tmp.input_head_;
            output_head_ = tmp.output_head_;
            input_buffer_size_ = tmp.input_buffer_size_;
            output_buffer_size_ = tmp.output_buffer_size_;
            last_chunk_size_ = tmp.last_chunk_size_;
            status_ = tmp.status_;
            ;
            is_done_ = tmp.is_done_;
            in_progress_ = tmp.in_progress_;
            finished_inputted_ = tmp.finished_inputted_;
            finished_outputted_ = tmp.finished_inputted_;
        }

    private:
        template <aes_chain_mode M>
        void do_encrypt(uint8_t *dst, size_t cb_dst, const uint8_t *src, const size_t cb_src); // dummy stub
        template <aes_chain_mode M>
        size_t do_decrypt(uint8_t *dst, size_t cb_dst, const uint8_t *src, const size_t cb_src) {
            auto h_key = std::visit([this](auto &&_) { return internal_get_key_handle(_); }, crypt_);
            auto __ = mapped_str<M>;
            if (!SUCCESS(status_ = BCryptSetProperty(h_key, BCRYPT_CHAINING_MODE, (PUCHAR)mapped_str<M>,
                                                     sizeof(mapped_str<M>), 0))) {
                throw cipher_error("BCryptSetProperty set chain mode failed", status_);
            }
            ULONG required_size = 0;
            if (!SUCCESS(status_ = BCryptDecrypt(h_key, (PUCHAR)src, cb_src, NULL, NULL, 0, NULL, 0, &required_size,
                                                 0 /*BCRYPT_BLOCK_PADDING*/))) {
                throw cipher_error("Cannot get required size", status_);
            }
            if (!SUCCESS(status_ = BCryptDecrypt(h_key, (PUCHAR)src, cb_src, NULL, NULL, 0, (PUCHAR)dst, cb_dst,
                                                 &required_size, 0 /*BCRYPT_BLOCK_PADDING*/))) {
                throw cipher_error("BCryptDecrypt failed", status_);
            }
            return required_size;
        }
        template <size_t KEYLEN>
        auto internal_get_key_handle(AES<KEYLEN> &_) {
            return _.h_key_;
        }
        template <size_t KEYLEN>
        auto internal_get_crypt_handle(AES<KEYLEN> &_) {
            return _.h_crypt_;
        }
        template <size_t KEYLEN>
        auto internal_get_key_length(const AES<KEYLEN> &) const {
            return KEYLEN;
        }
        // validate buffers, ensure state flags
        void prepare();

    public:
        // status check functionaliy
        // inline size_t buffer_capacity() const { return internal_buffer_.size(); }
        inline aes_chain_mode chain_mode() const { return chain_mode_; }
        inline size_t key_len() const {
            return std::visit([this](const auto &_) { return internal_get_key_length(_); }, crypt_);
        }
        inline size_t key_bit_len() const { return key_len() * 8; }
        inline bool is_done() const { return is_done_; }
        inline size_t inputted_len() const {
            if (is_done_) {
                return finished_inputted_;
            }
            if (std::holds_alternative<const uint8_t *>(input_buffer_)) {
                return input_head_ - std::get<const uint8_t *>(input_buffer_);
            } else {
                return input_head_ - std::get<const std::vector<uint8_t> *>(input_buffer_)->data();
            }
        }
        inline size_t input_remain() const {
            if (is_done_) {
                return 0;
            } else {
                return input_buffer_size_ - inputted_len();
            }
        }
        inline size_t outputted_len() const {
            if (is_done_) {
                return finished_outputted_;
            }
            if (std::holds_alternative<uint8_t *>(output_buffer_)) {
                return output_head_ - std::get<uint8_t *>(output_buffer_);
            } else {
                return output_head_ - std::get<std::vector<uint8_t> *>(output_buffer_)->data();
            }
        }
        inline size_t output_remain() const {
            if (is_done_) {
                return 0;
            } else {
                return output_buffer_size_ - outputted_len();
            }
        }

    public:
        // chained context operations
        auto set_input(const std::vector<uint8_t> &input) -> decltype(*this) &;
        auto set_input(const uint8_t *input, size_t size) -> decltype(*this) &;
        auto set_output(std::vector<uint8_t> &output) -> decltype(*this) &;
        auto set_output(uint8_t *output, size_t size) -> decltype(*this) &;
        auto set_chain_mode(aes_chain_mode mode) -> decltype(*this) &;
        auto decrypt_all() -> decltype(*this) &;
        auto decrypt_chunk(size_t chunk_size) -> decltype(*this) &;
        auto decrypt_next() -> decltype(*this) &;
        void finish();

    public:
        // get internal buffer as results
        std::vector<uint8_t> copy_buffer_as_vector();
        std::unique_ptr<uint8_t[]> copy_buffer_as_ptr();

    private:
        std::variant<AES128, AES192, AES256> crypt_;
        aes_chain_mode chain_mode_;
        std::vector<uint8_t> internal_buffer_;
        // both vector and raw c-style-array supported
        std::variant<const uint8_t *, const std::vector<uint8_t> *> input_buffer_;
        std::variant<uint8_t *, std::vector<uint8_t> *> output_buffer_;
        const uint8_t *input_head_ = nullptr;
        uint8_t *output_head_ = nullptr;
        size_t input_buffer_size_ = 0;
        size_t output_buffer_size_ = 0;
        size_t last_chunk_size_ = 0;
        NTSTATUS status_ = STATUS_UNSUCCESSFUL;
        ;
        bool is_done_ = false;
        bool in_progress_ = false;
        // retain for check after finished
        size_t finished_inputted_ = 0;
        size_t finished_outputted_ = 0;
    };

    // helper functions
    template <size_t KEYLEN>
    [[nodiscard]] inline AES_context make_AES_context_with_key(const uint8_t (&key)[KEYLEN]) {
        return AES_context(AES(key));
    }
    [[nodiscard]] inline AES_context make_AES_context_with_key(const uint8_t *key, size_t key_len) {
        switch (key_len) {
        case 16: {
            AES128 _t;
            _t.load_key(std::vector<uint8_t>{key, key + key_len});
            return AES_context(std::move(_t));
        }
        case 24: {
            AES192 _t;
            _t.load_key(std::vector<uint8_t>{key, key + key_len});
            return AES_context(std::move(_t));
        }
        case 32: {
            AES256 _t;
            _t.load_key(std::vector<uint8_t>{key, key + key_len});
            return AES_context(std::move(_t));
        }
        default:
            throw cipher_error("Invalid key length", STATUS_UNSUCCESSFUL);
        }
    }
    [[nodiscard]] inline AES_context make_AES_context_with_key(const std::vector<uint8_t> &key) {
        return make_AES_context_with_key(key.data(), key.size());
    }
    [[nodiscard]] inline size_t guess_padding(uint8_t *end) {
        uint8_t last = *(end - 1);
        for (uint8_t *p = end - 1; p >= (end - last); --p) {
            if (*p != last) {
                return 0;
            }
        }
        return last;
    }
} // namespace fb2k_ncm::cipher
