#include "stdafx.h"
#include "common/log.hpp"
#include "album_art_extractor.hpp"

using namespace fb2k_ncm;

bool ncm_album_art_extractor::is_our_path(const char *p_path, const char *p_extension) {
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

    auto album_art_instance = fb2k::service_new<album_art_extractor_instance_simple>();
    auto image = album_art_data_impl::g_create(_ncm_file->image_data().data(), _ncm_file->image_data().size());
    album_art_instance->set(album_art_ids::cover_front, image);
    // album_art_instance->set(album_art_ids::disc, image);
    return album_art_instance;
}

static service_factory_single_t<ncm_album_art_extractor> g_ncm_album_art_extractor;
