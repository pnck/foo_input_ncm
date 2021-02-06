#include "stdafx.h"
#include "input_ncm.h"
#include "cipher/cipher.h"

#include "rapidjson/include/rapidjson/stringbuffer.h"
#include "rapidjson/include/rapidjson/prettywriter.h"

#include <string>
#include <algorithm>

using namespace fb2k_ncm;

inline void fb2k_ncm::input_ncm::throw_format_error() {
    throw exception_io_unsupported_format("Unsupported format or corrupted file");
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
    throw exception_io_unsupported_format("Modification on ncm file not supported.");
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
    // skip gap
    file_ptr_->source_->seek_ex(2, file::t_seek_mode::seek_from_current, p_abort);

    // extract rc4 key for audio content decoding
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
    file_ptr_->rc4_decrypter = cipher::abnormal_RC4({
            data.get() + 17,
            data.get() + parsed_file_.rc4_seed_len - cipher::guess_padding(data.get() + parsed_file_.rc4_seed_len)
        });

    // get meta info json
    file_ptr_->source_->read(&parsed_file_.meta_len, sizeof(parsed_file_.meta_len), p_abort);
    if (0 == parsed_file_.meta_len) {
        FB2K_console_formatter() << "[WARN] No meta data found in ncm file: " << file_path_;
        goto NOMETA;
    }
    [this, &p_abort] {
        auto meta_b64 = std::make_unique<char[]>(parsed_file_.meta_len + 1);
        file_ptr_->source_->read(meta_b64.get(), parsed_file_.meta_len, p_abort);
        std::for_each_n(meta_b64.get(), parsed_file_.meta_len, [](auto &b) {b ^= 0x63; });
        meta_b64[parsed_file_.meta_len] = '\0';
        if (strncmp(meta_b64.get(), "163 key(Don't modify):", 22)) {
            throw_format_error();
        }
        auto meta_decrypt_buffer_size = pfc::base64_decode_estimate(meta_b64.get() + 22);
        auto meta_raw = std::make_unique<uint8_t[]>(meta_decrypt_buffer_size);
        pfc::base64_decode(meta_b64.get() + 22, meta_raw.get());
        auto total = cipher::make_AES_context_with_key(ncm_meta_aes_key)
            .set_chain_mode(cipher::aes_chain_mode::ECB)
            .set_input(meta_raw.get(), meta_decrypt_buffer_size)
            .set_output(meta_raw.get(), meta_decrypt_buffer_size)
            .decrypt_all().outputted_len();
        total -= cipher::guess_padding(meta_raw.get() + meta_decrypt_buffer_size);
        meta_raw[total] = '\0';
        if (strncmp(reinterpret_cast<char*>(meta_raw.get()), "music:", 6)) {
            throw_format_error();
        }
        // skip heading `music:`
        meta_str_.assign(reinterpret_cast<char*>(meta_raw.get()) + 6);
        meta_json_.Parse(meta_str_.c_str());
        if (!meta_json_.IsObject()) {
            FB2K_console_formatter() << "[WARN] Failed to parse meta info of ncm file: " << file_path_;
        }
    }();
NOMETA:
    // skip gap
    file_ptr_->source_->seek_ex(sizeof(parsed_file_.unknown_data), file::t_seek_mode::seek_from_current, p_abort);
    // get album image
    file_ptr_->source_->read(&parsed_file_.album_image_size, sizeof(parsed_file_.album_image_size), p_abort);
    if (!parsed_file_.album_image_size[0]) {
        FB2K_console_formatter() << "[WARN] No album image found in ncm file: " << file_path_;
        goto NOALBUMIMG;
    }
    [this, &p_abort] {
        image_data_.resize(parsed_file_.album_image_size[0]);
        file_ptr_->source_->read(image_data_.data(), image_data_.size(), p_abort);
    }();
NOALBUMIMG:
    // remember where audio content starts
    file_ptr_->audio_content_offset_ = file_ptr_->source_->get_position(p_abort);

}

void input_ncm::decode_seek(double p_seconds, abort_callback & p_abort) {
    // seek position is masqueraded by ncm_file
    decoder_->seek(p_seconds, p_abort);
}

bool input_ncm::decode_run(audio_chunk & p_chunk, abort_callback & p_abort) {
    return decoder_->run(p_chunk, p_abort);
}

