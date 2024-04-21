#pragma once

#include "stdafx.h"
#include "cipher/cipher.h"
#include "rapidjson/include/rapidjson/document.h"

#include <fstream>

namespace fb2k_ncm
{

    class ncm_file : public file {
    public:
        enum parse_contents {
            NCM_PARSE_META = 0b1,
            NCM_PARSE_ALBUM = 0b10,
            NCM_PARSE_AUDIO = 0b100,
        };

    public:
        t_size read(void *p_buffer, t_size p_bytes, abort_callback &p_abort) override;
        void write(const void *p_buffer, t_size p_bytes, abort_callback &p_abort) override;
        t_filesize get_size(abort_callback &p_abort) override;
        t_filesize get_position(abort_callback &p_abort) override;
        void resize(t_filesize p_size, abort_callback &p_abort) override;
        void seek(t_filesize p_position, abort_callback &p_abort) override;
        bool can_seek() override;
        bool get_content_type(pfc::string_base &p_out) override;
        void reopen(abort_callback &p_abort) override;
        bool is_remote() override;
        t_filetimestamp get_timestamp(abort_callback &p_abort) override;

    public:
        explicit ncm_file(const char *path) : this_path_(path) {
            filesystem::g_open(source_, path, filesystem::open_mode_read, fb2k::noAbort);
        }
        void ensure_audio_offset();
        void parse(uint16_t to_parse = 0xffff);

        inline auto path() const { return this_path_; }
        inline bool meta_parsed() const { return !meta_str_.empty(); }
        inline bool audio_parsed() const { return !parsed_file_.audio_content_offset; }

    private:
        inline void throw_format_error(const char *extra = nullptr);
        inline void throw_format_error(std::string extra);

    public:
        inline auto &meta_info() { return meta_json_; }
        inline auto &image_data() { return image_data_; }

    private:
        const char *this_path_;
        ncm_file_st parsed_file_;
        file_ptr source_;
        // std::unique_ptr<std::basic_fstream<char>> source_;
        // using fschar = decltype(source_)::element_type::char_type;

        std::string meta_str_;
        rapidjson::Document meta_json_;
        cipher::abnormal_RC4 rc4_decrypter_;
        std::vector<uint8_t> image_data_;
        std::vector<uint8_t> cache_;
    };

} // namespace fb2k_ncm
