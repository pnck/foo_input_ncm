#pragma once
#include "sdk/foobar2000/SDK/foobar2000.h"
#include "cipher/abnormal_RC4.h"
#include "common/define.h"

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
        explicit ncm_file(file_ptr &source) : source_(source), audio_content_offset_(0) {}
        FB2K_MAKE_SERVICE_INTERFACE(ncm_file, file);

    public:
        file_ptr source_;
        cipher::abnormal_RC4 rc4_decrypter;
        t_filesize audio_content_offset_;
    };

    FOOGUIDDECL const GUID ncm_file::class_guid = guid_candidates[1];

} // namespace fb2k_ncm
