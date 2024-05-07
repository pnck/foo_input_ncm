#include "stdafx.h"
#include "common/log.hpp"
#include "album_art.hpp"

using namespace fb2k_ncm;

bool ncm_album_art_extractor::is_our_path(const char *p_path, const char *p_extension) {
    return stricmp_utf8(p_extension, "ncm") == 0;
}

bool ncm_album_art_editor::is_our_path(const char *p_path, const char *p_extension) {
    return stricmp_utf8(p_extension, "ncm") == 0;
}

album_art_extractor_instance_ptr ncm_album_art_extractor::open(file_ptr p_filehint, const char *p_path, abort_callback &p_abort) {
    fb2k_ncm::ncm_file::ptr _ncm_file;
    do {
        if (p_filehint.is_valid()) {
            if (auto ok = p_filehint->cast(_ncm_file); ok) {
                break;
            }
        }

        if (is_our_path(p_path, pfc::string_extension(p_path))) {
            _ncm_file = fb2k::service_new<fb2k_ncm::ncm_file>(p_path);
            break;
        }
        input_open_file_helper(p_filehint, p_path, input_open_info_read, p_abort);
        if (auto ok = p_filehint->cast(_ncm_file); ok) {
            break;
        }

        DEBUG_LOG("ncm_album_art_extractor::open cast from file_ptr failed");
        throw exception_io_unsupported_feature();
    } while (false);

    if (_ncm_file->image_data().empty()) {
        _ncm_file->parse(ncm_file::parse_targets::NCM_PARSE_ALBUM);
    }

    if (_ncm_file->image_data().empty()) {
        throw exception_album_art_not_found();
    }

    auto album_art_instance = fb2k::service_new<album_art_extractor_instance_simple>();
    auto image = album_art_data_impl::g_create(_ncm_file->image_data().data(), _ncm_file->image_data().size());
    album_art_instance->set(album_art_ids::cover_front, image);
    // album_art_instance->set(album_art_ids::disc, image);
    return album_art_instance;
}

album_art_editor_instance_ptr ncm_album_art_editor::open(file_ptr p_filehint, const char *p_path, abort_callback &p_abort) {
    fb2k_ncm::ncm_file::ptr _ncm_file;
    do {
        if (p_filehint.is_valid()) {
            if (auto ok = p_filehint->cast(_ncm_file); ok) {
                break;
            }
        }

        if (is_our_path(p_path, pfc::string_extension(p_path))) {
            _ncm_file = fb2k::service_new<fb2k_ncm::ncm_file>(p_path, filesystem::open_mode_write_existing);
            break;
        }

        input_open_file_helper(p_filehint, p_path, input_open_info_write, p_abort);
        if (auto ok = p_filehint->cast(_ncm_file); ok) {
            break;
        }

        DEBUG_LOG("ncm_album_art_editor::open cast from file_ptr failed");
        throw exception_io_unsupported_feature();
    } while (false);

    return fb2k::service_new<ncm_album_art_editor_instance>(_ncm_file);
}

void ncm_album_art_editor_instance::set(const GUID &p_what, album_art_data_ptr p_data, abort_callback &p_abort) {
    if (p_what != album_art_ids::cover_front) {
        throw exception_album_art_unsupported_entry();
    }
    cache_album_art_data_ = p_data;
}
void ncm_album_art_editor_instance::remove(const GUID &p_what) {
    if (p_what != album_art_ids::cover_front) {
        // throw exception_album_art_unsupported_entry();
    } else {
        remove_all();
    }
}
void ncm_album_art_editor_instance::commit(abort_callback &p_abort) {
    ncm_file_->reset_album_image(cache_album_art_data_, p_abort);
    if (cache_album_art_data_.is_valid()) {
        cache_album_art_data_.release();
    }
}
void ncm_album_art_editor_instance::remove_all() {
    cache_album_art_data_.release();
}

album_art_data_ptr ncm_album_art_editor_instance::query(const GUID &p_what, abort_callback &p_abort) {
    if (p_what != album_art_ids::cover_front) {
        throw exception_album_art_unsupported_entry();
    }
    if (cache_album_art_data_.is_empty()) {
        if (ncm_file_->image_data().empty()) {
            ncm_file_->parse(ncm_file::parse_targets::NCM_PARSE_ALBUM);
        }
        cache_album_art_data_ = album_art_data_impl::g_create(ncm_file_->image_data().data(), ncm_file_->image_data().size());
    }
    return cache_album_art_data_;
}
static service_factory_single_t<ncm_album_art_extractor> g_ncm_album_art_extractor;
static service_factory_single_t<ncm_album_art_editor> g_ncm_album_art_editor;
