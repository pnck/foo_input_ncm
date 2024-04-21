#include "stdafx.h"
#include "ncm_file.hpp"
#include "rapidjson/include/rapidjson/stringbuffer.h"

#include <algorithm>
#include <span>
#include <ranges>

using namespace fb2k_ncm;

t_size fb2k_ncm::ncm_file::read(void *p_buffer, t_size p_bytes, abort_callback &p_abort) {
    if (!rc4_decrypter_.is_valid()) {
        FB2K_console_print("[ERR] Decryptor error.");
        throw exception_io();
    } else {
        auto source_pos = source_->get_position(p_abort);
        auto read_offset = source_pos - parsed_file_.audio_content_offset;
        std::vector<uint8_t> enc(p_bytes);

        t_size total = 0;
        total = source_->read(enc.data(), p_bytes, p_abort);
        if (!total) {
            return 0;
        }

        auto dec = rc4_decrypter_.reset_counter(read_offset).transform(enc);
        std::span<uint8_t> output(static_cast<uint8_t *>(p_buffer), p_bytes);
        std::copy(std::begin(dec), std::end(dec), std::begin(output));
        DEBUG_LOG("[DEBUG] Read at ", read_offset, " (", source_pos, ") : req=", p_bytes, " real=", total);
        return total;
    }
}

// @attention this function is not implemented / not in use
void fb2k_ncm::ncm_file::write(const void *p_buffer, t_size p_bytes, abort_callback &p_abort) {
    auto source_pos = source_->get_position(p_abort);
    if (source_pos < parsed_file_.audio_content_offset) {
        FB2K_console_print("[ERR] Modification (metadata) on a ncm file is not supported.");
        throw exception_io_write_protected();
    }
    auto write_offset = source_pos - parsed_file_.audio_content_offset;
    auto transform = rc4_decrypter_.get_transform();
    auto _tmp = std::make_unique<uint8_t[]>(p_bytes);
    for (size_t i = 0; i < p_bytes; ++i) {
        _tmp[i] = transform(static_cast<const uint8_t *>(p_buffer)[i], (write_offset & 0xff) + i);
    }
    return source_->write(_tmp.get(), p_bytes, p_abort);
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
    FB2K_console_print("[ERR] Modification (resize) on a ncm file is not supported.");
    throw exception_io_write_protected();
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
    if (parsed_file_.audio_content_offset) {
        source_->seek(parsed_file_.audio_content_offset, p_abort);
    }
}

bool fb2k_ncm::ncm_file::is_remote() {
    return false;
}

t_filetimestamp fb2k_ncm::ncm_file::get_timestamp(abort_callback &p_abort) {
    return source_->get_timestamp(p_abort);
}

void ncm_file::ensure_audio_offset() {
    if (!parsed_file_.audio_content_offset) {
        uBugCheck();
    }
}

inline void ncm_file::throw_format_error(const char *extra) {
    FB2K_console_print("[ERR] Unsupported format or corrupted file (", extra, ").");
    throw exception_io_unsupported_format();
}

inline void ncm_file::throw_format_error(std::string extra) {
    throw_format_error(extra.c_str());
}