void input_ncm::decode_initialize(unsigned p_flags, abort_callback & p_abort) {
    if (!file_ptr_->audio_content_offset_) {
        // initialize should always follow open
        uBugCheck();
    }

    service_list_t<input_entry> input_services;
    do {
        // there is format hint, so we don't have to find_input twice or more
        if (meta_json_.IsObject() && meta_json_.HasMember("format")) {
            if (meta_json_["format"] == "flac") {
                input_entry::g_find_inputs_by_content_type(input_services, "audio/flac", false);
                break;
            }
            else if (meta_json_["format"] == "mp3") {
                input_entry::g_find_inputs_by_content_type(input_services, "audio/mpeg", false);
                break;
            }
        }
        // unable to determine decoder directly,  guess possible
        service_list_t<input_entry> mpeg_decoders, flac_decoders;
        input_entry::g_find_inputs_by_content_type(mpeg_decoders, "audio/mpeg", false);
        input_entry::g_find_inputs_by_content_type(flac_decoders, "audio/flac", false);
        input_services.add_items(mpeg_decoders);
        input_services.add_items(flac_decoders);
    } while (false);

    service_ptr_t<input_entry_v3> input_ptr;

    for (size_t i = 0; i < input_services.get_count(); ++i) {
        input_services[i]->cast(input_ptr);
        try {
            if (input_ptr.is_valid()) {
                input_ptr->open_for_decoding(decoder_, file_ptr_, file_path_, p_abort);
                if (decoder_.is_valid()) {
                    decoder_->initialize(0, p_flags, p_abort);
                    FB2K_console_formatter() << "Actual decoder for ncm file: " << input_ptr->get_name();
                    break;
                }
            }
        }
        catch (const pfc::exception& /*e*/) {
            // pass
        }
    }
    if (decoder_.is_empty()) {
        // final attempt: enumerate all input_entry and test if who accepting the audio content
        service_enum_t<input_entry> input_enum;
        while (input_enum.next(input_ptr)) {
            try {
                if (input_ptr.is_valid()) {
                    input_ptr->open_for_decoding(decoder_, file_ptr_, file_path_, p_abort);
                    if (decoder_.is_valid()) {
                        decoder_->initialize(0, p_flags, p_abort);
                        FB2K_console_formatter() << "Actual decoder for ncm file: " << input_ptr->get_name();
                        break;
                    }
                }
            }
            catch (const pfc::exception& /*e*/) {
                // pass
            }
        }
    }

    if (decoder_.is_empty()) {
        throw exception_service_not_found("Failed to find proper audio decoder.");
    }
    input_ptr->open_for_info_read(source_info_reader_, file_ptr_, file_path_, p_abort);
}

t_filestats input_ncm::get_file_stats(abort_callback & p_abort) {
    // return real file stats, which would be displayed on Properties/Location
    return file_ptr_->source_->get_stats(p_abort);
}

void input_ncm::get_info(file_info & p_info, abort_callback & p_abort) {
    if (source_info_reader_.is_valid()) {
        source_info_reader_->get_info(0, p_info, p_abort);
    }
    if (meta_json_.IsNull()) {
        return;
    }
    p_info.set_length(meta_json_["duration"].GetInt() / 1000.0);
    for (auto &v : meta_json_["artist"].GetArray()) {
        p_info.meta_add("Artist", v[0].GetString());
    }
    p_info.meta_set("Album", meta_json_["album"].GetString());
    p_info.meta_set("Title", meta_json_["musicName"].GetString());
    p_info.info_set_bitrate(meta_json_["bitrate"].GetUint64() / 1000);
    p_info.info_set("Music ID", PFC_string_formatter() << meta_json_["musicId"].GetUint64());
    p_info.info_set("Album ID", PFC_string_formatter() << meta_json_["albumId"].GetUint64());
    //auto album_art = album_art_data_impl::g_create(image_data_.data(), image_data_.size());
    //static_api_ptr_t<album_art_manager_v3>()->
}

bool fb2k_ncm::input_ncm::decode_get_dynamic_info(file_info & p_out, double & p_timestamp_delta)
{
    return decoder_->get_dynamic_info(p_out, p_timestamp_delta);
}


static input_singletrack_factory_t<input_ncm> g_input_ncm_factory;


#ifdef _DEBUG

class debug_initquit : public initquit {
public:
    void on_init() {
        service_enum_t<input_entry> input_enum;
        service_ptr_t<input_entry_v2> ptr;
        while (input_enum.next(ptr)) {
            FB2K_console_formatter() << ptr->get_name() << " " << pfc::print_guid(ptr->class_guid);
        }

        rapidjson::Document d;
        d.Parse(
            u8R"__(

{
"array":[1,2,3],
"object":{"ok":true}
}

)__"
);
        rapidjson::StringBuffer sb;
        rapidjson::PrettyWriter writer(sb);
        d.Accept(writer);
        FB2K_console_formatter() << sb.GetString();
    }
    void on_quit() {

    }
};

static initquit_factory_t<debug_initquit> g_debug_initquit_factory;

#endif // _DEBUG