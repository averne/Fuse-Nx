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
#include <fnx/utils.hpp>
#include <fnx/formats/base.hpp>

namespace fnx::hac {

class RomFs final: public FormatBase {
    public:
        constexpr static auto magic = 0x50u; // Size of the header, always the same

        struct FileEntry;
        struct DirEntry;

        struct Entry {
            DirEntry *parent = nullptr;
            void *meta       = nullptr;
            std::string_view name;

            constexpr Entry() = default;
            constexpr Entry(const std::string_view &name): name(name) { }
            constexpr Entry(DirEntry *parent, void *meta, const std::string_view &name):
                parent(parent), meta(meta), name(name) { }

            std::string path() const;
        };

        struct DirEntry: public Entry {
            std::vector<DirEntry  *> children;
            std::vector<FileEntry *> files;

            DirEntry() = default;
            DirEntry(const std::string_view &name): Entry(name) { }
            DirEntry(DirEntry *parent, void *meta, const std::string_view &name):
                Entry(parent, meta, name) { }
        };

        struct FileEntry: public Entry {
            std::size_t offset, size;

            FileEntry() = default;
            FileEntry(const std::string_view &name, std::size_t offset, std::size_t size):
                Entry(name), offset(offset), size(size) { }
            FileEntry(DirEntry *parent, void *meta, const std::string_view &name, std::size_t offset, std::size_t size):
                Entry(parent, meta, name), offset(offset), size(size) { }
        };

    public:
        static inline bool match(const void *data, std::size_t size) {
            FNX_UNUSED(size);
            return reinterpret_cast<const Header *>(data)->header_size == RomFs::magic;
        }

        RomFs() = default;
        RomFs(std::unique_ptr<io::FileBase> &&base);

        // Fast path for parsing the whole rom
        // Will not populate parent/meta fields and children/files lists in directories
        bool parse_fast();

        inline bool parse() {
            return this->parse_dir(nullptr, false);
        }

        inline bool parse_full() {
            return this->parse_dir(nullptr, true);
        }

        // Parse the rom starting from a given directory
        // nullptr is equivalent to root
        bool parse_dir(DirEntry *entry, bool recursive = false);

        bool is_valid() const {
            return this->header.header_size == RomFs::magic;
        }

        // Finds entries using the hash tables
        DirEntry  *find_dir(const std::string_view &path);
        FileEntry *find_file(const std::string_view &path);

        std::size_t get_dir_nb() const {
            return this->dir_entries.size();
        }

        std::size_t get_file_nb() const {
            return this->file_entries.size();
        }

        const std::vector<std::unique_ptr<DirEntry>> &get_dir_entries() const {
            return this->dir_entries;
        }

        const std::vector<std::unique_ptr<FileEntry>> &get_file_entries() const {
            return this->file_entries;
        }

        const std::unique_ptr<DirEntry> &get_root() const {
            return this->dir_entries[0];
        }

        std::unique_ptr<io::OffsetFile> open(const FileEntry &entry) const;

        std::string_view get_name() const {
            return "RomFs";
        }

    protected:
        struct Header {
            std::uint64_t header_size; // Always 0x50 (on the Switch)
            std::uint64_t dir_tbl_off,   dir_tbl_sz;
            std::uint64_t dir_meta_off,  dir_meta_sz;
            std::uint64_t file_tbl_off,  file_tbl_sz;
            std::uint64_t file_meta_off, file_meta_sz;
            std::uint64_t file_dat_off;
        };
        FNX_ASSERT_SIZE(Header, 0x50);
        FNX_ASSERT_LAYOUT(Header);

        using Hash = std::uint32_t;

        struct DirEntryMeta {
            std::uint32_t parent_off;
            std::uint32_t sibling_off;
            std::uint32_t child_off;
            std::uint32_t file_off;
            std::uint32_t next;
            std::uint32_t name_len;
            char name[];
        };
        FNX_ASSERT_SIZE(DirEntryMeta, 0x18);
        FNX_ASSERT_LAYOUT(DirEntryMeta);

        struct FileEntryMeta {
            std::uint32_t parent_off;
            std::uint32_t sibling_off;
            std::uint64_t data_off, data_sz;
            std::uint32_t next;
            std::uint32_t name_len;
            char name[];
        };
        FNX_ASSERT_SIZE(FileEntryMeta, 0x20);
        FNX_ASSERT_LAYOUT(FileEntryMeta);

        constexpr static std::uint32_t invalid_meta = 0xffffffff;

    protected:
        void read_tables();
        std::uint32_t calc_path_hash(std::uint32_t parent_offset, const std::string_view &name) const;

    protected:
        Header header;

        std::vector<std::uint32_t> dir_hash_tbl;
        std::vector<std::uint32_t> file_hash_tbl;
        std::vector<std::uint8_t>  dir_meta_tbl;
        std::vector<std::uint8_t>  file_meta_tbl;

        std::vector<std::unique_ptr<DirEntry>>  dir_entries;
        std::vector<std::unique_ptr<FileEntry>> file_entries;
};

} // namespace fnx::hac
