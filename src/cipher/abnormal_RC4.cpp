#include "stdafx.h"
#include "abnormal_RC4.hpp"

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
        // no exchange comparing to RC4 PRGA
        // std::swap(key_box[k1],key_box[k2]); <- this step is missing
        auto k = key_box[(key_box[k1] + key_box[k2]) & 0xff];
        key_box_[i] = k;
    }
}

std::function<uint8_t(uint8_t, size_t)> fb2k_ncm::cipher::abnormal_RC4::get_transform() const {
    return [this](uint8_t b, size_t offset) -> uint8_t { return b ^ key_box_[offset & 0xff]; };
}

abnormal_RC4::abnormal_RC4(abnormal_RC4 &c) : key_seed(key_seed_), key_seed_(c.key_seed_), key_box_(c.key_box_), counter_(c.counter_) {}
abnormal_RC4 &abnormal_RC4::operator=(abnormal_RC4 &c) {
    key_seed_ = c.key_seed_;
    key_box_ = c.key_box_;
    counter_ = c.counter_;
    return *this;
}
abnormal_RC4::abnormal_RC4(abnormal_RC4 &&c)
    : key_seed(key_seed_), key_seed_(std::move(c.key_seed_)), key_box_(std::move(c.key_box_)), counter_(c.counter_) {}
abnormal_RC4 &abnormal_RC4::operator=(abnormal_RC4 &&c) {
    key_seed_ = std::move(c.key_seed_);
    key_box_ = std::move(c.key_box_);
    counter_ = c.counter_;
    return *this;
}
