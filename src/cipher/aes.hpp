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
        // chain ops
        { impl.set_input(std::declval<const std::vector<uint8_t> &>()) } -> std::same_as<decltype(impl) &>;
        { impl.set_input(std::declval<const uint8_t *>(), std::declval<size_t>()) } -> std::same_as<decltype(impl) &>;
        { impl.set_output(std::declval<std::vector<uint8_t> &>()) } -> std::same_as<decltype(impl) &>;
        { impl.set_output(std::declval<uint8_t *>(), std::declval<size_t>()) } -> std::same_as<decltype(impl) &>;
        { impl.set_chain_mode(std::declval<aes_chain_mode>()) } -> std::same_as<decltype(impl) &>;
        { impl.decrypt_all() } -> std::same_as<decltype(impl) &>;
        { impl.decrypt_chunk(std::declval<size_t>()) } -> std::same_as<decltype(impl) &>;
        { impl.decrypt_next() } -> std::same_as<decltype(impl) &>;
        { impl.finish() };
        // properties
        { impl.chain_mode() } -> std::same_as<aes_chain_mode>;
        { impl.key_len() } -> std::same_as<size_t>;
        { impl.key_bit_len() } -> std::same_as<size_t>;
        { impl.is_done() } -> std::same_as<bool>;
        { impl.inputted_len() } -> std::same_as<size_t>;
        { impl.input_remain() } -> std::same_as<size_t>;
        { impl.outputted_len() } -> std::same_as<size_t>;
        { impl.output_remain() } -> std::same_as<size_t>;
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
        AES<KEYLEN> c;
        c.load_key(key);
        return AES_context(std::move(c));
    }
    [[nodiscard]] inline AES_context make_AES_context_with_key(const uint8_t *key, size_t key_len) {
        switch (key_len) {
        case 16: {
            AES128 c;
            c.load_key(std::vector<uint8_t>(key, key + 16));
            return AES_context(std::move(c));
        }
        case 24: {
            AES192 c;
            c.load_key(std::vector<uint8_t>(key, key + 24));
            return AES_context(std::move(c));
        }
        case 32: {
            AES256 c;
            c.load_key(std::vector<uint8_t>(key, key + 32));
            return AES_context(std::move(c));
        }
        default:
            throw cipher_error("Invalid key size", KEYSIZE_ERROR);
        }
    }
    [[nodiscard]] inline AES_context make_AES_context_with_key(const std::vector<uint8_t> &key) {
        return make_AES_context_with_key(key.data(), key.size());
    }
} // namespace fb2k_ncm::cipher
