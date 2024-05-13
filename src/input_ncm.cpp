#include "stdafx.h"
#include "input_ncm.hpp"
#include "common/log.hpp"
#include "meta_process.hpp"

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

/// @note
/// - ReplayGain info and meta tags are stored in different places.
/// The former is stored in the audio content, while the latter is stored in the ncm header.
/// @note
/// - See also: `ncm_file::overwrite_meta()`
void fb2k_ncm::input_ncm::retag(const file_info &p_info, abort_callback &p_abort) {
    DEBUG_LOG("input_ncm::retag()");
    // Replay Gain
    file_info_impl cur_file_info;
    source_info_writer_->get_info(0, cur_file_info, p_abort);
    cur_file_info.set_replaygain(p_info.get_replaygain());
    source_info_writer_->set_info(0, cur_file_info, p_abort);
    source_info_writer_->commit(p_abort);

    // Meta tags
    // NOTE: Always do differential update, to maximally avoid appending "overwrite" key,
    // which will change the "163 key xxxx" content.
    auto target_json = meta_processor(p_info).dump();
    if (target_json.empty()) { // fallback case: clear all fields
        return this->remove_tags(p_abort);
    }
    this->get_info(cur_file_info, p_abort);
    auto current_json = meta_processor(cur_file_info).dump();

    auto overwrite = json_t::object();

    if (ncm_file_->meta_info().contains(overwrite_key)) {
        // overwrite is guaranteed to be suitable with current_meta
        // because this->get_info() will call parse() and reset meta_info()
        overwrite = ncm_file_->meta_info()[overwrite_key];
    }

    auto diff = json_t::diff(current_json, target_json);
    auto to_merge = json_t::object();
    for (const auto &op : diff) {
        json_t::json_pointer pt(op["path"]);
        for (; !pt.parent_pointer().empty(); pt = pt.parent_pointer()) {
            continue;
        }
        if (op["op"] == "remove") {
            if (!overwrite.contains(pt.back())) {
                overwrite.emplace(pt.back(), nullptr);
            } else {
                overwrite[pt.back()] = nullptr;
            }
        } else {
            if (overwrite.contains(pt.back())) {
                overwrite.erase(pt.back());
            }
            overwrite.emplace(pt.back(), target_json.at(pt)); // copy the top level
        }
    }

    overwrite.merge_patch(to_merge);

    ncm_file_->overwrite_meta(overwrite, p_abort);
}

/// @brief "Clear tags" => restore the meta field (163 key) to its original state
/// @attention ReplayGain and embedded tags in the audio content are left untouched.
/// @note ReplayGain can be wiped by retagging, since the meta diff would be empty then.
void fb2k_ncm::input_ncm::remove_tags(abort_callback &p_abort) {
    DEBUG_LOG("input_ncm::remove_tags()");
    // source_info_writer_->remove_tags_fallback(p_abort);
    ncm_file_->overwrite_meta(nlohmann::json(), p_abort);
}

inline bool fb2k_ncm::input_ncm::decode_can_seek() {
    return decoder_->can_seek();
}

