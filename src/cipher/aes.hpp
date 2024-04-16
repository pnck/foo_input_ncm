#pragma once

#ifdef _WIN32
#include "aes_win32.hpp"
#elif defined __APPLE__
#include "aes_macos.hpp"
#endif

namespace fb2k_ncm::cipher
{
    // interfaces
    template <typename IMPL>
    concept AES_context_functions = requires(IMPL impl) {
        { impl.set_input(std::declval<const std::vector<uint8_t> &>()) } -> std::same_as<decltype(impl) &>;
        { impl.set_input(std::declval<const uint8_t *>(), std::declval<size_t>()) } -> std::same_as<decltype(impl) &>;
        { impl.set_output(std::declval<std::vector<uint8_t> &>()) } -> std::same_as<decltype(impl) &>;
        { impl.set_output(std::declval<uint8_t *>(), std::declval<size_t>()) } -> std::same_as<decltype(impl) &>;
        { impl.set_chain_mode(std::declval<aes_chain_mode>()) } -> std::same_as<decltype(impl) &>;
        { impl.decrypt_all() } -> std::same_as<decltype(impl) &>;
        { impl.decrypt_chunk(std::declval<size_t>()) } -> std::same_as<decltype(impl) &>;
        { impl.decrypt_next() } -> std::same_as<decltype(impl) &>;
        { impl.finish() } -> std::same_as<void>;
    };

    template <typename IMPL>
        requires AES_context_functions<IMPL>
    using AES_context_impl = IMPL;

    using AES_context = AES_context_impl<details::AES_context_impl>;

    template <typename IMPL, size_t KEYLEN>
    concept AES_cipher = requires(IMPL impl) {
        requires requires(const std::vector<uint8_t> &key) {
            { impl.load_key(key) };
        } || requires(const uint8_t(&key)[KEYLEN]) {
            { impl.load_key(key) };
        };
    };

    template <typename IMPL, size_t KEYLEN>
        requires AES_cipher<IMPL, KEYLEN>
    using AES_impl = IMPL;

    template <size_t KEYLEN>
    using AES = AES_impl<details::AES_cipher_impl<KEYLEN>, KEYLEN>;

    using AES128 = AES<16>;
    using AES192 = AES<24>;
    using AES256 = AES<32>;

    // declarations
    enum class aes_chain_mode;

    // helper functions
    template <size_t KEYLEN>
    [[nodiscard]] inline AES_context make_AES_context_with_key(const uint8_t (&key)[KEYLEN]) {
        return AES_context(AES<KEYLEN>(key));
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
