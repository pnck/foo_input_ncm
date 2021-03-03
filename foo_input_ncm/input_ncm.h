#pragma once
#include "stdafx.h"
#include "common/define.h"
#include "ncm_file.h"

#include <vector>

namespace fb2k_ncm
{

    class input_ncm : public input_stubs {

    public:
        input_ncm() = default;
        virtual ~input_ncm(){};

    public:
        void open(service_ptr_t<file> p_filehint, const char *p_path, t_input_open_reason p_reason,
                  abort_callback &p_abort);
        static const char *g_get_name();
        static bool g_is_our_path(const char *p_path, const char *p_extension);
        static const GUID g_get_guid();
        static bool g_is_our_content_type(const char *p_content_type);
        void retag(const file_info &p_info, abort_callback &p_abort);
        bool decode_can_seek();
        void decode_seek(double p_seconds, abort_callback &p_abort);
        bool decode_run(audio_chunk &p_chunk, abort_callback &p_abort);
        void decode_initialize(unsigned p_flags, abort_callback &p_abort);
        t_filestats get_file_stats(abort_callback &p_abort);
        void get_info(file_info &p_info, abort_callback &p_abort);

    private:
        service_ptr_t<ncm_file> ncm_file_;
        service_ptr_t<input_decoder> decoder_;
        service_ptr_t<input_info_reader> source_info_reader_;
    };
} // namespace fb2k_ncm