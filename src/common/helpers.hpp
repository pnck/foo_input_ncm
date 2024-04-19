#pragma once

#include <type_traits>
#include <functional>

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
} // namespace
