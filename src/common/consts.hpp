#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>

#include "platform.hpp"

constexpr inline GUID guid_candidates[] = {
    {0x0ef1cb99, 0x91ad, 0x486c, {0x82, 0x82, 0xda, 0x70, 0x98, 0x4e, 0xa8, 0x51}}, // input_ncm service
    {0x9c99d51e, 0x1228, 0x4f25, {0x91, 0x63, 0xf1, 0x56, 0x1e, 0x57, 0x5b, 0x13}}, // ncm_file service
    {0xdb2c5ae1, 0x1a4c, 0x4c67, {0xb4, 0x13, 0xc9, 0xd9, 0x46, 0x34, 0xe2, 0xaf}}, // context menu
    {0xc2cb5fa6, 0x9d9f, 0x47ec, {0xae, 0x3a, 0x18, 0x5f, 0xc7, 0x98, 0xd6, 0x2c}},
};

struct _check_cpp_std {
    consteval static inline bool _check() {
        // if assert failed, add `/Zc:__cplusplus`  to project property -> C/C++ -> command line
        static_assert(__cplusplus >= 202002L, "c++20 standard required");
        return true;
    }
};

namespace fb2k_ncm
{
    constexpr uint64_t ncm_magic = 0x4d4144464e455443; // CTENFDAM
    constexpr uint8_t ncm_rc4_seed_aes_key[] = {
        0x68, 0x7A, 0x48, 0x52, 0x41, 0x6D, 0x73, 0x6F, 0x35, 0x6B, 0x49, 0x6E, 0x62, 0x61, 0x78, 0x57}; // the AES key used to encrypt its
                                                                                                         // RC4 seed
    constexpr uint8_t ncm_meta_aes_key[] = {
        0x23, 0x31, 0x34, 0x6C, 0x6A, 0x6B, 0x5F, 0x21, 0x5C, 0x5D, 0x26, 0x30, 0x55, 0x3C, 0x27, 0x28}; // the AES key used to encrypt meta
                                                                                                         // info

    /// @attention Don't use offsetof() macro due to the alignment issue
    struct ncm_file_st {
        uint64_t magic = ncm_magic;
        uint8_t unknown_padding[2];
        uint32_t rc4_seed_len;   // length of rc4_seed *field*, padding included
        const uint8_t *rc4_seed; // encrypted by AES, "neteasecloudmusic..."
        uint32_t meta_len;
        const uint8_t *meta_content; // base64 encoding, "163 key(Don't modify):${(AES encrypted)}"
        uint8_t unknown_data[5];
        uint32_t album_image_size[2]; // there are 2 exactly same field
        const uint8_t *album_image;
        const uint8_t *audio_content;
    };

    constexpr int max_thread_count = 8; // recommended number of threads (hint)

} // namespace fb2k_ncm

namespace fb2k_ncm
{

    namespace
    {
        template <typename T = std::string>
        using single_v_map = std::unordered_map<std::string, std::string>;

        template <typename T = std::string>
        using multi_v_map = std::unordered_map<std::string, std::vector<std::string>>;

        template <typename T = std::string>
        using single = T;

        template <typename T = std::string>
        using multi = std::unordered_set<T>;

        template <typename T = std::string>
        using optional = std::optional<T>;
    } // namespace

    // handling different structures of meta info is catastrophic!
    // we need a unified structure to represent them
    struct uniform_meta_st {
        // fb2k
        optional<std::unordered_map<std::string, uint64_t>> artist; // <artist name, artis id>

        optional<single<>> title; // aka musicName
        optional<single<>> album;
        optional<single<>> date; // aka year
        optional<multi<>> genre;
        optional<multi<>> composer;
        optional<multi<>> performer;
        optional<multi<>> album_artist;
        optional<single<>> track_number;
        optional<single<>> total_tracks;
        optional<single<>> disc_number;
        optional<single<>> total_discs;
        optional<single<>> comment;
        // ncm
        optional<single<uint64_t>> musicId;
        optional<single<uint64_t>> albumId;
        optional<single<>> albumPicDocId;
        optional<single<>> albumPic; // http link
        optional<single<>> mp3DocId;
        optional<single<uint64_t>> mvId;
        optional<single<uint64_t>> bitrate;  // should be info, not meta
        optional<single<uint64_t>> duration; // should be info, not meta
        optional<single<>> format;
        optional<multi<>> alias;
        optional<multi<>> transNames; // translated titles

        single_v_map<> extra_single_values;
        multi_v_map<> extra_multi_values;
    };
} // namespace fb2k_ncm