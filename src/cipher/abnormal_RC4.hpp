#pragma once

#include <vector>
#include <memory>
#include <functional>

#include <ranges>
#include <algorithm>

namespace fb2k_ncm::cipher
{
    // special rc4-like BLOCK CIPHER used for ncm files
    class abnormal_RC4 {
    public:
        abnormal_RC4() : key_seed(key_seed_) {}
        abnormal_RC4(const uint8_t *seed, size_t len);
        abnormal_RC4(const std::vector<uint8_t> &seed) : abnormal_RC4(seed.data(), seed.size()) {}
        abnormal_RC4(const abnormal_RC4 &c);
        abnormal_RC4 &operator=(const abnormal_RC4 &c);
        abnormal_RC4(abnormal_RC4 &&c);
        abnormal_RC4 &operator=(abnormal_RC4 &&c);

    public:
        inline bool is_valid() const { return key_seed_.size() && key_box_; }
        inline auto &reset_counter(size_t offset = 0) {
            counter_ = offset;
            return *this;
        }
        std::function<uint8_t(uint8_t, size_t)> get_transform() const;

        // c++20 ranges version, returns a transform view
        template <std::ranges::range R>
        auto transform(const R &r) {
            return r | std::views::all | std::views::transform([tf = get_transform(), this](auto b) { return tf(b, counter_++); });
        }

    public:
        const std::vector<uint8_t> &key_seed;

    private:
        std::vector<uint8_t> key_seed_;
        std::shared_ptr<uint8_t[]> key_box_;
        size_t counter_ = 0; // to keep decrypt indices on track
    };

} // namespace fb2k_ncm::cipher
