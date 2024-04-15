#pragma once

#include <vector>
#include <memory>
#include <functional>

namespace fb2k_ncm::cipher
{
    // special rc4-like BLOCK CIPHER used for ncm files
    class abnormal_RC4 {
    public:
        abnormal_RC4() : key_seed(key_seed_) {}
        abnormal_RC4(const uint8_t *seed, size_t len);
        abnormal_RC4(const std::vector<uint8_t> &seed) : abnormal_RC4(seed.data(), seed.size()) {}
        abnormal_RC4(abnormal_RC4 &c);
        abnormal_RC4 &operator=(abnormal_RC4 &c);
        abnormal_RC4(abnormal_RC4 &&c);
        abnormal_RC4 &operator=(abnormal_RC4 &&c);

    public:
        bool is_valid() const;
        std::function<uint8_t(uint8_t, size_t)> get_transform() const;

    public:
        const std::vector<uint8_t> &key_seed;

    private:
        std::vector<uint8_t> key_seed_;
        std::shared_ptr<uint8_t[]> key_box_;
    };

} // namespace fb2k_ncm::cipher