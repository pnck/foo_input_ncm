#pragma once

#include "stdafx.h"
#include "ncm_file.hpp"

namespace fb2k_ncm
{

    class ncm_album_art_extractor : public album_art_extractor {
    public:
        // GUID get_guid() override;
        bool is_our_path(const char *p_path, const char *p_extension) override;
        album_art_extractor_instance_ptr open(file_ptr p_filehint, const char *p_path, abort_callback &p_abort) override;
    };

} // namespace fb2k_ncm
