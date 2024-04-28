#pragma once

#include "aes_common.hpp"
#include "cipher/exception.hpp"
#include "cipher/utils.hpp"

#include <bcrypt.h>
#undef WIN32_NO_STATUS
#pragma comment(lib, "Bcrypt.lib")

#include <vector>
#include <sstream>
#include <iomanip>
#include <variant>
#include <memory>

namespace fb2k_ncm::cipher::details
{

    // AES cipher object handle the crypto state internally
    template <size_t KEYLEN>
    class AES_cipher_win32 {
        friend class AES_context_win32;

    public:
        AES_cipher_win32(const uint8_t (&key)[KEYLEN]) {
            static_assert(KEYLEN == 16 || KEYLEN == 24 || KEYLEN == 32, "AES key size invalid");

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
        void load_key(const uint8_t (&key)[KEYLEN]) { return load_key(std::vector<uint8_t>(std::begin(key), std::end(key))); }
        void load_key(const std::vector<uint8_t> key) {
            NTSTATUS status = STATUS_UNSUCCESSFUL;
            if (!h_crypt_) {
                throw cipher_error("Crypto provider error", status);
            }
            if (h_key_) {
                throw cipher_error("Key already loaded", status);
            }
            if (key.size() != KEYLEN) {
                throw cipher_error("Invalid key size", status);
            }
            if (!SUCCESS(status = BCryptGenerateSymmetricKey(h_crypt_, &h_key_, NULL, 0, (PUCHAR)key.data(), KEYLEN, 0))) {
                throw cipher_error("BCryptGenerateSymmetricKey failed", status);
            }
        }

    private:
        BCRYPT_ALG_HANDLE h_crypt_ = NULL;
        BCRYPT_KEY_HANDLE h_key_ = NULL;
        constexpr static size_t key_len() { return KEYLEN; }
    };

    class AES_context_win32 : public AES_context_common {
        using base_t = AES_context_common;
        using clazz = AES_context_win32;

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

    private:
        void do_finish() override;
        size_t do_decrypt(const aes_chain_mode M, uint8_t *dst, const size_t cb_dst, const uint8_t *src, const size_t cb_src) override;
        template <size_t KEYLEN>
        auto inline internal_get_key_handle(AES<KEYLEN> &_) noexcept {
            return _.h_key_;
        }
        template <size_t KEYLEN>
        auto inline internal_get_crypt_handle(AES<KEYLEN> &_) noexcept {
            return _.h_crypt_;
        }
        template <size_t KEYLEN>
        constexpr auto internal_get_key_length(const AES<KEYLEN> &) noexcept {
            return KEYLEN;
        }
        // validate buffers, ensure state flags
        void do_prepare() override;

    public:
        template <size_t KEYLEN>
        explicit AES_context_win32(AES<KEYLEN> &&c /* to consturct the context, the crypt object must not be standalone */) {
            if constexpr (std::is_constructible_v<decltype(cipher_), decltype(c)>) {
                cipher_ = std::move(c);
            } else {
                throw cipher_error("Invalid key size", KEYSIZE_ERROR);
            }
        }
        AES_context_win32(AES_context_win32 &&tmp) noexcept { operator=(std::move(tmp)); }
        void operator=(AES_context_win32 &&tmp) noexcept {
            if (this == &tmp) {
                return;
            }
            base_t::operator=(std::move(tmp));
            cipher_ = std::move(tmp.cipher_);
        }

    public:
        inline size_t key_len() const {
            return std::visit([this](const auto &c) { return c.key_len(); }, cipher_);
        }
        inline auto key_bit_len() const { return 8 * key_len(); }

    public:
        inline auto &set_input(const std::vector<uint8_t> &input) { CHAINED(set_input, input); }
        inline auto &set_input(const uint8_t *input, size_t size) { CHAINED(set_input, input, size); }
        inline auto &set_output(std::vector<uint8_t> &output) { CHAINED(set_output, output); }
        inline auto &set_output(uint8_t *output, size_t size) { CHAINED(set_output, output, size); }
        inline auto &set_chain_mode(aes_chain_mode mode) { CHAINED(set_chain_mode, mode); }
        inline auto &decrypt_all() { CHAINED(decrypt_all); }
        inline auto &decrypt_chunk(size_t chunk_size) { CHAINED(decrypt_chunk, chunk_size); }
        inline auto &decrypt_next() { CHAINED(decrypt_next); }
        inline void finish() { base_t::finish(); }

    private:
        std::variant<AES128, AES192, AES256> cipher_;
    };

} // namespace fb2k_ncm::cipher::details

namespace fb2k_ncm::cipher::details
{
    template <size_t KEYLEN>
    using AES_cipher_impl = AES_cipher_win32<KEYLEN>;

    using AES_context_impl = AES_context_win32;
} // namespace fb2k_ncm::cipher::details
