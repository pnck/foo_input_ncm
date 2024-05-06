#include "stdafx.h"
#include "context_menu.hpp"
#include "common/helpers.hpp"
#include "common/log.hpp"
#include "input_ncm.hpp"

#include <numeric>
#include <ranges>
#include <tuple>
#include <atomic>
#include <functional>
#include <thread>
#include <mutex>
#include <optional>

namespace fb2k_ncm::ui
{
    using cps_fn_t = std::optional<std::string>(ncm_file::ptr, threaded_process_status &, abort_callback &);
    /// @attention File dialog callback is being executed after the function returns.
    /// Everything should be properly moved into the callback function.
    void run_cmd_extract(metadb_handle_list_cref p_data, const GUID &p_caller, std::function<cps_fn_t> &&continuation = {}) {
        std::vector<ncm_file::ptr> ncm_files;
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

        const auto total = ncm_files.size();

        auto file_dialog = fb2k::fileDialog::get()->setupOpenFolder();
        file_dialog->setTitle(PFC_string_formatter() << "Save " << ncm_files.size() << " audio file(s) to...");
        file_dialog->runSimple(
            [total, cps_params = std::make_tuple(std::move(continuation), std::move(ncm_files))](fb2k::stringRef selected_path) {
                // file dialog callback

                auto _process = [total, cps_params = std::move(cps_params),
                                 path = std::string(selected_path->c_str())](threaded_process_status &p_status, abort_callback &p_abort) {
                    // NOTE: Since both p_status and p_abort are for message notifying, they are supposed to be thread-safe.
                    auto [continuation, ncm_files] = cps_params;
                    std::atomic_bool all_done = true;
                    std::atomic_uint32_t exclusive_index = 0;
                    std::atomic_uint32_t finished_count = 0;

                    std::mutex m_succ, m_fail;
                    std::vector<std::string> succs, fails;

                    auto worker = [&](threaded_process_status &p_status, abort_callback &p_abort) {
                        DEBUG_LOG("Extraction worker thread started.");
                        try {
                            for (auto index = exclusive_index++; index < total; index = exclusive_index++) {
                                p_abort.check();
                                auto &f = ncm_files[index];
                                if (auto ok = f->save_raw_audio(path.c_str(), p_abort); !ok) {
                                    bool expect_true = true;
                                    all_done.compare_exchange_strong(expect_true, false);
                                    std::lock_guard lock(m_fail);
                                    fails.emplace_back(f->path());
                                } else {
                                    // invoke CPS function
                                    if (continuation) {
                                        if (auto result = continuation(f, p_status, p_abort); !result.has_value()) {
                                            bool expect_true = true;
                                            all_done.compare_exchange_strong(expect_true, false);
                                            std::lock_guard lock(m_fail);
                                            fails.emplace_back(f->path());
                                        } else {
                                            std::lock_guard lock(m_succ);
                                            succs.emplace_back(*result); // NOTE: output dir
                                        }
                                    } else {
                                        std::lock_guard lock(m_succ);
                                        succs.emplace_back(f->saved_raw_path()); // NOTE: output dir
                                    }
                                }

                                p_status.set_progress(++finished_count, total);
                                p_status.set_title(PFC_string_formatter()
                                                   << "Extracting audio files (" << finished_count << " of " << total << ")");
                            }
                            DEBUG_LOG("Extraction worker thread ended normally.");
                        } catch (const exception_aborted &) {
                            DEBUG_LOG("Extraction worker aborted (p_abort)");
                            all_done = false;
                        } catch (const std::exception &e) {
                            ERROR_LOG("Extraction worker failed: ", e.what());
                            all_done = false;
                        }
                    };

                    // choose the greater one
                    size_t thread_count = fb2k_ncm::max_thread_count > std::thread::hardware_concurrency()
                                              ? fb2k_ncm::max_thread_count
                                              : std::thread::hardware_concurrency();
                    thread_count = thread_count > total ? total : thread_count;
                    DEBUG_LOG("Extraction main thread: scheduling ", thread_count, " threads.");

                    std::vector<std::thread> threads;
                    // slicing is still extreamly complicated! - rather not to imitate the [a:b:c] syntax :(
                    for ([[maybe_unused]] auto _ : std::views::iota(0) | std::views::take(thread_count)) {
                        threads.emplace_back(worker, std::ref(p_status), std::ref(p_abort));
                    }
                    for (auto &t : threads) {
                        t.join();
                    }
                    DEBUG_LOG("Extraction main thread: all workers finished.");
                    pfc::string8 msg;
                    if (all_done) {
                        msg << "All audio content have been extracted successfully.\n\n";
                        msg << succs.size() << " Extracted:\n";
                        for (auto &s : succs) {
                            msg << "  - " << s.c_str() << "\n";
                        }
                        popup_message::g_show(msg, "Extraction Completed", popup_message ::icon_information);
                    } else {
                        msg << "Extraction finished but some operations were failed.\n\n";
                        msg << succs.size() << " Extracted:\n";
                        for (auto &s : succs) {
                            msg << "  - " << s.c_str() << "\n";
                        }
                        msg << "\n" << fails.size() << " Failed:\n";
                        for (auto &s : fails) {
                            msg << "  - " << s.c_str() << "\n";
                        }
                        popup_message::g_show(msg, "Extraction Failed", popup_message::icon_error);
                    }
                }; // end of async logic

                auto process = threaded_process_callback_lambda::create(_process);
                uint32_t progress_style = threaded_process::flag_show_abort;
                progress_style |= threaded_process::flag_show_progress;
                progress_style |= threaded_process::flag_show_delayed;
                threaded_process::g_run_modeless(process, progress_style, core_api::get_main_window(),
                                                 PFC_string_formatter() << "Extracting " << total << " audio files");
            }); // end of file dialog callback
    }
    void run_cmd_extract_retag(metadb_handle_list_cref p_data, const GUID &p_caller) {
        run_cmd_extract(
            p_data, p_caller,
            [](ncm_file::ptr ncm_file, threaded_process_status &p_status, abort_callback &p_abort) -> std::optional<std::string> {
                DEBUG_LOG("Retagging ", ncm_file->saved_raw_path().data());

                if (p_abort.is_aborting()) {
                    DEBUG_LOG("Retagging aborted (p_abort)");
                    return {};
                }
                // p_status.set_title(PFC_string_formatter() << "Retagging (" << finished_count << " of " << total << ")");

                auto input = input_entry::g_find_by_guid(input_ncm::class_guid);
                service_ptr_t<input_info_reader> info_reader;
                input->open_for_info_read(info_reader, ncm_file, "", p_abort);
                file_info_impl info;
                info_reader->get_info(0, info, p_abort);

                // try to correct file extension if possible
                pfc::string codec;
                info.info_get_codec_long(codec);
                if (codec.toLower().contains("mp3")) {
                    codec = "mp3";
                } else if (codec.toLower().contains("flac")) {
                    codec = "flac";
                }

                const char *o_path = ncm_file->saved_raw_path().data();
                auto to_retag = pfc::string_directory(o_path);
                to_retag.add_filename(pfc::string_filename(o_path));
                to_retag << "." << codec;
                if (to_retag.empty()) {
                    DEBUG_LOG("Not extracted:", ncm_file->path());
                    return {};
                }

                try {
                    if (to_retag != ncm_file->saved_raw_path().data()) {
                        filesystem::get(to_retag)->move_overwrite(ncm_file->saved_raw_path().data(), to_retag, p_abort);
                    }

                    service_ptr_t<input_info_writer> retagger;
                    input_entry::g_open_for_info_write(retagger, file_ptr(), to_retag, p_abort);
                    retagger->set_info(0, info, p_abort);
                    retagger->commit(fb2k::noAbort); // do not interrupt committing
                    retagger = nullptr;              // release, to let album editor reopen the file

                    // embeded album art
                    auto album_extractor = album_art_extractor::g_open(ncm_file, ncm_file->path(), p_abort);
                    auto album_writer = album_art_editor::g_open(nullptr, to_retag, p_abort);
                    if (album_extractor.is_valid() && album_writer.is_valid()) {
                        if (auto data = album_extractor->query(album_art_ids::cover_front, p_abort); data.is_valid()) {
                            album_writer->set(album_art_ids::cover_front, data, p_abort);
                            album_writer->commit(fb2k::noAbort); // do not interrupt committing
                        }
                    }
                    DEBUG_LOG("Retagged ", to_retag);
                    return {std::string(to_retag.c_str())};
                } catch (const pfc::exception &e) {
                    ERROR_LOG("(", e.what(), ") Failed to retag ", to_retag);
                }
                return {};
            });
    }
} // namespace fb2k_ncm::ui

using namespace fb2k_ncm::ui;

static auto g_menu_factory = contextmenu_item_factory_t<context_menu>();
static auto g_menu = contextmenu_group_popup_factory(context_menu::class_guid, contextmenu_groups::root, "NCM Loader", 0);

constexpr context_menu::maybe_item_t context_menu::get_menu_item_(uint32_t index) {
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
    return base_t::context_get_display(p_index, p_data, p_out, p_displayflags, p_caller);
}
