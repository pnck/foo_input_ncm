#include "stdafx.h"
#include "input_ncm.hpp"

#include <string>
#include <sstream>
#include <algorithm>
#include <unordered_set>

using namespace fb2k_ncm;

inline const char *fb2k_ncm::input_ncm::g_get_name() {
    return "Netease Music Specific Format (*.ncm) Decoder";
}

inline bool fb2k_ncm::input_ncm::g_is_our_path(const char *p_path, const char *p_extension) {
    return stricmp_utf8(p_extension, "ncm") == 0;
}

inline const GUID fb2k_ncm::input_ncm::g_get_guid() {
    return input_ncm::class_guid;
}

inline bool fb2k_ncm::input_ncm::g_is_our_content_type(const char *p_content_type) {
    constexpr std::string_view mime = "audio/ncm";
    return stricmp_utf8_max(p_content_type, mime.data(), mime.size()) == 0;
}

inline void fb2k_ncm::input_ncm::retag(const file_info &p_info, abort_callback &p_abort) {
    // @todo: support of retagging on audio content (with ncm wrapper and meta info not touched)
    throw exception_tagging_unsupported();
}

inline void fb2k_ncm::input_ncm::remove_tags(abort_callback &p_abort) {}

inline bool fb2k_ncm::input_ncm::decode_can_seek() {
    return decoder_->can_seek();
}

void input_ncm::open(foobar2000_io::file::ptr p_filehint, const char *p_path, t_input_open_reason p_reason, abort_callback &p_abort) {
    if (p_reason == t_input_open_reason::input_open_info_write) {
        FB2K_console_print("[ERR] Modification on a ncm file is not supported.");
        throw exception_io_unsupported_format();
    }

    if (!ncm_file_.is_valid() && p_filehint.is_empty()) { // open from filesystem
        DEBUG_LOG("[DEBUG] input_ncm::open (", p_path, ") for reason ", p_reason);
        ncm_file_ = fb2k::service_new<ncm_file>(p_path);
        if (!ncm_file_.is_valid()) {
            FB2K_console_print("[ERR] Failed to open file: ", p_path);
            throw exception_io_not_found();
        }
    } else if (p_filehint.is_valid()) { // attach an opened ncm_file
        if (p_filehint->cast(ncm_file_)) {
            DEBUG_LOG("[DEBUG] input_ncm::open (", ncm_file_->path(), ") for reason ", p_reason, " (from file hint)");
        } else {
            DEBUG_LOG("[DEBUG] input_ncm::open cast froom file_ptr failed");
            throw exception_io_denied();
        }
    } else {
        FB2K_console_print("[ERR] Open ncm file failed (invalid file hint).");
        throw exception_io_unsupported_format();
    }

    // walk through the file structure
    // parse() won't decrypt the whole file, so we can always parse metadata and audio info at the same time.
    if (!(ncm_file_->meta_parsed() && ncm_file_->audio_parsed())) {
        ncm_file_->parse(ncm_file::parse_contents::NCM_PARSE_META | ncm_file::parse_contents::NCM_PARSE_AUDIO);
    }

    // find any available decoders by followint steps:
    // 1. check if there is format hint in meta_info
    // 2. select the 2 possible decoder (mp3,flac) and try if they work
    // 3. enumerate all input_entry and test if who accepts the audio content
    service_list_t<input_entry> input_services;
    do {
        // there is format hint, so we don't have to find_input twice or more
        if (ncm_file_->meta_info().IsObject() && ncm_file_->meta_info().HasMember("format")) {
            if (ncm_file_->meta_info()["format"] == "flac") {
                input_entry::g_find_inputs_by_content_type(input_services, "audio/flac", true);
                break;
            } else if (ncm_file_->meta_info()["format"] == "mp3") {
                input_entry::g_find_inputs_by_content_type(input_services, "audio/mpeg", true);
                break;
            } else {
                FB2K_console_print("[ERR] Unknown ncm format hint: ", ncm_file_->meta_info()["format"].GetString());
                throw exception_io_unsupported_format();
            }
        }
        // unable to determine decoder directly,  guess possible
        service_list_t<input_entry> mpeg_decoders, flac_decoders;
        input_entry::g_find_inputs_by_content_type(mpeg_decoders, "audio/mpeg", true);
        input_entry::g_find_inputs_by_content_type(flac_decoders, "audio/flac", true);
        input_services.add_items(mpeg_decoders);
        input_services.add_items(flac_decoders);
    } while (false);

    service_ptr_t<input_entry_v3> input_ptr;
    // see if found input matches the codec
    for (size_t i = 0; i < input_services.get_count(); ++i) {
        input_services[i]->cast(input_ptr);
        try {
            if (input_ptr.is_valid()) {
                input_ptr->open_for_decoding(decoder_, ncm_file_, /*file_path_*/ "", p_abort);
                if (decoder_.is_valid()) {
                    // decoder_->initialize(0, p_flags, p_abort);
                    DEBUG_LOG("[DEBUG] Found decoder [", input_ptr->get_name(), "] for ", ncm_file_->path());
                    break;
                }
            }
        } catch (const pfc::exception &e) {
            DEBUG_LOG("[DEBUG] (by MIME) Give up ", input_ptr->get_name_(), "; reason=", e.what());
        }
    }
    if (decoder_.is_empty()) {
        // final attempt: enumerate all input_entry and test if who accepts the audio content
        service_enum_t<input_entry> input_enum;
        while (input_enum.next(input_ptr)) {
            try {
                if (input_ptr.is_valid()) {
                    input_ptr->open_for_decoding(decoder_, ncm_file_, /*file_path_*/ "", p_abort);
                    if (decoder_.is_valid()) {
                        // decoder_->initialize(0, p_flags, p_abort);
                        DEBUG_LOG("[DEBUG] Found decoder [", input_ptr->get_name(), "] for ", ncm_file_->path());
                        break;
                    }
                }
            } catch (const pfc::exception &e) {
                DEBUG_LOG("[DEBUG] (by enumerate) Give up ", input_ptr->get_name_(), "; reason=", e.what());
            }
        }
    }

    if (decoder_.is_empty()) {
        FB2K_console_print("[ERR] Failed to find proper audio decoder.");
        throw exception_service_not_found();
    }

    if (p_reason == t_input_open_reason::input_open_info_read) {
        input_ptr->open_for_info_read(source_info_reader_, ncm_file_, /*p_path*/ "", p_abort);
    }
}

