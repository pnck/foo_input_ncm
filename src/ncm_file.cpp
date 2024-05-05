#include "stdafx.h"
#include "ncm_file.hpp"

#include "common/platform.hpp"
#include "meta_process.hpp"
#include "common/log.hpp"

#include <algorithm>
#include <span>
#include <ranges>
#include <unordered_map>

using namespace std::string_view_literals;
using namespace fb2k_ncm;

inline void ncm_file::ensure_audio_offset() {
    if (!parsed_file_.audio_content_offset) [[unlikely]] {
        uBugCheck();
    }
}

inline void ncm_file::ensure_decryptor() {
    if (!rc4_decryptor_.is_valid()) [[unlikely]] {
        throw exception_io();
    }
}
// helper macro: current function being logged will be at the right place
#define ENSURE_DECRYPTOR()                 \
    do {                                   \
        try {                              \
            ncm_file::ensure_decryptor();  \
        } catch (...) {                    \
            ERROR_LOG("Decryptor error."); \
            throw;                         \
        }                                  \
    } while (0)

inline void ncm_file::throw_format_error(const char *extra) {
    ERROR_LOG("Unsupported format or corrupted file (", extra, ").");
    throw exception_io_unsupported_format();
}

inline void ncm_file::throw_format_error(const std::string &extra) {
    throw_format_error(extra.c_str());
}

t_size fb2k_ncm::ncm_file::read(void *p_buffer, t_size p_bytes, abort_callback &p_abort) {
    ensure_audio_offset();
    ENSURE_DECRYPTOR();
    auto source_pos = source_->get_position(p_abort);
    auto read_offset = source_pos - parsed_file_.audio_content_offset;
    std::vector<uint8_t> enc(p_bytes);

    t_size total = 0;
    total = source_->read(enc.data(), p_bytes, p_abort);
    if (!total) [[unlikely]] {
        return 0;
    }

    auto dec = rc4_decryptor_.reset_counter(read_offset).transform(enc);
    std::span<uint8_t> output(static_cast<uint8_t *>(p_buffer), p_bytes);
    std::copy(std::begin(dec), std::end(dec), std::begin(output));
    // DEBUG_LOG_F("Read at {} ({}): req={}, real={}", read_offset, source_pos, p_bytes, total);
    return total;
}

/// @attention This function is not implemented / not in use
void fb2k_ncm::ncm_file::write(const void *p_buffer, t_size p_bytes, abort_callback &p_abort) {
    ensure_audio_offset();
    auto source_pos = source_->get_position(p_abort);
    if (source_pos < parsed_file_.audio_content_offset) {
        ERROR_LOG("Modification (metadata) on a ncm file is not supported.");
        throw exception_io_denied_readonly();
    }
    auto write_offset = source_pos - parsed_file_.audio_content_offset;
    std::span<const uint8_t> input(static_cast<const uint8_t *>(p_buffer), p_bytes);
    auto enc = rc4_decryptor_.reset_counter(write_offset).transform(input);
    std::vector<uint8_t> buf(enc.begin(), enc.end());

    source_->write(buf.data(), buf.size(), p_abort);
    // DEBUG_LOG_F("Write to {} ({}): {} bytes", write_offset, source_pos, p_bytes);
}

t_filesize fb2k_ncm::ncm_file::get_size(abort_callback &p_abort) {
    ensure_audio_offset();
    return source_->get_size(p_abort) - parsed_file_.audio_content_offset;
}

t_filesize fb2k_ncm::ncm_file::get_position(abort_callback &p_abort) {
    ensure_audio_offset();
    return source_->get_position(p_abort) - parsed_file_.audio_content_offset;
}

void fb2k_ncm::ncm_file::resize(t_filesize p_size, abort_callback &p_abort) {
    DEBUG_LOG_F("RESIZE ncm_file::resize({})", p_size);
    ensure_audio_offset();
    return source_->resize(parsed_file_.audio_content_offset + p_size, p_abort);
}

void fb2k_ncm::ncm_file::seek(t_filesize p_position, abort_callback &p_abort) {
    ensure_audio_offset();
    return source_->seek(parsed_file_.audio_content_offset + p_position, p_abort);
}

