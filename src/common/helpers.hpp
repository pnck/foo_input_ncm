#pragma once

#include <type_traits>
#include <functional>
#include <string_view>

//! if we need some template magic, find here

// A static value-literal mapping, map any value to a string literal
namespace
{
    template <typename T, T /* value */>
    struct mapping_item {
        template <size_t L, typename CT>
        static constexpr auto to(const CT (&s)[L]) {
            static_assert(std::is_same_v<CT, char> || std::is_same_v<CT, wchar_t>);
            return s;
        }
    };

    template <auto V, template <auto> typename Impl>
    struct make_mapping_with_impl {
        constexpr static auto value = mapping_item<decltype(V), V>::to(Impl<V>::value);
    };

    struct decay_string : public std::string {
        using base_t = std::string;
        constexpr decay_string() : base_t() {}
        constexpr decay_string(base_t &&str) noexcept : base_t(std::move(str)) {}
        operator const char *() const noexcept { return this->data(); }
        operator std::string_view() const noexcept { return {this->data(), this->size()}; }
    };

    constexpr decay_string operator""_upper(const char *str, std::size_t len) {
        std::string result;
        result.resize(len);
        auto to_upper = [](char c) -> char { return (c >= 'a' && c <= 'z') ? c - ('a' - 'A') : c; };
        for (std::size_t i = 0; i < len; ++i) {
            result[i] = to_upper(str[i]);
        }
        return result;
    }
    /*
        constexpr decay_string upper(const std::string &s) {
            decay_string result;
            std::transform(
                s.begin(), s.end(), std::back_inserter(result), [](char c) -> char { return (c >= 'a' && c <= 'z') ? c - ('a' - 'A') : c;
       }); return result;
        }
    */
    constexpr auto upper(std::string_view v) {
        return operator""_upper(v.data(), v.size());
    }

} // namespace
