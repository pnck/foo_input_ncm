#include "stdafx.h"
#include "input_ncm.h"


#include <string>

using namespace fb2k_ncm;

void input_ncm::open(service_ptr_t<file> p_filehint, const char * p_path, t_input_open_reason p_reason, abort_callback & p_abort) {
    throw exception_io_unsupported_format("not implemented");
    //input_entry::g_find_inputs_by_content_type
}

void input_ncm::decode_seek(double p_seconds, abort_callback & p_abort)
{

}

bool input_ncm::decode_run(audio_chunk & p_chunk, abort_callback & p_abort)
{
    auto a = cipher::AES(ncm_rc4_seed_aes_key);
    auto b = cipher::AES<32>();
    return false;
}

void input_ncm::decode_initialize(unsigned p_flags, abort_callback & p_abort)
{
}

t_filestats input_ncm::get_file_stats(abort_callback & p_abort)
{
    return t_filestats();
}

void input_ncm::get_info(file_info & p_info, abort_callback & p_abort)
{
}

static input_singletrack_factory_t<input_ncm> g_input_ncm_factory;