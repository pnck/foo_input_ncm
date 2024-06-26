#pragma once

#include "stdafx.h"
#include "common/consts.hpp"
#include "cipher/cipher.h"
#include "nlohmann/json.hpp"

#include <fstream>
#include <string_view>
#include <stdexcept>

namespace fb2k_ncm
{
    using json_t = nlohmann::json;

    /// @note
    /// - FB2K_MAKE_SERVICE_INTERFACE() is used for creating service interfaces,
    /// which usually contain only pure virtual function declarations.
    /// But we do so in an implementation class because the interface constraint is essential for service_ptr casting.
    /// To be specific, without the interface declaration,
    /// service_ptr<file> will not be able to cast to our service_ptr<ncm_file>.
    /// @note
    /// - And another consideration is that although ncm_file is an implementation class, but it's still abstract.
    /// Service classes are actually unable to be instantiated until it's wrapped into service_impl_t<>.
    /// So making it a service interface won't break any abstraction design, but can extreamly simplify our code.
    class ncm_file : public file {
        FB2K_MAKE_SERVICE_INTERFACE(ncm_file, file);

        struct ncm_file_parsed_st : public ncm_file_st {
            // ...
            // end of original file structure

            // offsets inside the file - pointers are actually not used
            // offsets are always parsed no matter what target is specified
            uint64_t rc4_seed_offset;
            uint64_t meta_offset;
            uint64_t album_image_offset;
            uint64_t audio_content_offset;
        };

    public:
        enum parse_targets : uint16_t {
            NCM_PARSE_META = 0b1,
            NCM_PARSE_ALBUM = 0b10,
            NCM_PARSE_AUDIO = 0b100,
        };

    public:
        t_size read(void *p_buffer, t_size p_bytes, abort_callback &p_abort);
        void write(const void *p_buffer, t_size p_bytes, abort_callback &p_abort);
        t_filesize get_size(abort_callback &p_abort);
        t_filesize get_position(abort_callback &p_abort);
        void resize(t_filesize p_size, abort_callback &p_abort);
        void seek(t_filesize p_position, abort_callback &p_abort);
        bool can_seek();
        bool get_content_type(pfc::string_base &p_out);
        void reopen(abort_callback &p_abort);
        bool is_remote();
        t_filetimestamp get_timestamp(abort_callback &p_abort);

    public:
        explicit ncm_file(const char *path, filesystem::t_open_mode open_mode = filesystem::open_mode_read) : this_path_(path) {
            filesystem::g_open(source_, path, open_mode, fb2k::noAbort);
        }
        void parse(uint16_t to_parse = 0xffff);
        bool save_raw_audio(const char *to_dir, abort_callback &p_abort = fb2k::noAbort);
        void overwrite_meta(const nlohmann::json &overwrite, abort_callback &p_abort = fb2k::noAbort);
        void reset_album_image(album_art_data_ptr image, abort_callback &p_abort = fb2k::noAbort);

    private:
        inline void throw_format_error(const char *extra = nullptr);
        inline void throw_format_error(const std::string &extra);
        inline void ensure_audio_offset();
        inline void ensure_decryptor();
        [[nodiscard]] auto make_seek_guard(abort_callback &p_abort = fb2k::noAbort);

    public:
        inline auto &meta_info() { return meta_json_; }
        inline auto &image_data() { return album_image_data_; }
        inline auto path() const { return this_path_; }
        inline bool meta_parsed() const { return meta_str_.size() > 0; }
        inline bool audio_key_parsed() const { return rc4_decryptor_.is_valid(); }
        inline bool album_image_parsed() const { return album_image_data_.capacity() > 0; }
        inline std::string_view saved_raw_path() const { return path_raw_saved_to_; }

    private:
        const char *this_path_ = nullptr;
        ncm_file_parsed_st parsed_file_{};
        file_ptr source_;
        std::string meta_str_;
        nlohmann::json meta_json_;
        cipher::abnormal_RC4 rc4_decryptor_;
        std::vector<uint8_t> album_image_data_;
        std::string path_raw_saved_to_;
    };

    FOOGUIDDECL constexpr GUID ncm_file::class_guid = guid_candidates[1];

} // namespace fb2k_ncm
