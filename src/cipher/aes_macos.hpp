#pragma once

#include "aes_common.hpp"
#include <CommonCrypto/CommonCrypto.h>

#include <vector>
#include <sstream>
#include <iomanip>
#include <variant>
#include <memory>
#include <format>
#include <ranges>

namespace fb2k_ncm::cipher::details
{

    template <size_t KEYLEN>
    class AES_cipher_macos {
        friend class AES_context_macos;

    public:
        AES_cipher_macos(const uint8_t (&key)[KEYLEN]) {
            static_assert(KEYLEN == 16 || KEYLEN == 32, "AES key size invalid");
            load_key(key);
        }
        AES_cipher_macos() {}
        ~AES_cipher_macos() {
            if (cryptor_) {
                CCCryptorRelease(cryptor_);
                cryptor_ = NULL;
            }
        }
        AES_cipher_macos(AES_cipher_macos<KEYLEN> &) = delete;
        void operator=(AES_cipher_macos<KEYLEN> &) = delete;

        AES_cipher_macos(AES_cipher_macos<KEYLEN> &&tmp) noexcept { operator=(std::move(tmp)); }
        void operator=(AES_cipher_macos<KEYLEN> &&tmp) noexcept {
            cryptor_ = tmp.cryptor_;
            key_ = std::move(tmp.key_);
            tmp.cryptor_ = NULL;
        }

    public:
        void load_key(const uint8_t (&key)[KEYLEN]) { return load_key(std::vector<uint8_t>(std::begin(key), std::end(key))); }
        void load_key(const std::vector<uint8_t> key) {
            if (cryptor_) {
                throw cipher_error("Cryptor already in used", kCCParamError);
            }
            if (key.size() != KEYLEN) {
                throw cipher_error("Invalid key size", kCCKeySizeError);
            }
            key_ = key;
        }

    private:
        CCCryptorRef cryptor_ = NULL;
        std::vector<uint8_t> key_;
        constexpr static size_t key_len() noexcept { return KEYLEN; }

        [[deprecated("Algorithm is always kCCAlgorithmAES, key size is specified otherwhere")]] constexpr static CCAlgorithm
        algorithm() noexcept {
            if constexpr (KEYLEN == 16) {
                return kCCAlgorithmAES128;
            } else if (KEYLEN == 32) {
                return kCCAlgorithmAES;
            }
            return kCCAlgorithmAES;
        }

        STATUS init(aes_chain_mode M, CCOperation op) {
            CCOptions option;
            switch (M) {
            case aes_chain_mode::ECB:
                option = kCCOptionECBMode;
                break;
            case aes_chain_mode::CBC:
                option = 0;
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
    };

    class AES_context_macos : public AES_context_common {
        using base_t = AES_context_common;

        template <size_t KEYLEN>
        using AES = AES_cipher_macos<KEYLEN>;
        using AES128 = AES<16>;
        using AES256 = AES<32>;

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

    public:
        template <size_t KEYLEN>
        explicit AES_context_macos(AES_cipher_macos<KEYLEN> &&c) : base_t() {
            if constexpr (std::is_constructible_v<decltype(cipher_), decltype(c)>) {
                cipher_ = std::move(c);
            } else {
                throw cipher_error("Invalid key size", KEYSIZE_ERROR);
            }
        }
        AES_context_macos(AES_context_macos &) = delete;
        void operator=(AES_context_macos &) = delete;
        AES_context_macos(AES_context_macos &&tmp) noexcept { operator=(std::move(tmp)); }
        void operator=(AES_context_macos &&tmp) noexcept {
            if (this == &tmp) {
                return;
            }
            base_t::operator=(std::move(tmp));
            cipher_ = std::move(tmp.cipher_);
        }

    public:
        inline size_t key_len() const {
            return std::visit([](const auto &c) { return c.key_len(); }, cipher_);
        }
        inline auto key_bit_len() const { return 8 * key_len(); }

    private:
        std::variant<AES128, AES256> cipher_;
        void do_prepare() override;
        void do_finish() override;
        size_t do_decrypt(const aes_chain_mode M, uint8_t *dst, const size_t cb_dst, const uint8_t *src, const size_t cb_src) override;
    };
} // namespace fb2k_ncm::cipher::details

namespace fb2k_ncm::cipher::details
{
    template <size_t KEYLEN>
    using AES_cipher_impl = AES_cipher_macos<KEYLEN>;

    using AES_context_impl = AES_context_macos;
} // namespace fb2k_ncm::cipher::details
