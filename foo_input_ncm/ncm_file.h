#pragma once
#include "sdk/foobar2000/SDK/foobar2000.h"
#include "cipher/abnormal_RC4.h"
#include "common/define.h"
#include "rapidjson/include/rapidjson/document.h"

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
        explicit ncm_file(file_ptr &source, const char *path)
            : source_(source), this_path_(path), meta_info(meta_json_), image_data(image_data_) {}
        const char *path() const { return this_path_; }
        void ensure_audio_offset();
        void parse(uint16_t to_parse = 0xffff);

    private:
        inline void throw_format_error();

    public:
        file_ptr source_;
        const rapidjson::Document &meta_info;
        const std::vector<uint8_t> &image_data;

    private:
        const char *this_path_;
        ncm_file_st parsed_file_;
        std::string meta_str_;
        rapidjson::Document meta_json_;
        cipher::abnormal_RC4 rc4_decrypter_;
        std::vector<uint8_t> image_data_;
    };

} // namespace fb2k_ncm
