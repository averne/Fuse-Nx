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

#include <cstring>
#include <algorithm>

#include <fnx/keyset.hpp>

namespace fnx::crypt {

namespace {

constexpr bool operator ==(const std::string_view &lhs, const std::string_view &rhs) {
    if (lhs.empty() || rhs.empty())
        return false;
    return std::equal(lhs.begin(), lhs.end(), rhs.begin(),
        [](char l, char r) { return std::tolower(l) == std::tolower(r); });
}

constexpr bool is_hex(const std::string_view &str) {
    return std::all_of(str.begin(), str.end(), [](char c) {
        return (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') || (c >= '0' && c <= '9');
    });
}

constexpr std::uint8_t htoi(char c) {
    if (c >= 'a' && c <= 'f')
        return (c - 'a') + 0xa;
    else if (c >= 'A' && c <= 'F')
        return (c - 'A') + 0xa;
    else if (c >= '0' && c <= '9')
        return c - '0';
    return 0;
};

template <typename T>
constexpr T to_hex_array(const std::string_view &str) {
    T arr{};
    for (std::size_t i = 0; i < std::min(arr.size(), str.size() / 2); ++i)
        arr[i] = (htoi(str[i * 2]) << 4) | (htoi(str[i * 2 + 1]) << 0);
    return arr;
}

} // namespace

void KeySet::set_key(const std::string_view &id, const std::string_view &value) {
    if (!is_hex(value)) {
        std::fprintf(stderr, "Key is not hexadecimal: %s %s\n", id.data(), value.data());
        return;
    }

    std::size_t mkey_len = std::strlen("master_key_");
    std::size_t tkek_len = std::strlen("titlekek_");

    if (id == "aes_kek_generation_source")
        this->aes_kek_generation_source       = to_hex_array<AesKey>(value);
    else if (id == "aes_key_generation_source")
        this->aes_key_generation_source       = to_hex_array<AesKey>(value);
    else if (id == "key_area_key_application_source")
        this->key_area_key_application_source = to_hex_array<AesKey>(value);
    else if (id == "key_area_key_ocean_source")
        this->key_area_key_ocean_source       = to_hex_array<AesKey>(value);
    else if (id == "key_area_key_system_source")
        this->key_area_key_system_source      = to_hex_array<AesKey>(value);
    else if (id == "header_key")
        this->header_key                      = to_hex_array<AesXtsKey>(value);
    else if ("master_key_" == id && is_hex(id.substr(mkey_len))) {
        auto gen = (htoi(id.at(mkey_len)) << 4) | (htoi(id.at(mkey_len + 1)) << 0);
        this->master_keys[gen]                = to_hex_array<AesKey>(value);
    } else if ("titlekek_" == id && is_hex(id.substr(tkek_len))) {
        auto gen = (htoi(id.at(tkek_len)) << 4) | (htoi(id.at(tkek_len + 1)) << 0);
        this->titlekeks[gen]                  = to_hex_array<AesKey>(value);
    }
}

const AesKey &KeySet::get_kaek(std::size_t idx) const {
    switch (idx) {
        default:
        case 0:
            return this->key_area_key_application_source;
        case 1:
            return this->key_area_key_ocean_source;
        case 2:
            return this->key_area_key_system_source;
    }
}

void TitlekeySet::set_cli_key(const std::string_view &key) {
    this->set_cli_key(to_hex_array<AesKey>(key));
}

void TitlekeySet::set_key(const std::string_view &id, const std::string_view &value) {
    this->set_key(to_hex_array<RightsId>(id), to_hex_array<AesKey>(value));
}

} // namespace fnx::crypt
