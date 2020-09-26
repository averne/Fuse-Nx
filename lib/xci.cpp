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

#include <algorithm>
#include <string_view>
#include <utility>

#include <fnx/formats/xci.hpp>

namespace fnx::hac {

namespace {

using namespace std::literals::string_view_literals;

constexpr std::array partition_map = {
    std::make_pair("update"sv, Xci::PartitionType::Update),
    std::make_pair("normal"sv, Xci::PartitionType::Normal),
    std::make_pair("secure"sv, Xci::PartitionType::Secure),
    std::make_pair("logo"sv,   Xci::PartitionType::Logo),
};

} // namespace

Xci::Xci(std::unique_ptr<io::FileBase> &&base): FormatBase(std::move(base)) {
    this->base->read_at(0, this->header);
}

bool Xci::parse() {
    Hfs::Header root_header;
    this->base->read_at(this->header.hfs_offset, root_header);

    auto num_files = root_header.num_files;
    std::vector<Hfs::FileEntryMeta> file_entries(num_files);
    this->base->read_at(this->header.hfs_offset + Hfs::file_table_offset, file_entries);

    auto strings_offset = this->header.hfs_offset + Hfs::file_table_offset + num_files * sizeof(Hfs::FileEntryMeta);
    auto data_offset    = strings_offset + root_header.string_table_size;
    std::vector<char> names_table(root_header.string_table_size);
    this->base->read_at(strings_offset, names_table);

    this->partitions.reserve(num_files);
    for (std::size_t i = 0; i < num_files; ++i) {
        auto &entry = file_entries[i];
        auto name = std::string_view(&names_table[entry.name_offset]);

        auto it = std::find_if(partition_map.begin(), partition_map.end(),
            [&name](const auto &pair) {
                return pair.first == name;
            }
        );

        if (it == partition_map.end())
            continue;

        this->partitions.emplace_back(Partition(it->second,
            std::make_unique<io::OffsetFile>(this->base->clone(), entry.size, entry.offset + data_offset)));
    }

    return true;
}

std::string_view Xci::Partition::get_name() const {
    auto it = std::find_if(partition_map.begin(), partition_map.end(),
        [this](const auto &pair) {
            return pair.second == this->type;
        }
    );
    return (it != partition_map.end()) ? it->first : "unknown";
}

} // namespace fnx::hac
