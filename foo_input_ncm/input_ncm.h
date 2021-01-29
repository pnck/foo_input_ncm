#pragma once
#include "stdafx.h"
#include "cipher/cipher.h"
#include "common/defines.h"

class input_ncm : public input_stubs {
public:
    void open(service_ptr_t<file> p_filehint, const char * p_path, t_input_open_reason p_reason, abort_callback & p_abort);
    static const char * g_get_name() { return "ncm input"; }
    static bool g_is_our_path(const char * p_path, const char * p_extension) { return stricmp_utf8(p_extension, "ncm") == 0; }
    static const GUID g_get_guid() {
        return GUID{ 0xef1cb99 ,0x91ad ,0x486c ,{ 0x82,0x82 ,0xda,0x70,0x98,0x4e,0xa8,0x51} };
    };
    static bool g_is_our_content_type(const char * p_content_type) { return false; }
    void retag(const file_info & p_info, abort_callback & p_abort) { throw exception_io_unsupported_format(); }
    bool decode_can_seek() { return false; }
    void decode_seek(double p_seconds, abort_callback & p_abort);
    bool decode_run(audio_chunk & p_chunk, abort_callback & p_abort);
    void decode_initialize(unsigned p_flags, abort_callback & p_abort);
    t_filestats get_file_stats(abort_callback & p_abort);
    void get_info(file_info & p_info, abort_callback & p_abort);
};