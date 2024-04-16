#pragma once

#include "common/platform.hpp"
#include <bcrypt.h>
#undef WIN32_NO_STATUS
#pragma comment(lib, "Bcrypt.lib")

#include "common/helpers.hpp"
#include "cipher/exception.hpp"
#include "cipher/utils.hpp"

#include <vector>
#include <sstream>
#include <iomanip>
#include <variant>
#include <memory>

namespace fb2k_ncm::cipher
{
    class AES_context_win32;
    enum class aes_chain_mode {
        ECB,
        CBC,
    };
} // namespace fb2k_ncm::cipher

namespace fb2k_ncm::cipher::details
{

    // AES cipher object handle the crypto state internally
    template <size_t KEYLEN>
    class AES_cipher_win32 {
        friend class AES_context_win32;

    public:
        AES_cipher_win32(const uint8_t (&key)[KEYLEN]) {
            static_assert(KEYLEN == 16 || KEYLEN == 24 || KEYLEN == 32, "AES key length invalid");

            NTSTATUS status = STATUS_UNSUCCESSFUL;
            if (!SUCCESS(status = BCryptOpenAlgorithmProvider(&h_crypt_, BCRYPT_AES_ALGORITHM, NULL, 0))) {
                throw cipher_error("BCryptOpenAlgorithmProvider failed", status);
            }
            load_key(key);
        }
        AES_cipher_win32() {
            NTSTATUS status = STATUS_UNSUCCESSFUL;
            if (!SUCCESS(status = BCryptOpenAlgorithmProvider(&h_crypt_, BCRYPT_AES_ALGORITHM, NULL, 0))) {
                throw cipher_error("BCryptOpenAlgorithmProvider failed", status);
            }
        }
        ~AES_cipher_win32() {
            if (h_key_) {
                BCryptDestroyKey(h_key_);
                h_key_ = NULL;
            }
            if (h_crypt_) {
                BCryptCloseAlgorithmProvider(h_crypt_, 0);
                h_crypt_ = NULL;
            }
        }
        AES_cipher_win32(AES_cipher_win32<KEYLEN> &&tmp) noexcept { operator=(std::move(tmp)); }
        AES_cipher_win32(AES_cipher_win32<KEYLEN> &) = delete;
        void operator=(AES_cipher_win32<KEYLEN> &) = delete;
        void operator=(AES_cipher_win32<KEYLEN> &&tmp) noexcept {
            h_crypt_ = tmp.h_crypt_;
            h_key_ = tmp.h_key_;
            tmp.h_crypt_ = NULL;
            tmp.h_key_ = NULL;
        }

    public:
        void load_key(const uint8_t (&key)[KEYLEN]) {
            return load_key(std::vector<uint8_t>(std::begin(key), std::end(key)));
        }
        void load_key(const std::vector<uint8_t> key) {
            NTSTATUS status = STATUS_UNSUCCESSFUL;
            if (!h_crypt_) {
                throw cipher_error("Crypto provider error", status);
            }
            if (h_key_) {
                throw cipher_error("Key already loaded", status);
            }
            if (key.size() != KEYLEN) {
                throw cipher_error("Invalid key length", status);
            }
            if (!SUCCESS(status =
                             BCryptGenerateSymmetricKey(h_crypt_, &h_key_, NULL, 0, (PUCHAR)key.data(), KEYLEN, 0))) {
                throw cipher_error("BCryptGenerateSymmetricKey failed", status);
            }
        }

    private:
        BCRYPT_ALG_HANDLE h_crypt_ = NULL;
        BCRYPT_KEY_HANDLE h_key_ = NULL;
        constexpr static size_t key_len() { return KEYLEN; }
    };

    // indicates the decrypting (currently encrypt not needed) context and buffer
    class AES_context_win32 {
        template <size_t KEYLEN>
        using AES = AES_cipher_win32<KEYLEN>;
        using AES128 = AES<16>;
        using AES192 = AES<24>;
        using AES256 = AES<32>;

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
        explicit AES_context_win32(
            AES<KEYLEN> &&aes_crypt /* to consturct the context, the crypt object must not be standalone */)
            : cipher_(std::move(aes_crypt)) {
            output_buffer_ = &internal_buffer_; // ensure output pointing to internal buffer at initial state
        }
        AES_context_win32(AES_context_win32 &) = delete;
        void operator=(AES_context_win32 &) = delete;
        AES_context_win32(AES_context_win32 &&tmp) noexcept { operator=(std::move(tmp)); }
        void operator=(AES_context_win32 &&tmp) noexcept {
            cipher_ = std::move(tmp.cipher_);
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
            auto h_key = std::visit([this](auto &&_) { return internal_get_key_handle(_); }, cipher_);
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
            return std::visit([this](const auto &_) { return internal_get_key_length(_); }, cipher_);
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
        std::variant<AES128, AES192, AES256> cipher_;
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

} // namespace fb2k_ncm::cipher::details

namespace fb2k_ncm::cipher::details
{
    template <size_t KEYLEN>
    using AES_cipher_impl = AES_cipher_win32<KEYLEN>;

    using AES_context_impl = AES_context_win32;
} // namespace fb2k_ncm::cipher::details