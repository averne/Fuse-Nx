// Copyright (C) 2020 averne
//
// This file is part of Fuse-Nx.
//
// Fuse-Nx is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Fuse-Nx is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Fuse-Nx.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <cstdint>
#include <algorithm>
#include <concepts>
#include <type_traits>
#include <utility>

#define _FNX_CAT(x, y) x ## y
#define  FNX_CAT(x, y) _FNX_CAT(x, y)
#define _FNX_STR(x) #x
#define  FNX_STR(x) _FNX_STR(x)

#define FNX_ANONYMOUS FNX_CAT(var, __COUNTER__)

#define FNX_SCOPEGUARD(f) auto FNX_ANONYMOUS = ::fnx::utils::ScopeGuard(f)

#define FNX_UNUSED(...) ::fnx::utils::variadic_unused(__VA_ARGS__)

#define FNX_ASSERT_SIZE(c, sz) static_assert(sizeof(c) == sz)
#define FNX_ASSERT_LAYOUT(c) static_assert(std::is_standard_layout_v<c>)

namespace fnx::utils {

template <typename ...Args>
__attribute__((always_inline))
static inline void variadic_unused(Args &&...args) {
    (static_cast<void>(args), ...);
}

template <typename T>
constexpr static T align_down(T val, auto align) {
    return val & ~(align - 1);
}

template <typename T>
constexpr static T align_up(T val, auto align) {
    return (val + align - 1) & ~(align - 1);
}

struct FourCC {
    consteval FourCC(char a, char b, char c, char d) {
        this->code = (d << 0x18) | (c << 0x10) | (b << 0x8) | a;
    }

    constexpr operator std::uint32_t() const {
        return this->code;
    }

    private:
        std::uint32_t code = 0;
};

template <typename F>
struct ScopeGuard {
    [[nodiscard]] ScopeGuard(F &&f): f(std::move(f)) { }

    ScopeGuard(const ScopeGuard &) = delete;
    ScopeGuard &operator =(const ScopeGuard &) = delete;

    ~ScopeGuard() {
        this->f();
    }

    private:
        F f;
};

template <typename C>
concept Container = requires (C c) {
    typename C::value_type;
    { c.data() } -> std::same_as<std::add_pointer_t<typename C::value_type>>;
    { c.size() } -> std::convertible_to<std::size_t>;
};

constexpr static bool is_nonzero(const Container auto &arr) {
    return std::any_of(arr.begin(), arr.end(), [](auto e) -> bool { return e; });
}

} // namespace fnx::utils
