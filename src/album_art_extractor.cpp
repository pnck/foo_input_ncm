#include "album_art_extractor.hpp"

using namespace fb2k_ncm;

bool ncm_album_art_extractor::is_our_path(const char *p_path, const char *p_extension) {
    return stricmp_utf8(p_extension, "ncm") == 0;
}

album_art_extractor_instance_ptr ncm_album_art_extractor::open(file_ptr p_filehint, const char *p_path,
                                                               abort_callback &p_abort) {
    if (p_filehint.is_empty()) {
        if (is_our_path(p_path, pfc::string_extension(p_path).toString())) {
            filesystem::g_open_read(p_filehint, p_path, p_abort);
        }
    }
    auto _ncm_file = fb2k::service_new<ncm_file>(p_filehint, p_path);
    _ncm_file->parse(ncm_file::parse_contents::NCM_PARSE_ALBUM);
    auto album_arts = fb2k::service_new<album_art_extractor_instance_simple>();
    auto image = album_art_data_impl::g_create(_ncm_file->image_data.data(), _ncm_file->image_data.size());
    album_arts->set(album_art_ids::cover_front, image);
    // album_arts->set(album_art_ids::disc, image);
    return album_arts;
}

static service_factory_single_t<ncm_album_art_extractor> g_ncm_album_art_extractor;