bool fb2k_ncm::ncm_file::can_seek() {
    return source_->can_seek();
}

bool fb2k_ncm::ncm_file::get_content_type(pfc::string_base &p_out) {
    p_out = "audio/ncm";
    return true;
}

void fb2k_ncm::ncm_file::reopen(abort_callback &p_abort) {
    source_->reopen(p_abort);
    parse(parse_targets::NCM_PARSE_META | parse_targets::NCM_PARSE_ALBUM | parse_targets::NCM_PARSE_AUDIO);
    ensure_audio_offset();
    source_->seek(parsed_file_.audio_content_offset, p_abort);
}

bool fb2k_ncm::ncm_file::is_remote() {
    return false;
}

t_filetimestamp fb2k_ncm::ncm_file::get_timestamp(abort_callback &p_abort) {
    return source_->get_timestamp(p_abort);
}

auto ncm_file::make_seek_guard(abort_callback &p_abort) {
    return std::unique_ptr<void, std::function<void(void *)>>(
        nullptr,
        [this, &p_abort, p = source_->get_position(p_abort), off = parsed_file_.audio_content_offset, &parsed = parsed_file_](void *) {
            // RAII guard, will seek back when function returns
            if (p > off) { // lands in audio
                source_->seek(parsed.audio_content_offset + (p - off), p_abort);
            } else {
                source_->seek(p, p_abort);
            }
        });
}

