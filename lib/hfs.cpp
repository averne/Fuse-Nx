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

#include <fnx/formats/hfs.hpp>

namespace fnx::hac {

Hfs::Hfs(std::unique_ptr<io::FileBase> &&base): FormatBase(std::move(base)) {
    this->base->read_at(0, this->header);
}

bool Hfs::parse() {
    auto num_files = this->get_num_entries();

    std::vector<FileEntryMeta> file_entries(num_files);
    this->base->read_at(sizeof(Header), file_entries);

    this->strings_offset = sizeof(Header) + num_files * sizeof(FileEntryMeta);
    this->data_offset    = this->strings_offset + this->header.string_table_size;
    this->names_table.resize(this->header.string_table_size);
    this->base->read_at(this->strings_offset, this->names_table);

    this->entries.resize(num_files);
    for (std::size_t i = 0; i < num_files; ++i) {
        this->entries[i].offset = file_entries[i].offset;
        this->entries[i].size   = file_entries[i].size;
        this->entries[i].name   = std::string_view(&this->names_table[file_entries[i].name_offset]);
    }

    return true;
}

std::unique_ptr<io::OffsetFile> Hfs::open(const Entry &entry) const {
    return std::make_unique<io::OffsetFile>(this->base->clone(), entry.size, entry.offset + this->data_offset);
}

} // namespace fnx::hac
