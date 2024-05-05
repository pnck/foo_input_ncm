#include "stdafx.h"
#include "meta_process.hpp"
#include "common/helpers.hpp"

#include <functional>

using namespace fb2k_ncm;
using json_t = nlohmann::json;

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
    // TODO: adapt to dump() format

    if (!json.is_object()) {
        return;
    }

    using refl_f_t = void(const nlohmann::json &);
    auto reflection = std::unordered_map<std::string_view, std::function<refl_f_t>>{}; // UPPERCASE keys
    reflection["artist"] = [&](const json_t &j) {
        if (!artist.has_value()) {
            artist.emplace();
        }
        for (const auto &val : j.get_ref<const json_t::array_t &>()) {
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
    };

#define reflect_single(field, TYPE) [&](const json_t j) { update_v(field, j.get<TYPE>()); };
#define reflect_multi_string(field)                                    \
    [&](const json_t j) {                                              \
        if (!j.is_array()) {                                           \
            return;                                                    \
        }                                                              \
        for (const auto &val : j.get_ref<const json_t::array_t &>()) { \
            if (!val.is_string()) {                                    \
                continue;                                              \
            }                                                          \
            update_v(field, val.get<std::string>());                   \
        }                                                              \
    }

    // NCM fields
    reflection["musicId"] = reflect_single(musicId, uint64_t);
    reflection["musicName"] = reflect_single(title, std::string);
    reflection["albumId"] = reflect_single(albumId, uint64_t);
    reflection["album"] = reflect_single(album, std::string);
    reflection["albumPicDocId"] = reflect_single(albumPicDocId, std::string);
    reflection["albumPic"] = reflect_single(albumPic, std::string);
    reflection["mp3DocId"] = reflect_single(mp3DocId, std::string);
    reflection["mvId"] = reflect_single(mvId, uint64_t);
    reflection["bitrate"] = reflect_single(bitrate, uint64_t);
    reflection["duration"] = reflect_single(duration, uint64_t);
    reflection["format"] = reflect_single(format, std::string);
    reflection["alias"] = reflect_multi_string(alias);
    reflection["transNames"] = reflect_multi_string(transNames);

    // FB2K fields, UPPERCASE
    reflection["title"_upper] = reflect_single(title, std::string); // maybe overwrite
    reflection["album"_upper] = reflect_single(album, std::string); // maybe overwrite
    reflection["date"_upper] = reflect_single(date, std::string);
    reflection["genre"_upper] = reflect_multi_string(genre);
    reflection["producer"_upper] = reflect_multi_string(producer);
    reflection["composer"_upper] = reflect_multi_string(composer);
    reflection["performer"_upper] = reflect_multi_string(performer);
    reflection["album artist"_upper] = reflect_multi_string(album_artist);
    reflection["TrackNumber"_upper] = reflect_single(track_number, std::string);
    reflection["TotalTracks"_upper] = reflect_single(total_tracks, std::string);
    reflection["DiscNumber"_upper] = reflect_single(disc_number, std::string);
    reflection["TotalDiscs"_upper] = reflect_single(total_discs, std::string);
    reflection["Comment"_upper] = reflect_single(comment, std::string);
    reflection["Lyrics"_upper] = reflect_single(lyrics, std::string);

    // ignore comment key
    reflection[foo_input_ncm_comment_key] = [&](const json_t &) { /* DO NOTHING*/ };

    for (const auto &[key, val] : json.items()) {
        if (reflection.contains(key)) {
            std::invoke(reflection[key], val);
        } else {
            if (val.is_string()) {
                extra_single_values[key] = val.get<std::string>();
            } else if (val.is_number_integer()) {
                extra_single_values[key] = std::to_string(val.get<uint64_t>());
            } else if (val.is_number_float()) {
                extra_single_values[key] = std::to_string(val.get<double>());
            } else if (val.is_array()) {
                for (const auto &v : val.get_ref<const json_t::array_t &>()) {
                    if (v.is_string()) {
                        extra_multi_values[key].emplace(v.get<std::string>());
                    }
                }
            }
        }
    }

#undef reflect_single
#undef reflect_multi_string
}

void meta_processor::apply(file_info &info) { // FB2K
    // TODO: use and_then() if c++23 is available
#define apply_meta_single(name, field)       \
    if (field.has_value()) {                 \
        info.meta_set(name, field->c_str()); \
    }

#define apply_meta_multi(name, field)         \
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
    apply_meta_single("title"_upper, title);
    apply_meta_single("album"_upper, album);
    apply_meta_single("date"_upper, date);
    apply_meta_multi("genre"_upper, genre);
    apply_meta_multi("producer"_upper, producer);
    apply_meta_multi("composer"_upper, composer);
    apply_meta_multi("performer"_upper, performer);
    apply_meta_multi("album artist"_upper, album_artist);
    apply_meta_single("TrackNumber"_upper, track_number);
    apply_meta_single("TotalTracks"_upper, total_tracks);
    apply_meta_single("DiscNumber"_upper, disc_number);
    apply_meta_single("TotalDiscs"_upper, total_discs);
    apply_meta_single("comment"_upper, comment);
    apply_meta_single("lyrics"_upper, lyrics);

    // NCM Meta
    apply_meta_multi("alias", alias);
    apply_meta_multi("transNames", transNames);

#undef apply_meta_single
#undef apply_meta_multi

    for (const auto &[name, val] : extra_single_values) {
        info.meta_set(name.c_str(), val.c_str());
    }

    for (const auto &[name, vals] : extra_multi_values) {
        info.meta_remove_field(name.c_str());
        for (const auto &val : vals) {
            info.meta_add(name.c_str(), val.c_str());
        }
    }

#define apply_info_num(name, field)                          \
    if (field.has_value()) {                                 \
        info.info_set(name, std::to_string(*field).c_str()); \
    }

#define apply_info_s(name, field)            \
    if (field.has_value()) {                 \
        info.info_set(name, field->c_str()); \
    }

    // info fields are immutable
    apply_info_num("musicId", musicId);
    apply_info_num("albumId", albumId);
    apply_info_num("mvId", mvId);
    // apply_info_num("bitrate", bitrate);
    apply_info_num("duration", duration);
    apply_info_s("albumPicDocId", albumPicDocId);
    apply_info_s("albumPic", albumPic);
    apply_info_s("mp3DocId", mp3DocId);
    apply_info_s("format", format);

#undef apply_info_num
#undef apply_info_s
}

/// @attention adapt to NCM json format
json_t meta_processor::dump() { // NCM
    return {};                  // TODO: dump
}
