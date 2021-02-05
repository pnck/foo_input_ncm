#include "stdafx.h"
#include "abnormal_RC4.h"

#include <numeric>
#include <algorithm>

using namespace fb2k_ncm::cipher;

fb2k_ncm::cipher::abnormal_RC4::abnormal_RC4(const uint8_t* seed, size_t len) {
    key_seed_.assign(seed, seed + len);
    len &= 0xff;
    uint8_t key_box[256];
    for (int i = 0; i < 256; ++i) {
        key_box[i] = i;
    }
    int last = 0;
    for (int i = 0; i < 256; i++) {
        last = (last + key_box[i] + seed[i% len]) & 0xff;
        std::swap(key_box[i], key_box[last]);
    }
    // here is the weired thing, dont think about it, feel it.
    key_box_ = std::shared_ptr<uint8_t[]>(new uint8_t[256]);
    for (int i = 0; i < 256; i++) {
        auto k1 = (i + 1) & 0xff;
        auto k2 = (k1 + key_box[k1]) & 0xff;
        auto k = key_box[(key_box[k1] + key_box[k2]) & 0xff];
        key_box_[i] = k;
    }

}
