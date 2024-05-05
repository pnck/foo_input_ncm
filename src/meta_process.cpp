#include "stdafx.h"
#include "meta_process.hpp"
#include "common/helpers.hpp"

#include <functional>

using namespace fb2k_ncm;

// small helper macro to make `if` statement breakable
// maybe so strange why it works, but it just does.
#define b_if(cond)                           \
    switch (1 & static_cast<uint32_t>(cond)) \
    case 1:

void meta_processor::update(const file_info &info) { // FB2K
    using refl_f_t = void(t_size /*enum index*/);
    auto reflection = std::unordered_map<std::string_view, std::function<refl_f_t>>{}; // UPPERCASE keys
    reflection["Artist"_upper] = [&](t_size meta_entry) {
        auto vc = info.meta_enum_value_count(meta_entry);
        if (!vc) {
            return;
        }
        if (!artist.has_value()) {
            artist.emplace();
        }
        for (auto i = 0; i < vc; ++i) {
            artist->emplace(info.meta_enum_value(meta_entry, i), 0 /*artist id, default to 0*/);
        }
    };
#define reflect_single(field) [&](t_size meta_entry) { update_v(field, info.meta_enum_value(meta_entry, 0)); }
#define reflect_multi(field)                                      \
    [&](t_size meta_entry) {                                      \
        auto vc = info.meta_enum_value_count(meta_entry);         \
        for (auto i = 0; i < vc; ++i) {                           \
            update_v(field, info.meta_enum_value(meta_entry, i)); \
        }                                                         \
    }

    reflection["Title"_upper] = reflect_single(title);
    reflection["Album"_upper] = reflect_single(album);
    reflection["Date"_upper] = reflect_single(date);
    reflection["Genre"_upper] = reflect_multi(genre);
    reflection["Producer"_upper] = reflect_multi(producer);
    reflection["Composer"_upper] = reflect_multi(composer);
    reflection["Performer"_upper] = reflect_multi(performer);
    reflection["Album Artist"_upper] = reflect_multi(album_artist);
    reflection["TrackNumber"_upper] = reflect_single(track_number);
    reflection["TotalTracks"_upper] = reflect_single(total_tracks);
    reflection["DiscNumber"_upper] = reflect_single(disc_number);
    reflection["TotalDiscs"_upper] = reflect_single(total_discs);
    reflection["Comment"_upper] = reflect_single(comment);
    reflection["Lyrics"_upper] = reflect_single(lyrics);

    auto meta_count = info.meta_get_count();
    for (auto i = 0; i < meta_count; ++i) {
        auto name = upper(info.meta_enum_name(i));
        if (reflection.contains(name)) {
            std::invoke(reflection[name], i);
            continue;
        } else {
            auto vc = info.meta_enum_value_count(i);
            if (vc == 1) {
                extra_single_values[name] = info.meta_enum_value(i, 0);
            } else {
                for (auto j = 0; j < vc; ++j) {
                    extra_multi_values[name].emplace(info.meta_enum_value(i, j));
                }
            }
        }
    }
#undef reflect_single
#undef reflect_multi
}

void meta_processor::update(const nlohmann::json &json) { // NCM, public
    update_by_json(json);
    if (json.contains("overwrite")) {
        update_by_json(json["overwrite"]);
    }
}

