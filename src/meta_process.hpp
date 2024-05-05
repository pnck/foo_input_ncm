#pragma once
#include "common/consts.hpp"
#include "foobar2000/SDK/file_info.h"
#include "nlohmann/json.hpp"

#include "common/log.hpp"

#include <type_traits>
#include <concepts>

namespace fb2k_ncm
{
    using json = nlohmann::json;
    namespace
    {
        template <typename T>
        concept optionalT = requires(std::remove_cvref_t<T> t) { t = std::nullopt; }; // can be assigned to std::nullopt

        template <optionalT T>
        consteval static bool opt_is_single() {
            using CT = std::remove_cvref_t<decltype(*std::declval<T>())>;
            constexpr bool is_char_seq = requires {
                {
                    CT{}.data()[0] /* NOTE: map::operator[] accepts pointer, so can't be distinguished by [0]*/
                } -> std::convertible_to<char>;
            };
            if constexpr (std::convertible_to<CT, uint64_t> || is_char_seq) {
                return true;
            } else {
                return false;
            }
        }

        template <optionalT T>
        consteval static bool opt_is_multi() {
            using CT = std::remove_cvref_t<decltype(*std::declval<T>())>;
            if constexpr (requires { CT{}.~unordered_set(); }) {
                return true;
            } else {
                return false;
            }
        }

        template <typename T>
        concept singleT = opt_is_single<T>();

        template <typename T>
        concept multiT = opt_is_multi<T>();

        consteval void traits_tests() {
            static_assert(singleT<decltype(uniform_meta_st{}.album)>, "check your template implementation");
            static_assert(multiT<decltype(uniform_meta_st{}.genre)>, "check your template implementation");
            using artistT = decltype(uniform_meta_st{}.artist);
            static_assert(!singleT<artistT> && !multiT<artistT>, "check your template implementation");
        }
    } // namespace

    class meta_processor : public uniform_meta_st {
    public:
        // TODO: update all fields
        void update(const file_info &info);      // FB2K
        void update(const nlohmann::json &json); // NCM
        void apply(file_info &info);             // FB2K
        nlohmann::json dump();                   // NCM
        explicit meta_processor(const file_info &info) { update(info); }
        explicit meta_processor(const nlohmann::json &json) { update(json); }

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
                field.emplace();
            }
            field->emplace(std::forward<val_t>(val));
        }
        static void update_v(auto &field, auto &&val) {
            using val_t = std::remove_cvref_t<decltype(val)>;
            if constexpr (singleT<decltype(field)>) {
                update_v_single(field, std::forward<val_t>(val));
            } else if constexpr (multiT<decltype(field)>) {
                update_v_multi(field, std::forward<val_t>(val));
            } else {
                uBugCheck();
            }
        }
    };

} // namespace fb2k_ncm