void input_ncm::decode_seek(double p_seconds, abort_callback &p_abort) {
    // seek position is masqueraded by ncm_file
    decoder_->seek(p_seconds, p_abort);
    DEBUG_LOG("[DEBUG] decode_seek() : ", p_seconds, " sec");
}

bool input_ncm::decode_run(audio_chunk &p_chunk, abort_callback &p_abort) {
    return decoder_->run(p_chunk, p_abort);
}

void input_ncm::decode_initialize(unsigned p_flags, abort_callback &p_abort) {
    // initialize should always follow open
    ncm_file_->ensure_audio_offset();
    decoder_->initialize(/*no subsong*/ 0, p_flags, p_abort);
    DEBUG_LOG("[DEBUG] decode_initialize() called");
}

t_filestats input_ncm::get_file_stats(abort_callback &p_abort) {
    // return real file stats, which would be displayed on Properties/Location
    return ncm_file_->get_stats(p_abort);
}

t_filestats2 fb2k_ncm::input_ncm::get_stats2(uint32_t f, abort_callback &a) {
    return ncm_file_->get_stats2_(f, a);
}

void input_ncm::get_info(file_info &p_info, abort_callback &p_abort) {
    if (source_info_reader_.is_valid()) {
        source_info_reader_->get_info(/*sub song*/ 0, p_info, p_abort);
    }
    if (ncm_file_->meta_info().IsNull()) {
        return;
    }

    // p_info.set_length(meta()["duration"].GetInt() / 1000.0);
    // p_info.info_set_bitrate(meta()["bitrate"].GetUint64() / 1000);
    if (ncm_file_->meta_info().HasMember("artist")) {
        std::unordered_set<std::string> artists; // remove redundant (from tags within audio content)
        if (auto count = p_info.meta_get_count_by_name("Artist"); count > 0) {
            auto index = p_info.meta_find("Artist");
            for (size_t i = 0; i < count; ++i) {
                artists.insert(p_info.meta_enum_value(index, i));
            }
        }
        for (auto &v : ncm_file_->meta_info()["artist"].GetArray()) {
            artists.insert(v[0].GetString());
        }
        p_info.meta_remove_field("Artist");
        for (auto &v : artists) {
            p_info.meta_add("Artist", v.c_str());
        }
    }
    if (ncm_file_->meta_info().HasMember("date")) {
        p_info.meta_set("Date", ncm_file_->meta_info()["date"].GetString());
    }
    if (ncm_file_->meta_info().HasMember("album")) {
        p_info.meta_set("Album", ncm_file_->meta_info()["album"].GetString());
    }
    if (ncm_file_->meta_info().HasMember("musicName")) {
        p_info.meta_set("Title", ncm_file_->meta_info()["musicName"].GetString());
    }
    if (ncm_file_->meta_info().HasMember("musicId")) {
        p_info.info_set("Music ID", PFC_string_formatter() << ncm_file_->meta_info()["musicId"].GetUint64());
    }
    if (ncm_file_->meta_info().HasMember("albumId")) {
        p_info.info_set("Album ID", PFC_string_formatter() << ncm_file_->meta_info()["albumId"].GetUint64());
    }
    if (ncm_file_->meta_info().HasMember("albumPic")) {
        p_info.info_set("Album Artwork", ncm_file_->meta_info()["albumPic"].GetString());
    }
    if (ncm_file_->meta_info().HasMember("alias")) {
        std::stringstream comment;
        bool first = true;
        for (auto &v : ncm_file_->meta_info()["alias"].GetArray()) {
            // p_info.meta_add("Alias", v.GetString());
            if (!first) {
                comment << " | ";
            }
            comment << v.GetString();
            first = false;
        }
        p_info.meta_remove_field("Comment");
        p_info.meta_set("Comment", comment.str().c_str());
    }
    if (ncm_file_->meta_info().HasMember("transNames")) {
        for (auto &v : ncm_file_->meta_info()["transNames"].GetArray()) {
            p_info.meta_add("Translated Title", v.GetString());
        }
    }
}

static input_singletrack_factory_t<input_ncm> g_input_ncm_factory;
