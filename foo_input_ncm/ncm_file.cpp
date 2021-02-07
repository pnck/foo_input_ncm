#include "stdafx.h"
#include "ncm_file.h"

#include <algorithm>

using namespace fb2k_ncm;

t_size fb2k_ncm::ncm_file::read(void *p_buffer, t_size p_bytes, abort_callback &p_abort) {
    if (!rc4_decrypter.is_valid()) {
        throw exception_io_data();
    } else {
        auto source_pos = source_->get_position(p_abort);
        auto read_offset = source_pos - audio_content_offset_;
        auto _tmp = std::make_unique<uint8_t[]>(p_bytes);
        t_size total = 0;
        total = source_->read(_tmp.get(), p_bytes, p_abort);
        for (size_t i = 0; i < total; ++i) {
            static_cast<uint8_t *>(p_buffer)[i] = rc4_decrypter.get_transform()(_tmp[i], (read_offset & 0xff) + i);
        }
        return total;
    }
}

void fb2k_ncm::ncm_file::write(const void *p_buffer, t_size p_bytes, abort_callback &p_abort) {
    throw exception_io_write_protected("Modifying ncm file not supported");
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
