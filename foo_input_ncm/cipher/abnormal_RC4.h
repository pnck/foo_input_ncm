#pragma once

#include <vector>
#include <memory>

namespace fb2k_ncm::cipher {
    class abnormal_RC4
    {
    public:
        abnormal_RC4() = default;
        abnormal_RC4(const uint8_t* seed, size_t len);
        abnormal_RC4(const std::vector<uint8_t> & seed) :abnormal_RC4(seed.data(), seed.size()) {}

        // private:
        std::vector<uint8_t> key_seed_;
        std::shared_ptr<uint8_t[]> key_box_; 
    };

}