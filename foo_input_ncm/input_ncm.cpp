#include "stdafx.h"
#include "input_ncm.h"
#include "cipher/cipher.h"


#include <string>
#include <algorithm>

using namespace fb2k_ncm;

inline void fb2k_ncm::input_ncm::throw_format_error() {
    throw exception_io_data("Unsupported format or corrupted file");
}

inline const char * fb2k_ncm::input_ncm::g_get_name() {
    return "Netease Music Specific Format (*.ncm) Decoder";
}

inline bool fb2k_ncm::input_ncm::g_is_our_path(const char * p_path, const char * p_extension) {
    return stricmp_utf8(p_extension, "ncm") == 0;
}

inline const GUID fb2k_ncm::input_ncm::g_get_guid() {
    return guid_candidates[0];
}

inline bool fb2k_ncm::input_ncm::g_is_our_content_type(const char * p_content_type) {
    return strncmp(p_content_type, "audio/ncm", 10) == 0;
}

inline void fb2k_ncm::input_ncm::retag(const file_info & p_info, abort_callback & p_abort) {
    throw exception_io_unsupported_format();
}

inline bool fb2k_ncm::input_ncm::decode_can_seek() {
    return decoder_->can_seek();
}


void input_ncm::open(service_ptr_t<file> p_filehint, const char * p_path, t_input_open_reason p_reason, abort_callback & p_abort) {
    if (p_reason == t_input_open_reason::input_open_info_write) {
        throw exception_io_unsupported_format("Modification on ncm file not supported.");
    }
    if (!file_ptr_.is_valid()) {
        input_open_file_helper(p_filehint, p_path, p_reason, p_abort);
        file_ptr_ = fb2k::service_new<ncm_file>(p_filehint);
        file_path_ = p_path;
    }
    file_ptr_->source_->reopen(p_abort);
    uint64_t magic = 0;
    file_ptr_->source_->read(&magic, sizeof(uint64_t), p_abort);
    if (magic != parsed_file_.magic) {
        throw_format_error();
    }
    file_ptr_->source_->seek_ex(2, file::t_seek_mode::seek_from_current, p_abort);
    switch (p_reason) {
    case t_input_open_reason::input_open_info_read: {
        file_ptr_->source_->read(&parsed_file_.rc4_seed_len, sizeof(parsed_file_.rc4_seed_len), p_abort);
        if (0 == parsed_file_.rc4_seed_len || parsed_file_.rc4_seed_len > 256) {
            throw_format_error();
        }
        file_ptr_->source_->seek_ex(parsed_file_.rc4_seed_len, file::t_seek_mode::seek_from_current, p_abort);
        file_ptr_->source_->read(&parsed_file_.meta_len, sizeof(parsed_file_.meta_len), p_abort);
    } break;
    case t_input_open_reason::input_open_decode: {
        file_ptr_->source_->read(&parsed_file_.rc4_seed_len, sizeof(parsed_file_.rc4_seed_len), p_abort);
        if (0 == parsed_file_.rc4_seed_len || parsed_file_.rc4_seed_len > 256) {
            throw_format_error();
        }
        auto data_size = cipher::aligned(parsed_file_.rc4_seed_len);
        auto data = std::make_unique<uint8_t[]>(data_size);
        file_ptr_->source_->read(data.get(), parsed_file_.rc4_seed_len, p_abort);
        std::for_each_n(data.get(), parsed_file_.rc4_seed_len, [](uint8_t& _b) {_b ^= 0x64; });
        cipher::make_AES_context_with_key(ncm_rc4_seed_aes_key)
            .set_chain_mode(cipher::aes_chain_mode::ECB)
            .set_input(data.get(), parsed_file_.rc4_seed_len)
            .set_output(data.get(), data_size)
            .decrypt_all();
        if (strncmp(reinterpret_cast<char*>(data.get()), "neteasecloudmusic", 17)) {
            throw_format_error();
        }
        rc4_seed_.assign(data.get() + 17, data.get() + parsed_file_.rc4_seed_len - cipher::guess_padding(data.get() + parsed_file_.rc4_seed_len));
        file_ptr_->rc4_decrypter = cipher::abnormal_RC4(rc4_seed_);
        file_ptr_->source_->read(&parsed_file_.meta_len, sizeof(parsed_file_.meta_len), p_abort);
        if (0 == parsed_file_.meta_len) {
            FB2K_console_formatter() << "[WARN] No meta data found in ncm file: " << file_path_;
        }
        file_ptr_->source_->seek_ex(parsed_file_.meta_len, file::t_seek_mode::seek_from_current, p_abort);
        file_ptr_->source_->seek_ex(sizeof(parsed_file_.unknown_data), file::t_seek_mode::seek_from_current, p_abort);
        file_ptr_->source_->read(&parsed_file_.album_image_size, sizeof(parsed_file_.album_image_size), p_abort);
        file_ptr_->source_->seek_ex(parsed_file_.album_image_size[0], file::t_seek_mode::seek_from_current, p_abort);
        // remember where audio content starts
        file_ptr_->audio_content_off_ = file_ptr_->source_->get_position(p_abort);
    }break;
    default:
        uBugCheck();
    }
}

void input_ncm::decode_seek(double p_seconds, abort_callback & p_abort) {
    decoder_->seek(p_seconds, p_abort);
}

bool input_ncm::decode_run(audio_chunk & p_chunk, abort_callback & p_abort) {
    return decoder_->run(p_chunk, p_abort);
}

void input_ncm::decode_initialize(unsigned p_flags, abort_callback & p_abort) {
    if (!file_ptr_->audio_content_off_) {
        uBugCheck();
    }

    //file_ptr_->source_->seek(file_ptr_->audio_content_off_, p_abort);
    service_list_t<input_entry> result;
    input_entry::g_find_inputs_by_content_type(result, "audio/flac", false);
    service_ptr_t<input_entry_v2> ptr;
    result[0]->cast(ptr);

    FB2K_console_formatter() << FB2K_DebugLog() << "using decoder for ncm file: " << ptr->get_name();
    ptr->open_for_decoding(decoder_, file_ptr_, file_path_, p_abort);
    decoder_->initialize(0, p_flags, p_abort);
}

t_filestats input_ncm::get_file_stats(abort_callback & p_abort) {
    return file_ptr_->source_->get_stats(p_abort);
}

void input_ncm::get_info(file_info & p_info, abort_callback & p_abort) {
    decoder_->get_info(0, p_info, p_abort);
}


static input_singletrack_factory_t<input_ncm> g_input_ncm_factory;


#ifdef _DEBUG

class debug_initquit : public initquit {
public:
    void on_init() {
        service_enum_t<input_entry> input_services;
        service_ptr_t<input_entry_v3> ptr;
        while (input_services.next(ptr)) {
            FB2K_console_formatter() << ptr->get_name() << " " << pfc::print_guid(ptr->class_guid);
        }

    }
    void on_quit() {

    }
};

static initquit_factory_t<debug_initquit> g_debug_initquit_factory;

#endif // _DEBUG