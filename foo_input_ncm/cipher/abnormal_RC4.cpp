#include "stdafx.h"
#include "abnormal_RC4.h"

#include <numeric>
#include <algorithm>

using namespace fb2k_ncm::cipher;

fb2k_ncm::cipher::abnormal_RC4::abnormal_RC4(const uint8_t *seed, size_t len) : abnormal_RC4() {
    key_seed_.assign(seed, seed + len);
    len &= 0xff;
    uint8_t key_box[256];
    for (int i = 0; i < 256; ++i) {
        key_box[i] = i;
    }
    int last = 0;
    for (int i = 0; i < 256; i++) {
        last = (last + key_box[i] + seed[i % len]) & 0xff;
        std::swap(key_box[i], key_box[last]);
    }
    // here is the weired thing, don't think about it, feel it.
    key_box_ = std::shared_ptr<uint8_t[]>(new uint8_t[256]);
    for (int i = 0; i < 256; i++) {
        auto k1 = (i + 1) & 0xff;
        auto k2 = (k1 + key_box[k1]) & 0xff;
        auto k = key_box[(key_box[k1] + key_box[k2]) & 0xff];
        key_box_[i] = k;
    }
}

std::function<uint8_t(uint8_t, size_t)> fb2k_ncm::cipher::abnormal_RC4::get_transform() const {
    return [key_box = this->key_box_](uint8_t _b, size_t offset) -> uint8_t { return _b ^ key_box[offset & 0xff]; };
}

bool fb2k_ncm::cipher::abnormal_RC4::is_valid() const {
    return key_seed_.size() && key_box_;
}

abnormal_RC4::abnormal_RC4(abnormal_RC4 &c) : key_seed(key_seed_), key_seed_(c.key_seed_), key_box_(c.key_box_) {}
abnormal_RC4 &abnormal_RC4::operator=(abnormal_RC4 &c) {
    key_seed_ = c.key_seed_;
    key_box_ = c.key_box_;
    return *this;
}
abnormal_RC4::abnormal_RC4(abnormal_RC4 &&c)
    : key_seed(key_seed_), key_seed_(std::move(c.key_seed_)), key_box_(std::move(c.key_box_)) {}
abnormal_RC4 &abnormal_RC4::operator=(abnormal_RC4 &&c) {
    key_seed_ = std::move(c.key_seed_);
    key_box_ = std::move(c.key_box_);
    return *this;
}