void ncm_file::parse(uint16_t to_parse /* = 0xff*/) {
    DEBUG_LOG("[DEBUG] Parse (C=", to_parse, ") ", this_path_);

    auto &p_abort = fb2k::noAbort;

    auto _seek_guard_ = std::unique_ptr<void, std::function<void(void *)>>(nullptr, [this, p = source_->get_position(p_abort)](void *) {
        // RAII guard, will seek back when function returns
        source_->seek_ex(p, file::t_seek_mode::seek_from_beginning, fb2k::noAbort);
    });

    source_->seek_ex(0, file::t_seek_mode::seek_from_beginning, p_abort);

    uint64_t magic = 0;
    source_->read(&magic, sizeof(uint64_t), p_abort);
    if (magic != parsed_file_.magic) {
        throw_format_error(std::string("magic number mismatch: ") + std::to_string(magic));
    }

    // skip gap
    source_->seek_ex(2, file::t_seek_mode::seek_from_current, p_abort);

    // extract rc4 key for audio content decoding
    source_->read(&parsed_file_.rc4_seed_len, sizeof(parsed_file_.rc4_seed_len), p_abort);
    if (to_parse & parse_contents::NCM_PARSE_AUDIO) {
        if (0 == parsed_file_.rc4_seed_len || parsed_file_.rc4_seed_len > 256) {
            throw_format_error("rc4 key length error");
        }
        auto data_size = cipher::aligned(parsed_file_.rc4_seed_len);
        auto data = std::make_unique<uint8_t[]>(data_size);
        source_->read(data.get(), parsed_file_.rc4_seed_len, p_abort);
        std::for_each_n(data.get(), parsed_file_.rc4_seed_len, [](uint8_t &_b) { _b ^= 0x64; });
        cipher::make_AES_context_with_key(ncm_rc4_seed_aes_key)
            .set_chain_mode(cipher::aes_chain_mode::ECB)
            .set_input(data.get(), parsed_file_.rc4_seed_len)
            .set_output(data.get(), data_size)
            .decrypt_all();
        if (strncmp(reinterpret_cast<char *>(data.get()), "neteasecloudmusic", 17)) {
            throw_format_error("wrong rc4 key magic");
        }
        rc4_decrypter_ = cipher::abnormal_RC4(
            {data.get() + 17, data.get() + parsed_file_.rc4_seed_len - cipher::guess_padding(data.get() + parsed_file_.rc4_seed_len)});
        parsed_file_.rc4_seed_ = rc4_decrypter_.key_seed.data();
    } else {
        source_->seek_ex(parsed_file_.rc4_seed_len, file::t_seek_mode::seek_from_current, p_abort);
    }
    // get meta info json
    source_->read(&parsed_file_.meta_len, sizeof(parsed_file_.meta_len), p_abort);
    if (to_parse & parse_contents::NCM_PARSE_META) {
        if (0 == parsed_file_.meta_len) {
            FB2K_console_formatter() << "[WARN] No meta data found in ncm file: " << this_path_;
            meta_str_ = "{}"; // meta_parsed() is determined by the content of meta_str_
            goto ENDMETA;
        } else {
            auto meta_b64 = std::make_unique<char[]>(parsed_file_.meta_len + 1);
            source_->read(meta_b64.get(), parsed_file_.meta_len, p_abort);
            std::for_each_n(meta_b64.get(), parsed_file_.meta_len, [](auto &b) { b ^= 0x63; });
            meta_b64[parsed_file_.meta_len] = '\0';
            constexpr std::string_view meta_info_hint = "163 key(Don't modify):";
            if (strncmp(meta_b64.get(), meta_info_hint.data(), meta_info_hint.size())) {
                throw_format_error("wrong meta info hint");
            }
            auto meta_decrypt_buffer_size = pfc::base64_decode_estimate(meta_b64.get() + meta_info_hint.size());
            auto meta_raw = std::make_unique<uint8_t[]>(meta_decrypt_buffer_size);
            pfc::base64_decode(meta_b64.get() + meta_info_hint.size(), meta_raw.get());
            auto total = cipher::make_AES_context_with_key(ncm_meta_aes_key)
                             .set_chain_mode(cipher::aes_chain_mode::ECB)
                             .set_input(meta_raw.get(), meta_decrypt_buffer_size)
                             .set_output(meta_raw.get(), meta_decrypt_buffer_size)
                             .decrypt_all()
                             .outputted_len();
            total -= cipher::guess_padding(meta_raw.get() + meta_decrypt_buffer_size);
            meta_raw[total] = '\0';
            if (strncmp(reinterpret_cast<const char *>(meta_raw.get()), "music:", 6)) {
                throw_format_error("wrong meta info schema");
            }
            // skip heading `music:`
            meta_str_.assign(reinterpret_cast<const char *>(&meta_raw[6]));
            parsed_file_.meta_content = reinterpret_cast<uint8_t *>(meta_str_.data());
            meta_json_.Parse(meta_str_.c_str());
            if (!meta_json_.IsObject()) {
                FB2K_console_formatter() << "[WARN] Failed to parse meta info of ncm file: " << this_path_;
            } else {
                DEBUG_LOG("[DEBUG] Metainfo str: ", meta_str_.c_str());
            }
        }
    } else {
        source_->seek_ex(parsed_file_.meta_len, file::t_seek_mode::seek_from_current, p_abort);
    }

ENDMETA:
    // skip gap
    source_->seek_ex(sizeof(parsed_file_.unknown_data), file::t_seek_mode::seek_from_current, p_abort);
    // get album image
    source_->read(&parsed_file_.album_image_size, sizeof(parsed_file_.album_image_size), p_abort);
    if (to_parse & parse_contents::NCM_PARSE_ALBUM) {
        if (!parsed_file_.album_image_size[0]) {
            FB2K_console_formatter() << "[WARN] No album image found in ncm file: " << this_path_;
            goto ENDIMG;
        }
        parsed_file_.album_image_offset = source_->get_position(p_abort);
        image_data_.resize(parsed_file_.album_image_size[0]);
        source_->read(image_data_.data(), image_data_.size(), p_abort);
        parsed_file_.album_image = image_data_.data();
    } else {
        source_->seek_ex(parsed_file_.album_image_size[0], file::t_seek_mode::seek_from_current, p_abort);
    }
ENDIMG:
    // remember where audio content starts
    parsed_file_.audio_content_offset = source_->get_position(p_abort);
}
