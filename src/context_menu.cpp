#include "stdafx.h"
#include "context_menu.hpp"
#include "common/helpers.hpp"
#include "input_ncm.hpp"

#include <numeric>
#include <ranges>
#include <tuple>
#include <atomic>
#include <functional>

namespace fb2k_ncm::ui
{
    /// @attention File dialog callback is being executed after the function returns.
    /// Everything should be properly moved into the callback function.
    void run_cmd_extract(metadb_handle_list_cref p_data, const GUID &p_caller,
                         std::function<bool(std::vector<service_ptr_t<fb2k_ncm::ncm_file>>)> &&continuation = {}) {
        std::vector<service_ptr_t<fb2k_ncm::ncm_file>> ncm_files;
        for (auto item : p_data) {
            if (pfc::string_extension(item->get_path()) != "ncm") {
                continue;
            }
            auto ncm_file = fb2k::service_new<fb2k_ncm::ncm_file>(item->get_path());
            ncm_files.emplace_back(std::move(ncm_file));
        }

        if (ncm_files.empty()) {
            return;
        }

        auto file_dialog = fb2k::fileDialog::get()->setupOpenFolder();
        file_dialog->setTitle(PFC_string_formatter() << "Save " << ncm_files.size() << " audio file(s) to...");
        file_dialog->runSimple(
            [cps_params = std::make_tuple(std::move(continuation), std::move(ncm_files))](fb2k::stringRef selected_path) {
                auto [continuation, ncm_files] = cps_params;
                bool all_done = true;
                for (auto &f : ncm_files) {
                    DEBUG_LOG("[DEBUG] Extracting audio content from ", f->path());
                    if (auto ok = f->save_raw_audio(selected_path->c_str(), fb2k::mainAborter()); !ok) {
                        DEBUG_LOG("[DEBUG] Failed to extract from ", f->path());
                        all_done = false;
                    }
                }
                if (all_done) {
                    // call CPS function
                    if (auto ok = (!continuation) || continuation(std::forward<std::decay_t<decltype(ncm_files)>>(ncm_files)); ok) {
                        popup_message::g_show("All audio content extracted successfully", "Extraction completed",
                                              popup_message ::icon_information);
                        return;
                    }
                }
                // TODO: list failed files
                popup_message::g_show("Some audio content failed to extract", "Extraction failed", popup_message::icon_error);
            });
    }
    void run_cmd_extract_retag(metadb_handle_list_cref p_data, const GUID &p_caller) {
        run_cmd_extract(p_data, p_caller, [](std::vector<service_ptr_t<fb2k_ncm::ncm_file>> ncm_files) -> bool {
            DEBUG_LOG("[DEBUG] Retagging extracted ", ncm_files.size(), " audio files...");
            bool all_done = true;
            for (auto &f : ncm_files) {
                auto input = input_entry::g_find_by_guid(input_ncm::class_guid);
                service_ptr_t<input_info_reader> info_reader;
                input->open_for_info_read(info_reader, f, "", fb2k::noAbort);
                file_info_impl info;
                info_reader->get_info(0, info, fb2k::noAbort);
                auto to_retag = f->saved_raw_path();
                if (to_retag.empty()) {
                    DEBUG_LOG("[DEBUG] Not extracted:", f->path());
                    all_done = false;
                    continue;
                }
                service_ptr_t<input_info_writer> retagger;
                input_entry::g_open_for_info_write(retagger, file_ptr(), to_retag.data(), fb2k::noAbort);
                retagger->set_info(0, info, fb2k::noAbort);
                retagger->commit(fb2k::noAbort);
                retagger = nullptr; // release, to let album editor reopen the file

                try {
                    auto album_extractor = album_art_extractor::g_open(f, f->path(), fb2k::noAbort);
                    auto album_writer = album_art_editor::g_open(nullptr, to_retag.data(), fb2k::noAbort);
                    if (album_extractor.is_valid() && album_writer.is_valid()) {
                        if (auto data = album_extractor->query(album_art_ids::cover_front, fb2k::noAbort); data.is_valid()) {
                            album_writer->set(album_art_ids::cover_front, data, fb2k::noAbort);
                            album_writer->commit(fb2k::noAbort);
                        }
                    }
                    DEBUG_LOG("[DEBUG] Retagged ", to_retag.data());
                } catch (const pfc::exception &e) {
                    FB2K_console_print("[ERR] Failed to retag ", to_retag.data(), ":", e.what());
                    all_done = false;
                }
            }
            return all_done;
        });
    }
} // namespace fb2k_ncm::ui

