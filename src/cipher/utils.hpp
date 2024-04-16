#pragma once

namespace fb2k_ncm::cipher
{
    enum { AES_BLOCKSIZE = 16 };
    inline size_t aligned(size_t v) {
        return 1 + (v | (AES_BLOCKSIZE - 1));
    }
} // namespace fb2k_ncm::cipher