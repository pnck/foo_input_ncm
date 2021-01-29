#pragma once

#include <stdint.h>

struct _check_cpp17 {
    constexpr static inline bool _check() {
        // if assert failed, add `/Zc:__cplusplus`  to project property -> C/C++ -> command line
        static_assert(__cplusplus >= 201703L, "c++17 standard required");
        return true;
    }
};

namespace fb2k_ncm {
    constexpr uint64_t ncm_magic = 0x4d4144464e455443; // CTENFDAM
    constexpr uint8_t ncm_rc4_seed_aes_key[] = { 0x68, 0x7A, 0x48, 0x52, 0x41, 0x6D, 0x73, 0x6F, 0x35, 0x6B, 0x49, 0x6E, 0x62, 0x61, 0x78, 0x57 }; // the AES key used to encrypt its RC4 seed
    constexpr uint8_t ncm_meta_aes_key[] = { 0x23, 0x31, 0x34, 0x6C, 0x6A, 0x6B, 0x5F, 0x21, 0x5C, 0x5D, 0x26, 0x30, 0x55, 0x3C, 0x27, 0x28 }; // the AES key used to encrypt meta info

    struct ncm_file_st {
        uint8_t magic[8];
        uint8_t unknown_padding[2];
        uint32_t rc4_key_len;
        uint8_t* rc4_key;
        uint32_t meta_len;
        uint8_t* meta_content;
        uint8_t  unknown_data[5];
        uint32_t album_image_size[2]; // there are 2 exactly same field
        uint8_t* album_image;
        uint8_t* audio_file;
    };
}