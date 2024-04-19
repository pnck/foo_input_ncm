#pragma once

#include "common/platform.hpp"
#include "common/helpers.hpp"
#include "cipher/exception.hpp"
#include "cipher/utils.hpp"

#include <variant>

namespace fb2k_ncm::cipher
{
    enum class aes_chain_mode {
        ECB,
        CBC,
    };
} // namespace fb2k_ncm::cipher

// indicates the decrypting (currently encrypt not needed) context and buffer
namespace fb2k_ncm::cipher::details
{
    class AES_context_common {
    public:
        virtual ~AES_context_common() = default;

    protected: // cipher-specific operations
        // validate buffers, ensure state flags
        virtual void do_prepare() = 0;
        // additional finish operations
        virtual void do_finish() = 0;
        // virtual size_t do_encrypt(uint8_t *dst, size_t cb_dst, const uint8_t *src, const size_t cb_src) = 0;
        virtual size_t do_decrypt(const aes_chain_mode M, uint8_t *dst, const size_t cb_dst, const uint8_t *src, const size_t cb_src) = 0;

    public:
        AES_context_common() = default;
        AES_context_common(AES_context_common &) = delete;
        void operator=(AES_context_common &) = delete;
        AES_context_common(AES_context_common &&tmp) noexcept { operator=(std::move(tmp)); }
        void operator=(AES_context_common &&tmp) noexcept {
            if (this == &tmp) {
                return;
            }
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
            is_done_ = tmp.is_done_;
            in_progress_ = tmp.in_progress_;
            finished_inputted_ = tmp.finished_inputted_;
            finished_outputted_ = tmp.finished_inputted_;
        }

    protected:
        // chain ops
        void set_input(const std::vector<uint8_t> &input);
        void set_input(std::vector<uint8_t> &&input) = delete; // avoid rvalues
        void set_input(const uint8_t *input, size_t size);
        void set_output(std::vector<uint8_t> &output);
        void set_output(uint8_t *output, size_t size);
        void set_chain_mode(aes_chain_mode mode);
        void decrypt_all();
        void decrypt_chunk(size_t chunk_size);
        void decrypt_next();
        void finish();

    protected:
        template <typename D, typename F, typename... ARGS>
            requires std::derived_from<D, AES_context_common>
        inline D &make_chain_op(F &&f, ARGS &&...args) {
            std::invoke(std::forward<F>(f), std::forward<ARGS>(args)...);
            return static_cast<D &>(*this);
        }
#define CHAINED(F, ...) return make_chain_op<std::decay_t<decltype(*this)>>([&] { AES_context_common::F(__VA_ARGS__); })

    public:
        // properties
        // inline size_t buffer_capacity() const { return internal_buffer_.size(); }
        aes_chain_mode chain_mode() const;
        bool is_done() const;
        size_t inputted_len() const;
        size_t input_remain() const;
        size_t outputted_len() const;
        size_t output_remain() const;

    public:
        // get internal buffer as results
        std::vector<uint8_t> copy_buffer_as_vector();
        std::unique_ptr<uint8_t[]> copy_buffer_as_ptr();

    protected:
        aes_chain_mode chain_mode_;
        std::vector<uint8_t> internal_buffer_;
        // both vector and raw c-style-array are supported
        std::variant<const uint8_t *, const std::vector<uint8_t> *> input_buffer_;
        // ensure output pointing to internal buffer at the initial state
        std::variant<uint8_t *, std::vector<uint8_t> *> output_buffer_ = &internal_buffer_;
        const uint8_t *input_head_ = nullptr;
        uint8_t *output_head_ = nullptr;
        size_t input_buffer_size_ = 0;
        size_t output_buffer_size_ = 0;
        size_t last_chunk_size_ = 0;

        bool is_done_ = false;
        bool in_progress_ = false;
        STATUS status_ = COMMON_ERROR;

        // retain for check after finished
        size_t finished_inputted_ = 0;
        size_t finished_outputted_ = 0;
    };
} // namespace fb2k_ncm::cipher::details
