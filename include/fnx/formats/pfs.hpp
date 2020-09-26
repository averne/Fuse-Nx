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
#include <memory>
#include <string>

#include <fnx/io.hpp>
#include <fnx/formats/base.hpp>

namespace fnx::hac {

class Pfs final: public FormatBase {
    public:
        constexpr static auto magic = utils::FourCC('P', 'F', 'S', '0');

        struct Entry {
            std::size_t offset, size;
            std::string_view name;
        };

    public:
        static inline bool match(const void *data, std::size_t size) {
            FNX_UNUSED(size);
            return reinterpret_cast<const Header *>(data)->magic == Pfs::magic;
        }

        Pfs() = default;
        Pfs(std::unique_ptr<io::FileBase> &&base);

        bool parse();

        bool is_valid() const {
            return this->header.magic == Pfs::magic;
        }

        std::uint32_t get_num_entries() const {
            return this->header.num_files;
        }

        const std::vector<Entry> &get_entries() const {
            return this->entries;
        }

        std::unique_ptr<io::OffsetFile> open(const Entry &entry) const;

        std::string_view get_name() const {
            return "Pfs";
        }

    protected:
        struct Header {
            std::uint32_t magic;
            std::uint32_t num_files;
            std::uint32_t string_table_size;
            std::uint32_t _res1;
        };
        FNX_ASSERT_SIZE(Header, 0x10);
        FNX_ASSERT_LAYOUT(Header);

        struct FileEntryMeta {
            std::uint64_t offset;
            std::uint64_t size;
            std::uint32_t name_offset;
        };
        FNX_ASSERT_SIZE(FileEntryMeta, 0x18);
        FNX_ASSERT_LAYOUT(FileEntryMeta);

    protected:
        Header      header;
        std::size_t strings_offset;
        std::size_t data_offset;

        std::vector<Entry> entries;
        std::vector<char> names_table;
};

} // namespace fnx::hac
