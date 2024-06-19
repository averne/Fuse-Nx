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

#include <fnx/formats/romfs.hpp>

namespace fnx::hac {

std::string RomFs::Entry::path() const {
    std::string path(this->name.data(), this->name.size());
    auto parent = this->parent;
    while (parent) {
        path.insert(0, 1, '/');
        path.insert(0, parent->name);
        parent = parent->parent;
    }
    return path;
}

RomFs::RomFs(std::unique_ptr<io::FileBase> &&base): FormatBase(std::move(base)) {
    this->base->read_at(0, this->header);
}

bool RomFs::parse_fast() {
    this->read_tables();

    std::size_t offset = 0;
    while (offset < this->dir_meta_tbl.size()) {
        auto *entry = reinterpret_cast<DirEntryMeta *>(&this->dir_meta_tbl[offset]);
        this->dir_entries.emplace_back(std::make_unique<DirEntry>(std::string_view(entry->name, entry->name_len)));
        offset += sizeof(DirEntryMeta) + utils::align_up(entry->name_len, 4);
    }

    offset = 0;
    while (offset < this->file_meta_tbl.size()) {
        auto *entry = reinterpret_cast<FileEntryMeta *>(&this->file_meta_tbl[offset]);
        this->file_entries.emplace_back(std::make_unique<FileEntry>(std::string_view(entry->name, entry->name_len),
            entry->data_off, entry->data_sz));
        offset += sizeof(FileEntryMeta) + utils::align_up(entry->name_len, 4);
    }

    return true;
}

bool RomFs::parse_dir(DirEntry *entry, bool recursive) {
    DirEntryMeta *meta;
    if (!entry) { // Root
        this->read_tables();
        meta = reinterpret_cast<DirEntryMeta *>(this->dir_meta_tbl.data());
        auto &root = this->dir_entries.emplace_back(std::make_unique<DirEntry>(nullptr, static_cast<void *>(meta),
            std::string_view(meta->name, meta->name_len)));
        entry = root.get();
    } else {
        meta = reinterpret_cast<DirEntryMeta *>(entry->meta);
    }

    if (meta->child_off != -1u) {
        auto *dir_meta = reinterpret_cast<DirEntryMeta *>(&this->dir_meta_tbl[meta->child_off]);
        while (static_cast<void *>(dir_meta) < static_cast<void *>(this->dir_meta_tbl.end().base())) {
            auto &dir = this->dir_entries.emplace_back(std::make_unique<DirEntry>(entry, static_cast<void *>(dir_meta),
                std::string_view(dir_meta->name, dir_meta->name_len)));
            entry->children.push_back(dir.get());

            if (recursive)
                this->parse_dir(dir.get(), recursive);

            if (dir_meta->sibling_off == -1u)
                break;

            dir_meta = reinterpret_cast<DirEntryMeta *>(&this->dir_meta_tbl[dir_meta->sibling_off]);
        }
    }

    if (meta->file_off != -1u) {
        auto *file_meta = reinterpret_cast<FileEntryMeta *>(&this->file_meta_tbl[meta->file_off]);
        while (static_cast<void *>(file_meta) < static_cast<void *>(this->file_meta_tbl.end().base())) {
            auto &file = this->file_entries.emplace_back(std::make_unique<FileEntry>(entry, static_cast<void *>(file_meta),
                std::string_view(file_meta->name, file_meta->name_len), file_meta->data_off, file_meta->data_sz));
            entry->files.push_back(file.get());

            if (file_meta->sibling_off == -1u)
                break;

            file_meta = reinterpret_cast<FileEntryMeta *>(&this->file_meta_tbl[file_meta->sibling_off]);
        }
    }

    return true;
}

RomFs::DirEntry *RomFs::find_dir(const std::string_view &path) {
    auto *entry = this->dir_entries.front().get();
    if (path == "/")
        return entry;

    auto cur_path = path;
    auto sep_pos  = path.find_first_of('/');
    while (sep_pos != std::string_view::npos) {
        cur_path = cur_path.substr(sep_pos + 1);
        sep_pos  = cur_path.find_first_of('/');
        auto cur_name = cur_path.substr(0, sep_pos);
        if (cur_name.empty())
            break;

        // Calculate offset into hash table
        auto offset = reinterpret_cast<uintptr_t>(entry->meta) - reinterpret_cast<uintptr_t>(&*this->dir_meta_tbl.begin());
        auto hash   = this->calc_path_hash(offset, cur_name) % (this->header.dir_tbl_sz / sizeof(std::uint32_t));
        auto bucket = this->dir_hash_tbl[hash];
        if (bucket == RomFs::invalid_meta)
            return nullptr;

        // Find meta
        auto *dir_meta = reinterpret_cast<const DirEntryMeta *>(&this->dir_meta_tbl[bucket]);
        while ((cur_name != std::string_view(dir_meta->name, dir_meta->name_len)) || (offset != dir_meta->parent_off)) {
            if (dir_meta->next == RomFs::invalid_meta)
                break;
            dir_meta = reinterpret_cast<const DirEntryMeta *>(&this->dir_meta_tbl[dir_meta->next]);
        }

        // Find entry
        auto it = std::find_if(entry->children.begin(), entry->children.end(), [dir_meta](auto *d) { return d->meta == dir_meta; });
        if (it == entry->children.end())
            return nullptr;
        entry = *it;
    }

    return entry;
}

RomFs::FileEntry *RomFs::find_file(const std::string_view &path) {
    auto sep_pos = path.find_last_of('/');
    auto *dir = this->find_dir(path.substr(0, sep_pos + 1));
    if (!dir)
        return nullptr;

    auto it = std::find_if(dir->files.begin(), dir->files.end(),
        [name = path.substr(sep_pos + 1)](auto *d) { return d->name == name; });

    return (it != dir->files.end()) ? *it : nullptr;
}

std::unique_ptr<io::OffsetFile> RomFs::open(const FileEntry &entry) const {
    return std::make_unique<io::OffsetFile>(this->clone_base(), entry.size, entry.offset + this->header.file_dat_off);
}

void RomFs::read_tables() {
    // Read hash tables
    this->dir_hash_tbl.resize (this->header.dir_tbl_sz  / sizeof(std::uint32_t));
    this->file_hash_tbl.resize(this->header.file_tbl_sz / sizeof(std::uint32_t));
    this->base->read_at(this->header.dir_tbl_off,  this->dir_hash_tbl);
    this->base->read_at(this->header.file_tbl_off, this->file_hash_tbl);

    // Read meta tables
    this->dir_meta_tbl.resize (this->header.dir_meta_sz);
    this->file_meta_tbl.resize(this->header.file_meta_sz);
    this->base->read_at(this->header.dir_meta_off,  this->dir_meta_tbl);
    this->base->read_at(this->header.file_meta_off, this->file_meta_tbl);

    // Preallocate entries vectors
    // Since the romfs doesn't contain a field for the total number of dir/file entries, we use the size
    // of the hash tables which are guaranteed to contain a number of buckets equal or greater to the number of entries
    this->dir_entries.reserve (this->header.dir_tbl_sz  / sizeof(std::uint32_t));
    this->file_entries.reserve(this->header.file_tbl_sz / sizeof(std::uint32_t));
}

std::uint32_t RomFs::calc_path_hash(std::uint32_t parent_offset, const std::string_view &name) const {
    auto hash = parent_offset ^ 123456789;
    for (auto c: name) {
        hash  = (hash >> 5) | (hash << 27);
        hash ^= c;
    }
    return hash;
}

} // namespace fnx::hac
