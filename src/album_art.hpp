#pragma once

#include "stdafx.h"
#include "common/consts.hpp"
#include "ncm_file.hpp"

namespace fb2k_ncm
{

    class ncm_album_art_extractor : public album_art_extractor_v2 {
        FB2K_MAKE_SERVICE_INTERFACE(ncm_album_art_extractor, album_art_extractor_v2);

    public:
        GUID get_guid() override { return class_guid; }
        bool is_our_path(const char *p_path, const char *p_extension) override;
        album_art_extractor_instance_ptr open(file_ptr p_filehint, const char *p_path, abort_callback &p_abort) override;
    };

    class ncm_album_art_editor : public album_art_editor_v2 {
        FB2K_MAKE_SERVICE_INTERFACE(ncm_album_art_editor, album_art_editor_v2);

    public:
        GUID get_guid() override { return class_guid; }
        bool is_our_path(const char *p_path, const char *p_extension) override;
        album_art_editor_instance_ptr open(file_ptr p_filehint, const char *p_path, abort_callback &p_abort) override;
    };

    class ncm_album_art_editor_instance : public album_art_editor_instance_v2 {
        ncm_file::ptr ncm_file_;
        album_art_data_ptr cache_album_art_data_;

    public:
        explicit ncm_album_art_editor_instance(ncm_file::ptr p_ncm_file) : ncm_file_(std::move(p_ncm_file)) {}
        album_art_data_ptr query(const GUID & p_what,abort_callback & p_abort) override ;
        void set(const GUID &p_what, album_art_data_ptr p_data, abort_callback &p_abort) override;
        void remove(const GUID &p_what) override;
        void commit(abort_callback &p_abort) override;
        void remove_all() override;
    };

    FOOGUIDDECL constexpr GUID ncm_album_art_extractor::class_guid = guid_candidates[0];
    FOOGUIDDECL constexpr GUID ncm_album_art_editor::class_guid = guid_candidates[0];

} // namespace fb2k_ncm
