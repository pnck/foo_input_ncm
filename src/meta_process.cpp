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
    reflection["artist"_upper] = [&](t_size meta_entry) {
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

    // fb2k
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

    // ncm
    reflection["alias"_upper] = reflect_multi(alias);
    reflection["transNames"_upper] = reflect_multi(transNames);

    // ignore essential special keys
    reflection[foo_input_ncm_comment_key] = [](auto...) {};
    reflection[upper(foo_input_ncm_comment_key)] = [](auto...) {};
    reflection[overwrite_key] = [](auto...) {};
    reflection[upper(overwrite_key)] = [](auto...) {};

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
        update_by_json(json["overwrite"], true);
    }
}

void meta_processor::update_by_json(const nlohmann::json &json, bool overwriting) { // NCM

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

#define reflect_single(field, TYPE) [&](const json_t &j) { update_v(field, j.get<TYPE>()); };
#define reflect_multi_string(field)                                    \
    [&](const json_t &j) {                                             \
        if (!j.is_array()) {                                           \
            return;                                                    \
        }                                                              \
        if (overwriting && field.has_value()) {                        \
            field.reset();                                             \
            field.emplace();                                           \
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
    reflection[foo_input_ncm_comment_key] = [](auto...) { /* DO NOTHING*/ };
    // ignore overwrite currently
    reflection[overwrite_key] = [](auto...) {};

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
    // NOTE: use and_then() if c++23 is available

    if (artist.has_value()) {
        for (const auto &[name, id] : *artist) {
            info.meta_add("Artist"_upper, name.c_str());
        }
    }

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

    // NOTE: fb2k uses UPPERCASE tags for metainfo
    apply_meta_single("title"_upper, title);
    apply_meta_single("album"_upper, album);
    apply_meta_single("date"_upper, date);
    apply_meta_multi("genre"_upper, genre);
    apply_meta_multi("producer"_upper, producer);
    apply_meta_multi("composer"_upper, composer);
    apply_meta_multi("performer"_upper, performer);
    apply_meta_multi("Album Artist"_upper, album_artist);
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

#define apply_info_num(field)                                  \
    if (field.has_value()) {                                   \
        info.info_set(#field, std::to_string(*field).c_str()); \
    }

#define apply_info_s(field)                    \
    if (field.has_value()) {                   \
        info.info_set(#field, field->c_str()); \
    }

    // info fields are immutable
    apply_info_num(musicId);
    apply_info_num(albumId);
    apply_info_num(mvId);
    // apply_info_num("bitrate", bitrate);
    // apply_info_num("duration", duration);
    apply_info_s(albumPicDocId);
    apply_info_s(albumPic);
    apply_info_s(mp3DocId);
    apply_info_s(format);

#undef apply_info_num
#undef apply_info_s
}

/// @attention adapt to NCM json format
json_t meta_processor::dump() { // NCM
    json_t json;
    if (artist.has_value()) {
        json["artist"] = json_t::array();
        for (const auto &[name, id] : *artist) {
            json["artist"].emplace_back(json_t::array({name, id}));
        }
    }
#define dump_single2(name, field) \
    if (field.has_value()) {      \
        json[name] = *field;      \
    }
#define dump_single(field) dump_single2(#field, field)
#define dump_single_u(field) dump_single2(#field##_upper, field)

#define dump_multi2(name, field)          \
    if (field.has_value()) {              \
        json[name] = json_t::array();     \
        for (const auto &val : *field) {  \
            json[name].emplace_back(val); \
        }                                 \
    }
#define dump_multi(field) dump_multi2(#field, field)
#define dump_multi_u(field) dump_multi2(#field##_upper, field)

    // fb2k
    dump_single_u(title);
    // dump_single_u(album);
    dump_single_u(date);
    dump_multi_u(genre);
    dump_multi_u(producer);
    dump_multi_u(composer);
    dump_multi_u(performer);
    dump_multi2("Album Artist"_upper, album_artist);
    dump_single2("TrackNumber"_upper, track_number);
    dump_single2("TotalTracks"_upper, total_tracks);
    dump_single2("DiscNumber"_upper, disc_number);
    dump_single2("TotalDiscs"_upper, total_discs);
    dump_single(comment);
    dump_single(lyrics);

    // ncm
    dump_single2("musicName", title);
    dump_single(album);
    dump_single(musicId);
    dump_single(albumId);
    dump_single(albumPicDocId);
    dump_single(albumPic);
    dump_single(mp3DocId);
    dump_single(mvId);
    // dump_single(bitrate);
    // dump_single(duration);
    dump_single(format);
    dump_multi(alias);
    dump_multi(transNames);

    // extra
    for (const auto &[name, val] : extra_single_values) {
        json[name] = val;
    }

    for (const auto &[name, vals] : extra_multi_values) {
        json[name] = json_t::array();
        for (const auto &val : vals) {
            json[name].emplace_back(val);
        }
    }
    return json;

#undef dump_single
#undef dump_single2
#undef dump_multi
#undef dump_multi2
}
