#pragma once

#include "stdafx.h"
#include "common/consts.hpp"

#include <map>
#include <tuple>
#include <string>
#include <optional>

namespace fb2k_ncm::ui
{
    class context_menu : public contextmenu_item_simple {
        using base_t = contextmenu_item_simple;
        enum {
            CMD_EXTRACT = 0, // retrieve wrapped content
            CMD_CONVERT,     // retrieve and retag
            __CMD_LAST__,
        };

    public:
        constexpr static GUID class_guid = guid_candidates[2];
        GUID get_parent() { return class_guid; }
        uint32_t get_num_items() {
            // NOTE: To be compatible with old (like v1.6) version
            // Ensure filedialog api works. If not, return 0 to hide the menu, 
            // in order to prevent the dialog from being called.
            fb2k::fileDialog::ptr _;
            if (!fb2k::std_api_try_get<fb2k::fileDialog>(_)) {
                return 0;
            }
            return __CMD_LAST__;
        }

    public:
        void get_item_name(unsigned p_index, pfc::string_base &p_out);
        GUID get_item_guid(uint32_t p_index);
        bool get_item_description(unsigned p_index, pfc::string_base &p_out);
        void context_command(unsigned p_index, metadb_handle_list_cref p_data, const GUID &p_caller);
        bool context_get_display(unsigned p_index, metadb_handle_list_cref p_data, pfc::string_base &p_out, unsigned &p_displayflags,
                                 const GUID &p_caller);

    private:
        using name_t = std::string;
        using descript_t = name_t;
        using maybe_item_t = std::optional<std::tuple<GUID, name_t, descript_t>>;
        constexpr static maybe_item_t get_menu_item_(uint32_t index);
    };

} // namespace fb2k_ncm::ui