void ncm_file::parse(uint16_t to_parse /* = 0xff*/) {

    // NOTE:
    // This is basically a state-driven function.
    // But we don't need an actual state table, because the parsing process is linear.
    // We just do `goto`s to jump to the next state.

    DEBUG_LOG("Parse (C=", to_parse, ") ", this_path_);

    auto &p_abort = fb2k::noAbort;
    auto _seek_guard_ = make_seek_guard();
    source_->seek_ex(0, file::t_seek_mode::seek_from_beginning, p_abort);

    uint64_t magic = 0;
    source_->read(&magic, sizeof(uint64_t), p_abort);
    if (magic != parsed_file_.magic) [[unlikely]] {
        throw_format_error(fmtlib::format("magic number mismatch: {}", magic));
    }

    // skip gap
    source_->seek_ex(2, file::t_seek_mode::seek_from_current, p_abort);
    // extract rc4 key for audio content decoding
    source_->read(&parsed_file_.rc4_seed_len, sizeof(parsed_file_.rc4_seed_len), p_abort);
    if (!(to_parse & parse_targets::NCM_PARSE_AUDIO)) {
        source_->seek_ex(parsed_file_.rc4_seed_len, file::t_seek_mode::seek_from_current, p_abort);
        goto STATE_END_AUDIORC4;
    } else {
        if (0 == parsed_file_.rc4_seed_len || parsed_file_.rc4_seed_len > 256 || (parsed_file_.rc4_seed_len % cipher::AES_BLOCKSIZE))
            [[unlikely]] {
            throw_format_error("rc4 key length error");
        }
        // NOTE: rc4 key is encrypted by AES
        auto rc4key_raw = std::make_unique<uint8_t[]>(parsed_file_.rc4_seed_len);
        source_->read(rc4key_raw.get(), parsed_file_.rc4_seed_len, p_abort);
        std::for_each_n(rc4key_raw.get(), parsed_file_.rc4_seed_len, [](uint8_t &_b) { _b ^= 0x64; });
        cipher::make_AES_context_with_key(ncm_rc4_seed_aes_key)
            .set_chain_mode(cipher::aes_chain_mode::ECB)
            .set_input(rc4key_raw.get(), parsed_file_.rc4_seed_len)
            .set_output(rc4key_raw.get(), parsed_file_.rc4_seed_len)
            .decrypt_all();
        constexpr auto rc4key_magic = "neteasecloudmusic"sv;
        if (memcmp(rc4key_raw.get(), rc4key_magic.data(), rc4key_magic.size())) [[unlikely]] {
            throw_format_error("wrong rc4 key magic");
        }
        {
            auto _beg = rc4key_raw.get();
            auto _end = rc4key_raw.get() + parsed_file_.rc4_seed_len;
            _beg += rc4key_magic.size();
            _end -= cipher::guess_padding(_end);
            rc4_decryptor_ = cipher::abnormal_RC4(_beg, _end);
            parsed_file_.rc4_seed = rc4_decryptor_.key_seed().data();
        }
    }
STATE_END_AUDIORC4:
    // get meta info json
    source_->read(&parsed_file_.meta_len, sizeof(parsed_file_.meta_len), p_abort);
    if (!(to_parse & parse_targets::NCM_PARSE_META)) {
        source_->seek_ex(parsed_file_.meta_len, file::t_seek_mode::seek_from_current, p_abort);
        goto STATE_END_META;
    } else {
        if (0 == parsed_file_.meta_len) [[unlikely]] {
            WARN_LOG("No meta data found in ncm file: ", this_path_);
            meta_str_ = "{}"; // meta_parsed() is determined by the content of meta_str_
            meta_json_ = json::parse(meta_str_.c_str());
            goto STATE_END_META;
        } else {
            auto meta_b64 = std::make_unique<char[]>(parsed_file_.meta_len + 1);
            source_->read(meta_b64.get(), parsed_file_.meta_len, p_abort);
            std::for_each_n(meta_b64.get(), parsed_file_.meta_len, [](auto &b) { b ^= 0x63; });
            meta_b64[parsed_file_.meta_len] = '\0';
            if (strncmp(meta_b64.get(), meta_b64_hint.data(), meta_b64_hint.size())) {
                throw_format_error("wrong meta info hint");
            }
            auto meta_decrypt_buffer_size = pfc::base64_decode_estimate(meta_b64.get() + meta_b64_hint.size());
            auto meta_raw = std::make_unique<uint8_t[]>(meta_decrypt_buffer_size);
            pfc::base64_decode(meta_b64.get() + meta_b64_hint.size(), meta_raw.get());
            auto total = cipher::make_AES_context_with_key(ncm_meta_aes_key)
                             .set_chain_mode(cipher::aes_chain_mode::ECB)
                             .set_input(meta_raw.get(), meta_decrypt_buffer_size)
                             .set_output(meta_raw.get(), meta_decrypt_buffer_size)
                             .decrypt_all()
                             .outputted_len();
            total -= cipher::guess_padding(meta_raw.get() + meta_decrypt_buffer_size);
            meta_raw[total] = '\0';
            if (memcmp(meta_raw.get(), "music:", 6)) {
                throw_format_error("wrong meta info schema");
            }
            // skip heading `music:`
            meta_str_.assign(reinterpret_cast<const char *>(&meta_raw[6]), reinterpret_cast<const char *>(&meta_raw[total]));
            parsed_file_.meta_content = reinterpret_cast<uint8_t *>(meta_str_.data());
            meta_json_ = json::parse(meta_str_.c_str());
            if (!meta_json_.is_object()) {
                WARN_LOG("Failed to parse meta info of ncm file: ", this_path_);
            } else {
                DEBUG_LOG("Parsed NCM Meta: ", meta_json_.dump(2));

#if 0 // TODO: apply overwriting
                if (!meta_json_.HasMember(overwrite_key.data())) {
                    goto STATE_END_META;
                } else {
                    auto &overwrite = meta_json_[overwrite_key.data()];
                    if (!overwrite.IsObject()) {
                        goto STATE_END_META; // invalid overwrite
                    }
                    using map = std::unordered_map<std::string, rapidjson::Value>;
                    map iname_map;
                    auto upper = [](std::string &&s) {
                        std::ranges::transform(s, s.begin(), [](auto c) { return std::toupper(c); });
                        return s;
                    };
                    for (auto &m : meta_json_.GetObject()) {
                        std::string name = m.name.GetString();
                        iname_map[upper(std::move(name))] = m.value;
                    }
                }
#endif
            }
        }
    }

STATE_END_META:
    // skip gap
    source_->seek_ex(sizeof(parsed_file_.unknown_data), file::t_seek_mode::seek_from_current, p_abort);
    // get album image
    source_->read(&parsed_file_.album_image_size, sizeof(parsed_file_.album_image_size), p_abort);
    if (!(to_parse & parse_targets::NCM_PARSE_ALBUM)) {
        source_->seek_ex(parsed_file_.album_image_size[0], file::t_seek_mode::seek_from_current, p_abort);
        goto STATE_END_ALBUMIMG;
    } else {
        if (!parsed_file_.album_image_size[0]) {
            WARN_LOG("No album image found in ncm file: ", this_path_);
            goto STATE_END_ALBUMIMG;
        }
        parsed_file_.album_image_offset = source_->get_position(p_abort);
        image_data_.resize(parsed_file_.album_image_size[0]);
        source_->read(image_data_.data(), image_data_.size(), p_abort);
        parsed_file_.album_image = image_data_.data();
    }
STATE_END_ALBUMIMG:
    // remember where audio content starts
    parsed_file_.audio_content_offset = source_->get_position(p_abort);
}

