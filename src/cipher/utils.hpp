#pragma once

namespace fb2k_ncm::cipher
{
    enum { AES_BLOCKSIZE = 16 };
    // calculate aligned size, which is a multiple of AES_BLOCKSIZE (aka 16bytes)
    template <size_t N = AES_BLOCKSIZE>
    inline size_t aligned(size_t v) {
        return 1 + (v | (N - 1));
    }
    // guess the padding (maybe pkcs7) size of
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