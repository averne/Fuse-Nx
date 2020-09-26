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
#include <array>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <fnx/types.hpp>

namespace fnx::crypt {

enum class KeySetType {
    Prod,
    Dev,
    Title,
};

using AesKey    = std::array<std::uint8_t, 0x10>;
using AesXtsKey = std::array<std::uint8_t, 0x20>;

struct KeySet {
    std::array<AesKey, 0x10> master_keys;
    std::array<AesKey, 0x10> titlekeks;

    AesXtsKey header_key;

    AesKey aes_kek_generation_source,
        aes_key_generation_source;

    AesKey key_area_key_application_source,
        key_area_key_ocean_source,
        key_area_key_system_source;

    void set_key(const std::string_view &id, const std::string_view &value);

    const AesKey &get_kaek(std::size_t idx) const;

    static KeySet *get() {
        return KeySet::g_keyset.get();
    }

    static void set(std::unique_ptr<KeySet> &&set) {
        KeySet::g_keyset = std::move(set);
    }

    private:
        static inline std::unique_ptr<KeySet> g_keyset;
};

struct TitlekeySet {
    void set_cli_key(const std::string_view &key);
    void set_cli_key(const AesKey &key) {
        this->cli_key = std::make_unique<AesKey>(key);
    }

    void remove_cli_key() {
        this->cli_key.reset();
    }

    void set_key(const std::string_view &id, const std::string_view &value);
    void set_key(const RightsId &id, const AesKey &key) {
        this->map.insert_or_assign(id, key);
    }

    const AesKey &get_key(const RightsId &id) const {
        if (this->cli_key)
            return *this->cli_key;
        return this->map.at(id);
    }

    static TitlekeySet *get() {
        return TitlekeySet::g_keyset.get();
    }

    static void set(std::unique_ptr<TitlekeySet> &&set) {
        TitlekeySet::g_keyset = std::move(set);
    }

    private:
        struct RightsIdHash {
            std::size_t operator ()(const RightsId &arr) const {
                auto *tmp = reinterpret_cast<const std::uint64_t *>(arr.data());
                return tmp[0] ^ tmp[1];
            }
        };

        std::unique_ptr<AesKey> cli_key;
        std::unordered_map<RightsId, AesKey, RightsIdHash> map;

        static inline std::unique_ptr<TitlekeySet> g_keyset;
};

} // namespace fnx::crypt
