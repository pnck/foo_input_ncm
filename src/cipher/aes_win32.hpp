#pragma once

#include "common/platform.hpp"
#include <bcrypt.h>
#undef WIN32_NO_STATUS
#pragma comment(lib, "Bcrypt.lib")

#include "common/helpers.hpp"
#include "cipher/exception.hpp"

#include <vector>
#include <sstream>
#include <iomanip>
#include <variant>
#include <memory>

namespace fb2k_ncm::cipher
{
    class AES_context;
}

namespace fb2k_ncm::cipher::details
{

    enum { AES_BLOCKSIZE = 16 };
    inline size_t aligned(size_t v) {
        return 1 + (v | (AES_BLOCKSIZE - 1));
    }

    enum class aes_chain_mode {
        ECB,
        CBC,
    };

    // AES cipher object handle the crypto state internally
    template <size_t KEYLEN>
    class AES_impl_WIN32 {
        friend class fb2k_ncm::cipher::AES_context;

    public:
        AES_impl_WIN32(const uint8_t (&key)[KEYLEN]) {
            static_assert(KEYLEN == 16 || KEYLEN == 24 || KEYLEN == 32, "AES key length invalid");

            NTSTATUS status = STATUS_UNSUCCESSFUL;
            if (!SUCCESS(status = BCryptOpenAlgorithmProvider(&h_crypt_, BCRYPT_AES_ALGORITHM, NULL, 0))) {
                throw cipher_error("BCryptOpenAlgorithmProvider failed", status);
            }
            load_key(key);
        }
        AES_impl_WIN32() {
            NTSTATUS status = STATUS_UNSUCCESSFUL;
            if (!SUCCESS(status = BCryptOpenAlgorithmProvider(&h_crypt_, BCRYPT_AES_ALGORITHM, NULL, 0))) {
                throw cipher_error("BCryptOpenAlgorithmProvider failed", status);
            }
        }
        ~AES_impl_WIN32() {
            if (h_key_) {
                BCryptDestroyKey(h_key_);
                h_key_ = NULL;
            }
            if (h_crypt_) {
                BCryptCloseAlgorithmProvider(h_crypt_, 0);
                h_crypt_ = NULL;
            }
        }
        AES_impl_WIN32(AES_impl_WIN32<KEYLEN> &&tmp) noexcept { operator=(std::move(tmp)); }
        AES_impl_WIN32(AES_impl_WIN32<KEYLEN> &) = delete;
        void operator=(AES_impl_WIN32<KEYLEN> &) = delete;
        void operator=(AES_impl_WIN32<KEYLEN> &&tmp) noexcept {
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

} // namespace fb2k_ncm::cipher::details

namespace fb2k_ncm::cipher
{
    template <size_t KEYLEN>
    using AES = details::AES_impl_WIN32<KEYLEN>;
    using AES128 = details::AES_impl_WIN32<16>;
    using AES192 = details::AES_impl_WIN32<24>;
    using AES256 = details::AES_impl_WIN32<32>;
} // namespace fb2k_ncm::cipher