void input_ncm::open(foobar2000_io::file::ptr p_filehint, const char *p_path, t_input_open_reason p_reason, abort_callback &p_abort) {

    if (std::string checked_path(p_path); checked_path.length()) { // available from filesystem
        DEBUG_LOG_F("input_ncm::open() for {} => {}", p_reason, p_path);
        switch (p_reason) {
        case t_input_open_reason::input_open_decode:
            [[fallthrough]];
        case t_input_open_reason::input_open_info_read:
            ncm_file_ = fb2k::service_new<ncm_file>(p_path);
            break;
        case t_input_open_reason::input_open_info_write:
            ncm_file_ = fb2k::service_new<ncm_file>(p_path, filesystem::open_mode_write_existing);
            break;
        default:
            throw exception_io_unsupported_feature();
        }
        if (ncm_file_.is_empty()) [[unlikely]] {
            uBugCheck();
        }
    } else if (p_filehint.is_valid()) { // attach an opened ncm_file
        if (p_filehint->cast(ncm_file_)) {
            DEBUG_LOG("input_ncm::open (", ncm_file_->path(), ") for reason ", p_reason, " (from file hint)");
        } else {
            DEBUG_LOG("input_ncm::open cast from file_ptr failed, reason=", p_reason);
            throw exception_io_unsupported_feature();
        }
    } else {
        ERROR_LOG("Open ncm file failed (invalid file hint).");
        throw exception_io_no_handler_for_path();
    }

    // walk through the file structure
    // parse() won't decrypt the whole file, so we can always parse metadata and audio info at the same time.
    if (!(ncm_file_->meta_parsed() && ncm_file_->audio_key_parsed())) {
        ncm_file_->parse(ncm_file::parse_targets::NCM_PARSE_META | ncm_file::parse_targets::NCM_PARSE_AUDIO);
    }

    // find any available decoders by the following steps:
    // 1. check if there is a format hint in meta_info
    // 2. select the 2 possible decoder (mp3,flac) and try if any of them works
    // 3. enumerate all input_entry and test if one accepts the audio content
    service_list_t<input_entry> input_services;
    do {
        // there is format hint, so we don't have to find_input twice or more
        if (ncm_file_->meta_info().is_object() && ncm_file_->meta_info().contains("format")) {
            if (ncm_file_->meta_info()["format"] == "flac") {
                input_entry::g_find_inputs_by_content_type(input_services, "audio/flac", true);
                break;
            } else if (ncm_file_->meta_info()["format"] == "mp3") {
                input_entry::g_find_inputs_by_content_type(input_services, "audio/mpeg", true);
                break;
            } else {
                ERROR_LOG("Unknown ncm format hint: ", ncm_file_->meta_info()["format"].get_ref<const nlohmann::json::string_t &>());
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

    service_ptr_t<input_entry_v2> input_ptr;
    // see if found input matches the codec
    for (size_t i = 0; i < input_services.get_count(); ++i) {
        input_services[i]->cast(input_ptr);
        try {
            if (input_ptr.is_valid()) {
                input_ptr->open_for_decoding(decoder_, ncm_file_, /*file_path_*/ "", p_abort);
                if (decoder_.is_valid()) {
                    // decoder_->initialize(0, p_flags, p_abort);
                    DEBUG_LOG("Found decoder [", input_ptr->get_name(), "] for ", ncm_file_->path());
                    input_ = input_ptr;
                    break;
                }
            }
        } catch (const pfc::exception &e) {
            DEBUG_LOG("(by MIME) Give up ", input_ptr->get_name_(), "; reason=", e.what());
        }
    }
    if (decoder_.is_empty()) {
        // final attempt: enumerate all input_entry and test if who accepts the audio content
        service_enum_t<input_entry> input_enum;
        while (input_enum.next(input_ptr)) {
            try {
                if (input_ptr.is_empty()) {
                    continue;
                }
                if (input_ptr->get_guid_() == class_guid) { // self
                    continue;
                }
                input_ptr->open_for_decoding(decoder_, ncm_file_, /*file_path_*/ "", p_abort);
                if (decoder_.is_valid()) {
                    // decoder_->initialize(0, p_flags, p_abort);
                    DEBUG_LOG("Found decoder [", input_ptr->get_name(), "] for ", ncm_file_->path());
                    input_ = input_ptr;
                    break;
                }

            } catch (const pfc::exception &e) {
                DEBUG_LOG("(by enumerate) Give up ", input_ptr->get_name_(), "; reason=", e.what());
            }
        }
    }

    if (decoder_.is_empty()) {
        ERROR_LOG("Failed to find proper audio decoder.");
        throw exception_service_not_found();
    }

#if 0 // maybe misused
    if (p_reason == t_input_open_reason::input_open_info_read || p_reason == t_input_open_reason::input_open_info_write) {
        input_ptr->open_for_info_read(source_info_reader_, ncm_file_, /*p_path*/ "", p_abort);
    }
#endif
    if (p_reason == t_input_open_reason::input_open_info_write) {
        DEBUG_LOG("Attempt to write to ncm file.");
        input_->open_for_info_write(source_info_writer_, ncm_file_, /*p_path*/ "", p_abort);
    }
}

void input_ncm::decode_seek(double p_seconds, abort_callback &p_abort) {
    // seek position is masqueraded by ncm_file
    decoder_->seek(p_seconds, p_abort);
    DEBUG_LOG("decode_seek() : ", p_seconds, " sec");
}

bool input_ncm::decode_run(audio_chunk &p_chunk, abort_callback &p_abort) {
    return decoder_->run(p_chunk, p_abort);
}

void input_ncm::decode_initialize(unsigned p_flags, abort_callback &p_abort) {
    // initialize should always follow open
    decoder_->initialize(/*no subsong*/ 0, p_flags, p_abort);
    DEBUG_LOG("decode_initialize() called");
}

t_filestats input_ncm::get_file_stats(abort_callback &p_abort) {
    return ncm_file_->get_stats(p_abort);
}

t_filestats2 fb2k_ncm::input_ncm::get_stats2(uint32_t f, abort_callback &a) {
    return ncm_file_->get_stats2_(f, a);
}

void input_ncm::get_info(file_info &p_info, abort_callback &p_abort) {
    DEBUG_LOG("input_ncm::get_info()");
    if (ncm_file_.is_empty()) {
        return;
    }

    // refresh meta info after retagging
    ncm_file_->parse(ncm_file::parse_targets::NCM_PARSE_META);

    input_info_reader::ptr reader;
    if (source_info_writer_.is_valid()) { // if just retagged, use the recent file_info
        reader = source_info_writer_;
    } else if (decoder_.is_valid()) {
        reader = decoder_;
    }

    reader->get_info(/*sub song*/ 0, p_info, p_abort);

    auto mp = meta_processor(p_info);
    p_info.meta_remove_all();
    mp.update(ncm_file_->meta_info());
    mp.apply(p_info);
}

static input_singletrack_factory_t<input_ncm> g_input_ncm_factory;
