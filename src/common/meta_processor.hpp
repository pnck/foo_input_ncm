#pragma once
#include "consts.hpp"
#include "foobar2000/SDK/file_info.h"
#include "nlohmann/json.hpp"

#include <type_traits>
#include <concepts>
#include <string_view>

// small helper macro to make `if` statement breakable
// maybe so strange why it works, but it just does.
#define b_if(cond)                           \
    switch (1 & static_cast<uint32_t>(cond)) \
    case 1:

namespace fb2k_ncm
{
    using json = nlohmann::json;
    namespace
    {
        template <typename T>
        concept optionalT = requires(T t) { t = std::nullopt; }; // can be assigned to std::nullopt

        template <optionalT T>
        consteval static bool is_single() {
            using ET = std::remove_cvref_t<decltype(*std::begin(*std::declval<T>() /*begin(field)*/) /*field[0]*/)>;
            if constexpr (std::integral<ET>) {
                return true;
            } else {
                return false;
            }
        }
        template <optionalT T>
        consteval static bool is_multi() {
            using CT = std::remove_cvref_t<decltype(*std::declval<T>())>;
            if constexpr (requires { CT{}.~unordered_set(); }) {
                return true;
            } else {
                return false;
            }
        }

        template <typename T>
        concept singleT = is_single<std::remove_cvref_t<T>>();

        template <typename T>
        concept multiT = is_multi<std::remove_cvref_t<T>>();

        consteval void traits_tests() {
            static_assert(is_single<decltype(uniform_meta_st::album)>(), "check your template implementation");
            using artistT = decltype(uniform_meta_st::artist);
            static_assert(is_multi<decltype(uniform_meta_st::genre)>(), "check your template implementation");
            static_assert((!is_multi<artistT>()) && (!is_single<artistT>()), "check your template implementation");
        }
    } // namespace

    class meta : public uniform_meta_st {
    public:
        // TODO: update all fields
        void update(const file_info &info);      // FB2K
        void update(const nlohmann::json &json); // NCM
        explicit meta(const file_info &info) { update(info); }
        explicit meta(const nlohmann::json &json) { update(json); }

    private:
        void update_by_json(const nlohmann::json &json); // NCM

        static void update_v_single(singleT auto &field, auto &&val) {
            using hold_t = std::remove_cvref_t<decltype(*field)>;
            using val_t = std::remove_cvref_t<decltype(val)>;
            if constexpr (std::is_same_v<hold_t, std::string>) {
                field = std::make_optional(std::string(std::forward<val_t>(val)));
            } else if constexpr (std::is_same_v<hold_t, uint64_t>) {
                field = std::make_optional(val);
            }
        }
        static void update_v_multi(multiT auto &field, auto &&val) {
            using val_t = std::remove_cvref_t<decltype(val)>;
            if (!field.has_value()) {
                field.emplace({});
            }
            field->emplace(std::forward<val_t>(val));
        }
        static void update_v(auto &field, auto &&val) {
            using val_t = std::remove_cvref_t<decltype(val)>;
            if constexpr (singleT<decltype(val)>) {
                update_v_single(field, std::forward<val_t>(val));
            } else if constexpr (multiT<decltype(val)>) {
                update_v_multi(field, std::forward<val_t>(val));
            }
        }
    };

    void meta::update(const file_info &info) { // FB2K
        if (info.meta_exists("title")) {
            update_v(title, info.meta_get("title", 0));
        }
    }

    void meta::update(const nlohmann::json &json) { // NCM
        // TODO: Apply overwrite

        // TODO: use string_view literal operator
    }

    void meta::update_by_json(const nlohmann::json &json) { // NCM
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

} // namespace fb2k_ncm