bool ncm_file::save_raw_audio(const char *to_dir, abort_callback &p_abort) {
    if (!audio_parsed() || !meta_parsed()) {
        parse(parse_targets::NCM_PARSE_AUDIO | parse_targets::NCM_PARSE_META);
    }
    ENSURE_DECRYPTOR();
    auto ext = [this] {
        if (bool ok = meta_json_.contains("format"); ok) {
            const auto &format = meta_json_["format"].get_ref<const nlohmann::json::string_t &>();
            if (format == "flac") {
                return ".ncm.flac";
            }
            if (format == "mp3") {
                return ".ncm.mp3";
            }
        }
        return ".ncm.unknown";
    };
    auto output = pfc::string(to_dir);
    output.add_filename(pfc::string_filename(this->path()));
    output += ext();

    auto _seek_guard_ = make_seek_guard(p_abort);

    // NOTE:
    // If g_open_write_new() opens a file that is being played, the playback will be broken, and the file content will be truncated.
    // This is somewhat by design because I found it impossible to check if a file is opened in sharing mode,
    // and it's inefficient to check if the file is being played for every extraction, which usually happens rarely.
    // Besides, people usually won't choose the directory containing the songs currently being played as the output directory.
    // To avoid overwriting isn't a good idea either, because the user may want to extract more files to the same directory again,
    // and they likely won't unselect the existing files accurately.
    try {
        file_ptr file_raw;
        filesystem::g_open_write_new(file_raw, output, p_abort);

        if (auto size = file_v2::g_transfer(this, file_raw.get_ptr(), get_size(p_abort), p_abort); size == get_size(p_abort)) {
            DEBUG_LOG("Extraction done: ", output);
            path_raw_saved_to_ = output;
            return true;
        }
    } catch (const exception_aborted &) {
        DEBUG_LOG("Aborted: ", path());
    } catch (const pfc::exception &e) {
        ERROR_LOG("", e.what(), " (writing ", output, ") ");
    }
    DEBUG_LOG("Extraction failed: ", path());
    path_raw_saved_to_.clear();
    return false;
}

