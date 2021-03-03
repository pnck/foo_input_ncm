#include "stdafx.h"
#include "ncm_file.h"
#include "cipher/cipher.h"

#include "rapidjson/include/rapidjson/stringbuffer.h"

#include <algorithm>

using namespace fb2k_ncm;

t_size fb2k_ncm::ncm_file::read(void *p_buffer, t_size p_bytes, abort_callback &p_abort) {
    if (!rc4_decrypter_.is_valid()) {
        throw exception_io_data();
    } else {
        auto source_pos = source_->get_position(p_abort);
        auto read_offset = source_pos - audio_content_offset_;
        auto _tmp = std::make_unique<uint8_t[]>(p_bytes);
        t_size total = 0;
        total = source_->read(_tmp.get(), p_bytes, p_abort);
        for (size_t i = 0; i < total; ++i) {
            static_cast<uint8_t *>(p_buffer)[i] = rc4_decrypter_.get_transform()(_tmp[i], (read_offset & 0xff) + i);
        }
        return total;
    }
}

void fb2k_ncm::ncm_file::write(const void *p_buffer, t_size p_bytes, abort_callback &p_abort) {
    auto source_pos = source_->get_position(p_abort);
    if (source_pos < audio_content_offset_) {
        throw exception_io_write_protected("Modifying ncm file not supported");
    }
    auto write_offset = source_pos - audio_content_offset_;
    auto _tmp = std::make_unique<uint8_t[]>(p_bytes);
    for (size_t i = 0; i < p_bytes; ++i) {
        _tmp[i] = rc4_decrypter_.get_transform()(static_cast<const uint8_t *>(p_buffer)[i], (write_offset + i) & 0xff);
    }
    return source_->write(_tmp.get(), p_bytes, p_abort);
}

t_filesize fb2k_ncm::ncm_file::get_size(abort_callback &p_abort) {
    return source_->get_size(p_abort) - audio_content_offset_;
}

t_filesize fb2k_ncm::ncm_file::get_position(abort_callback &p_abort) {
    return source_->get_position(p_abort) - audio_content_offset_;
}

void fb2k_ncm::ncm_file::resize(t_filesize p_size, abort_callback &p_abort) {
    throw exception_io_write_protected("Modifying ncm file not supported");
}

void fb2k_ncm::ncm_file::seek(t_filesize p_position, abort_callback &p_abort) {
    return source_->seek(audio_content_offset_ + p_position, p_abort);
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
    source_->seek(audio_content_offset_, p_abort);
}

bool fb2k_ncm::ncm_file::is_remote() {
    return false;
}

t_filetimestamp fb2k_ncm::ncm_file::get_timestamp(abort_callback &p_abort) {
    return source_->get_timestamp(p_abort);
}

PFC_NORETURN void ncm_file::ensure_audio_offset() {
    if (!audio_content_offset_) {
        // initialize should always follow open
        uBugCheck();
    }
}

inline void ncm_file::throw_format_error() {
    throw exception_io_unsupported_format("Unsupported format or corrupted file");
}

void ncm_file::parse() {
    abort_callback_dummy p_abort;
    source_->reopen(p_abort);
    uint64_t magic = 0;
    source_->read(&magic, sizeof(uint64_t), p_abort);
    if (magic != parsed_file_.magic) {
        throw_format_error();
    }
    // skip gap
    source_->seek_ex(2, file::t_seek_mode::seek_from_current, p_abort);

    // extract rc4 key for audio content decoding
    source_->read(&parsed_file_.rc4_seed_len, sizeof(parsed_file_.rc4_seed_len), p_abort);
    if (0 == parsed_file_.rc4_seed_len || parsed_file_.rc4_seed_len > 256) {
        throw_format_error();
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
        throw_format_error();
    }
    rc4_decrypter_ =
        cipher::abnormal_RC4({data.get() + 17, data.get() + parsed_file_.rc4_seed_len -
                                                   cipher::guess_padding(data.get() + parsed_file_.rc4_seed_len)});

    // get meta info json
    source_->read(&parsed_file_.meta_len, sizeof(parsed_file_.meta_len), p_abort);
    if (0 == parsed_file_.meta_len) {
        FB2K_console_formatter() << "[WARN] No meta data found in ncm file: " << this_path_;
        goto NOMETA;
    }
    [this, &p_abort] {
        auto meta_b64 = std::make_unique<char[]>(parsed_file_.meta_len + 1);
        source_->read(meta_b64.get(), parsed_file_.meta_len, p_abort);
        std::for_each_n(meta_b64.get(), parsed_file_.meta_len, [](auto &b) { b ^= 0x63; });
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
                         .decrypt_all()
                         .outputted_len();
        total -= cipher::guess_padding(meta_raw.get() + meta_decrypt_buffer_size);
        meta_raw[total] = '\0';
        if (strncmp(reinterpret_cast<char *>(meta_raw.get()), "music:", 6)) {
            throw_format_error();
        }
        // skip heading `music:`
        meta_str_.assign(reinterpret_cast<char *>(meta_raw.get()) + 6);
        parsed_file_.meta_content = reinterpret_cast<uint8_t *>(meta_str_.data());
        meta_json_.Parse(meta_str_.c_str());
        if (!meta_json_.IsObject()) {
            FB2K_console_formatter() << "[WARN] Failed to parse meta info of ncm file: " << this_path_;
        }
    }();
NOMETA:
    // skip gap
    source_->seek_ex(sizeof(parsed_file_.unknown_data), file::t_seek_mode::seek_from_current, p_abort);
    // get album image
    source_->read(&parsed_file_.album_image_size, sizeof(parsed_file_.album_image_size), p_abort);
    if (!parsed_file_.album_image_size[0]) {
        FB2K_console_formatter() << "[WARN] No album image found in ncm file: " << this_path_;
        goto NOALBUMIMG;
    }
    image_data_.resize(parsed_file_.album_image_size[0]);
    source_->read(image_data_.data(), image_data_.size(), p_abort);
    parsed_file_.album_image = image_data_.data();
NOALBUMIMG:
    // remember where audio content starts
    audio_content_offset_ = source_->get_position(p_abort);
}