using namespace fb2k_ncm::ui;

static auto g_menu_factory = contextmenu_item_factory_t<context_menu>();
static auto g_menu = contextmenu_group_popup_factory(context_menu::class_guid, contextmenu_groups::root, "NCM Loader", 0);

constexpr auto context_menu::get_menu_item_(uint32_t index) -> std::optional<std::tuple<GUID, name_t, descript_t>> {
    switch (index) {
    case CMD_EXTRACT:
        return {
            {// GUID
             {0x808d1e41, 0xfba, 0x42b0, {0x91, 0xda, 0xd2, 0x6b, 0x59, 0xee, 0x18, 0xcf}},
             // name
             "Extract audio",
             // description
             "Extract audio content wrapped by NCM file"},
        };
    case CMD_CONVERT:
        return {
            {// GUID
             {0x708c2d3e, 0x2c7b, 0x4d3a, {0x9c, 0x1d, 0xe4, 0x8d, 0x6e, 0x1f, 0x20, 0xed}},
             // name
             "Extract audio and retag",
             // description
             "Extract audio and rebuild its meta info"},
        };
    default:
        return std::nullopt;
    }
}

GUID context_menu::get_item_guid(uint32_t p_index) {
    if (auto item = get_menu_item_(p_index); item.has_value()) {
        return std::get<0>(*item);
    }
    uBugCheck();
    return {};
}

void context_menu::get_item_name(unsigned p_index, pfc::string_base &p_out) {
    if (auto item = get_menu_item_(p_index); item.has_value()) {
        p_out = std::get<1>(*item).c_str();
        return;
    }
    uBugCheck();
}

bool context_menu::get_item_description(unsigned int p_index, pfc::string_base &p_out) {
    if (auto item = get_menu_item_(p_index); item.has_value()) {
        p_out = std::get<2>(*item).c_str();
        return true;
    }
    return false;
}

void context_menu::context_command(unsigned int p_index, metadb_handle_list_cref p_data, const GUID &p_caller) {
    switch (p_index) {
    case CMD_EXTRACT:
        fb2k_ncm::ui::run_cmd_extract(p_data, p_caller);
        break;
    case CMD_CONVERT:
        fb2k_ncm::ui::run_cmd_extract_retag(p_data, p_caller);
        break;
    default:
        uBugCheck();
    }
}

bool context_menu::context_get_display(unsigned p_index, metadb_handle_list_cref p_data, pfc::string_base &p_out, unsigned &p_displayflags,
                                       const GUID &p_caller) {
    int count = 0;
    p_out = "";
    if (p_index == CMD_EXTRACT || p_index == CMD_CONVERT) {
        auto filter = [](auto acc, auto item) {
            if (pfc::string_extension(item->get_path()) == "ncm") {
                return acc + 1;
            }
            return acc;
        };
        count = std::accumulate(p_data.begin(), p_data.end(), 0, filter);
    }
    if (p_index == CMD_EXTRACT) {
        p_out << "Extract RAW audio content of " << count << " ncm file";
        if (count != 1) {
            p_out << "s";
        }
        p_out << "...";
        return true;
    }
    if (p_index == CMD_CONVERT) {
        p_out << "Convert " << count << " ncm file";
        if (count != 1) {
            p_out << "s";
        }
        p_out << " to its original format (keep meta info)...";
        return true;
    }
    return base::context_get_display(p_index, p_data, p_out, p_displayflags, p_caller);
}
