// Copyright (C) 2020 averne
//
// This file is part of fuse-nx.
//
// fuse-nx is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// fuse-nx is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with fuse-nx.  If not, see <http://www.gnu.org/licenses/>.

#include "containers.hpp"

namespace fnx {

using FileEntry = ContainerBase::FileEntry;
using DirEntry  = ContainerBase::DirEntry;

namespace {

using namespace std::string_view_literals;

constexpr std::array extension_whitelist = {
    "nca", "nsp", "pfs", "romfs", "hfs", "xci",
};

constexpr bool should_try_container(std::string_view name) {
    auto ext = name.substr(name.rfind('.') + 1);
    return std::find(extension_whitelist.begin(), extension_whitelist.end(), ext) !=
        extension_whitelist.end();
}

} // namespace

std::vector<FileEntry> PfsContainer::read_files() {
    std::vector<FileEntry> out;
    out.reserve(this->container->get_num_entries());
    for (auto &entry: this->container->get_entries())
        out.emplace_back(entry.name, this->container->open(entry), true);
    return out;
}

std::vector<FileEntry> HfsContainer::read_files() {
    std::vector<FileEntry> out;
    out.reserve(this->container->get_num_entries());
    for (auto &entry: this->container->get_entries())
        out.emplace_back(entry.name, this->container->open(entry), true);
    return out;
}

std::vector<FileEntry> RomFsContainer::read_files() {
    std::vector<FileEntry> out;
    auto *dir = this->container->find_dir(this->path);
    if (!dir)
        return out;

    this->parse_dir(dir);
    for (auto *file: dir->files)
        out.emplace_back(file->name, this->container->open(*file),
            RomFsContainer::search_containers || should_try_container(file->name));
    return out;
}

std::vector<DirEntry> RomFsContainer::read_folders() {
    std::vector<DirEntry> out;
    auto *dir = this->container->find_dir(this->path);
    if (!dir)
        return out;

    this->parse_dir(dir);
    for (auto *child: dir->children)
        out.emplace_back(child->name, std::make_unique<RomFsContainer>(*this, this->path + std::string(child->name) + '/'));
    return out;
}

std::vector<FileEntry> NcaContainer::read_files() {
    std::vector<FileEntry> out;
    out.reserve(this->container->get_num_sections());
    for (std::size_t i = 0; i < this->container->get_num_sections(); ++i) {
        auto &section = this->container->get_section(i);
        if (section.get_type() == hac::Nca::Section::Type::Pfs)
            out.emplace_back(std::string(NcaContainer::section_names[i]) + ".nsp",   section.get_pfs().clone_base(), true);
        else
            out.emplace_back(std::string(NcaContainer::section_names[i]) + ".romfs", section.get_romfs().clone_base(), true);
    }
    return out;
}

std::vector<FileEntry> XciContainer::read_files() {
    std::vector<FileEntry> out;
    out.reserve(this->container->get_num_partitions());
    for (auto &section: this->container->get_partitions())
        out.emplace_back(std::string(section.get_name()) + ".hfs", section.get_hfs().clone_base(), true);
    return out;
}

} // namespace fnx
