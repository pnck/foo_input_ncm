#pragma once
#include "sdk/foobar2000/SDK/foobar2000.h"
#include "cipher/abnormal_RC4.h"
#include "common/define.h"
#include "rapidjson/include/rapidjson/document.h"

namespace fb2k_ncm
{

    class ncm_file : public file {
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
            : source_(source), this_path_(path), audio_content_offset_(0) {}
        const char *path() const { return this_path_; }
        PFC_NORETURN void ensure_audio_offset();
        void parse();
        inline const rapidjson::Document &meta() const { return meta_json_; }

    private:
        inline void throw_format_error();

    public:
        file_ptr source_;

    private:
        const char *this_path_;
        ncm_file_st parsed_file_;
        t_filesize audio_content_offset_;
        std::string meta_str_;
        rapidjson::Document meta_json_;
        cipher::abnormal_RC4 rc4_decrypter_;
        std::vector<uint8_t> image_data_;
    };

} // namespace fb2k_ncm