void meta_processor::update_by_json(const nlohmann::json &json) { // NCM
    if (!json.is_object()) {
        return;
    }
    // TODO: adapt to dump() format
    b_if(json.contains("artist") && json["artist"].is_array()) {
        if (!artist.has_value()) {
            artist.emplace();
        }
        for (const auto &val : json["artist"].get_ref<const json::array_t &>()) {
            if (!val.is_array()) {
                continue;
            }
            if (val.size() != 2) {
                continue;
            }
            if (!val[0].is_string() || !val[1].is_number_integer()) {
                continue;
            }
            artist->emplace(val[0].get<std::string>(), val[1].get<uint64_t>());
        }
    }
#define update_number_checked(name, field)           \
    b_if(json.contains(name)) {                      \
        if (!json[name].is_number_integer()) {       \
            break;                                   \
        }                                            \
        update_v(field, json[name].get<uint64_t>()); \
    }

#define update_string_checked(name, field)              \
    b_if(json.contains(name)) {                         \
        if (!json[name].is_string()) {                  \
            break;                                      \
        }                                               \
        update_v(field, json[name].get<std::string>()); \
    }
    update_number_checked("musicId", musicId);
    update_string_checked("musicName", title);
    update_number_checked("albumId", albumId);
    update_string_checked("album", album);
    update_string_checked("albumPicDocId", albumPicDocId);
    update_string_checked("albumPic", albumPic);
    update_string_checked("mp3DocId", mp3DocId);
    update_number_checked("mvId", mvId);
    update_number_checked("bitrate", bitrate);
    update_number_checked("duration", duration);
    update_string_checked("format", format);

    b_if(json.contains("alias")) {
        if (!json["alias"].is_array()) {
            break;
        }

        for (const auto &val : json["alias"].get_ref<const json::array_t &>()) {
            if (!val.is_string()) {
                continue;
            }
            update_v(alias, val.get<std::string>());
        }
    }
    b_if(json.contains("transNames")) {
        if (!json["transNames"].is_array()) {
            break;
        }

        for (const auto &val : json["transNames"].get_ref<const json::array_t &>()) {
            if (!val.is_string()) {
                continue;
            }
            update_v(transNames, val.get<std::string>());
        }
    }

#undef update_number_checked
#undef update_string_checked
}

void meta_processor::apply(file_info &info) { // FB2K
    // TODO: use and_then() if c++23 is available
#define apply_single(name, field)            \
    if (field.has_value()) {                 \
        info.meta_set(name, field->c_str()); \
    }

#define apply_multi(name, field)              \
    if (field.has_value()) {                  \
        info.meta_remove_field(name);         \
        for (const auto &val : *field) {      \
            info.meta_add(name, val.c_str()); \
        }                                     \
    }

    if (artist.has_value()) {
        for (const auto &[name, id] : *artist) {
            info.meta_add("Artist"_upper, name.c_str());
        }
    }

    // NOTE: fb2k uses UPPERCASE tags for metainfo
    apply_single("Title"_upper, title);
    apply_single("Album"_upper, album);
    apply_single("Date"_upper, date);
    apply_multi("Genre"_upper, genre);
    apply_multi("Producer"_upper, producer);
    apply_multi("Composer"_upper, composer);
    apply_multi("Performer"_upper, performer);
    apply_multi("Album Artist"_upper, album_artist);
    apply_single("TrackNumber"_upper, track_number);
    apply_single("TotalTracks"_upper, total_tracks);
    apply_single("DiscNumber"_upper, disc_number);
    apply_single("TotalDiscs"_upper, total_discs);
    apply_single("Comment"_upper, comment);
    apply_single("Lyrics"_upper, lyrics);

    // NCM Meta
    apply_multi("alias", alias);
    apply_multi("transNames", transNames);

#undef apply_single
#undef apply_multi

    for (const auto &[name, val] : extra_single_values) {
        info.meta_set(name.c_str(), val.c_str());
    }

    for (const auto &[name, vals] : extra_multi_values) {
        info.meta_remove_field(name.c_str());
        for (const auto &val : vals) {
            info.meta_add(name.c_str(), val.c_str());
        }
    }

#define apply_num(name, field)                               \
    if (field.has_value()) {                                 \
        info.info_set(name, std::to_string(*field).c_str()); \
    }

#define apply_s(name, field)                 \
    if (field.has_value()) {                 \
        info.info_set(name, field->c_str()); \
    }

    // info fields are immutable
    apply_num("musicId", musicId);
    apply_num("albumId", albumId);
    apply_num("mvId", mvId);
    // apply_num("bitrate", bitrate);
    apply_num("duration", duration);
    apply_s("albumPicDocId", albumPicDocId);
    apply_s("albumPic", albumPic);
    apply_s("mp3DocId", mp3DocId);
    apply_s("format", format);

#undef apply_num
#undef apply_s
}

json meta_processor::dump() { // NCM
    return {};                // TODO: dump
}