void ncm_file::overwrite_meta(const nlohmann::json &meta, abort_callback &p_abort) {
    if (!meta_parsed()) {
        parse(); // parse all => about to move the remaining contents
    }
    auto _seek_guard_ = make_seek_guard();

    // check if source file is opened in write mode
    source_->seek_ex(0, seek_from_beginning, p_abort);
    auto magic = ncm_magic;
    source_->write(&magic, sizeof(magic), p_abort);
    // passed

    // embed overwriting meta into the source
    json live_meta = json::parse(meta_str_); // NOTE:  use unmodified raw json string. `meta_json_` is merged with `overwrite` fields.
    if (live_meta.contains(overwrite_key)) {
        live_meta.erase(overwrite_key); // replace the old key
    }

    live_meta[overwrite_key] = meta;
    live_meta[overwrite_key].emplace(foo_input_ncm_comment_key, foo_input_ncm_comment);
    auto meta_str_to_write = std::string("music:") + live_meta.dump();

    DEBUG_LOG_F("Overwriting meta (to {}): {}", path(), meta_str_to_write);

    // NOTE: meta json processing:
    // 1. read <meta_len>
    // 2. XOR each byte by 0x63
    // 3. format: "164 key(Don't modify):<base64>"
    // 4. base64 decode
    // 5. AES ECB decrypt (pkcs#7 padding)
    // 6. "music:" <json>

    size_t meta_aligned_size = cipher::aligned(meta_str_to_write.size());
    auto _step1_handle_padding = [&] {
        uint8_t padding = meta_aligned_size - meta_str_to_write.size();
        meta_str_to_write.reserve(meta_aligned_size);
        for (auto i = meta_str_to_write.size(); i < meta_aligned_size; ++i) {
            meta_str_to_write.push_back(padding);
        }
    };
    [[maybe_unused]] auto _step2_aes = [&](uint8_t *meta_raw) -> size_t {
        auto total = cipher::make_AES_context_with_key(ncm_meta_aes_key)
                         .set_chain_mode(cipher::aes_chain_mode::ECB)
                         .set_input(meta_raw, meta_aligned_size)
                         .set_output(meta_raw, meta_aligned_size)
                         .encrypt_chunk(meta_aligned_size)
                         .outputted_len();
        if (total != meta_aligned_size) {
            uBugCheck();
        }
        return total; // should be equal to meta_aligned_size
    };
    auto _step3_base64 = [&](void *meta_raw) -> pfc::string {
        pfc::string out;
        pfc::base64_encode(out, meta_raw, meta_aligned_size);
        return pfc::string(meta_b64_hint.data(), meta_b64_hint.size()) + out;
    };
    auto _step4_xor = [&](pfc::string &&meta_b64) {
        auto out_raw = std::make_unique<uint8_t[]>(meta_b64.get_length());
        std::span<uint8_t> s(out_raw.get(), meta_b64.get_length());
        std::copy(meta_b64.get_ptr(), meta_b64.get_ptr() + meta_b64.get_length(), s.begin());
        std::for_each(s.begin(), s.end(), [](auto &b) { b ^= 0x63; });
        return out_raw;
    };

    _step1_handle_padding();
    _step2_aes(reinterpret_cast<uint8_t *>(meta_str_to_write.data()));
    auto meta_b64 = _step3_base64(meta_str_to_write.data());
    const size_t new_meta_raw_len = meta_b64.get_length();
    auto new_meta_raw = _step4_xor(std::move(meta_b64));

    file_ptr tmp_file;
    filesystem::g_open_temp(tmp_file, p_abort);
    {
        size_t untouched_size = 14 /*offsetof(ncm_file_st, rc4_seed)*/ + parsed_file_.rc4_seed_len;
        uint8_t untouched_buffer[512 /*rc4_seed_len is never longer than 256*/];
        source_->seek_ex(0, seek_from_beginning, p_abort);
        source_->read(untouched_buffer, untouched_size, p_abort);
        tmp_file->write(untouched_buffer, untouched_size, p_abort);
    }
    tmp_file->write_lendian_t(static_cast<uint32_t>(new_meta_raw_len), p_abort);
    tmp_file->write(new_meta_raw.get(), new_meta_raw_len, p_abort);
    // finish transferring remaiing data
    source_->seek_ex(14 /*offsetof(ncm_file_st, rc4_seed)*/ + parsed_file_.rc4_seed_len, seek_from_beginning, p_abort);
    source_->seek_ex(sizeof(parsed_file_.meta_len) + parsed_file_.meta_len, seek_from_current, p_abort);
    file::g_transfer(source_, tmp_file, source_->get_remaining(p_abort), p_abort);
    // write back to current file
    tmp_file->seek_ex(0, seek_from_beginning, p_abort);
    source_->seek_ex(0, seek_from_beginning, p_abort);
    source_->set_eof(p_abort);
    file::g_transfer(tmp_file, source_, tmp_file->get_size(p_abort), p_abort);
}

void ncm_file::reset_album_image(album_art_data_ptr image, abort_callback &p_abort) {
    throw pfc::exception_not_implemented();
}
