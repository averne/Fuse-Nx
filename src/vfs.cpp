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

#include "vfs.hpp"

namespace fnx {

namespace fs = std::filesystem;

namespace {

void try_load_ticket_key(ContainerBase *container) {
    for (auto &&[name, file, _]: container->read_files()) {
        constexpr std::size_t tik_size = 0x2c0;
        if (name.ends_with(".tik") && file->size() >= tik_size) {
            auto dat = file->read(tik_size);

            std::printf("Detected ticket %s, loading title key\n", name.c_str());

            RightsId rights_id;
            std::copy_n(dat.data() + 0x2a0, sizeof(rights_id), rights_id.begin());

            crypt::AesKey key;
            std::copy_n(dat.data() + 0x180, sizeof(key), key.begin());

            crypt::TitlekeySet::get()->set_key(rights_id, key);
        }
    }
}

} // namespace


std::optional<std::shared_ptr<Folder>> File::make_container() const {
    std::unique_ptr<ContainerBase> container;

    auto fmt = hac::match(this->base->read_at(0, 0x400));
    switch (fmt) {
        case hac::Format::Pfs:
            container = std::make_unique<PfsContainer>(this->base->clone());
            break;
        case hac::Format::Hfs:
            container = std::make_unique<HfsContainer>(this->base->clone());
            break;
        case hac::Format::RomFs:
            container = std::make_unique<RomFsContainer>(this->base->clone());
            break;
        case hac::Format::Nca:
            container = std::make_unique<NcaContainer>(this->base->clone());
            break;
        case hac::Format::Xci:
            container = std::make_unique<XciContainer>(this->base->clone());
            break;
        case hac::Format::Unknown:
        default:
            return std::nullopt;
    }

    if (!container->parse())
        return std::nullopt;

    if (fmt == hac::Format::Pfs)
        try_load_ticket_key(container.get());

    auto &name = this->get_name();
    return std::make_shared<Folder>(name.substr(0, name.find_last_of('.')), std::move(container));
}

std::optional<std::shared_ptr<Folder>> FileSystem::process_dir(const fs::path &path) {
    auto opt = this->get_folder(FileSystem::normalize_path(PATHSTR(path)));
    if (!opt)
        return opt;

    auto dir = *opt;
    if (dir->is_processed())
        return opt;
    dir->process(this->keep_raw);

    for (const auto &folder: dir->get_children())
        this->add_folder(path / folder->get_name(), folder);

    for (const auto &file: dir->get_files())
        this->add_file(path / file->get_name(), file);

    return opt;
}

void Folder::process(bool keep_raw) {
    std::scoped_lock lk(this->processed_mtx);

    if (!this->base && this->processed)
        return;

    for (auto &&[name, file, try_container]: this->base->read_files()) {
        auto f = std::make_shared<File>(std::move(name), std::move(file));

        bool keep_file = keep_raw;
        if (try_container) {
            if (auto cont = f->make_container(); cont)
                this->children.emplace_back(std::move(*cont));
            else
                keep_file = true;
        }

        if (!try_container || keep_file)
            this->files.emplace_back(std::move(f));
    }

    for (auto &&[name, container]: this->base->read_folders())
        this->children.emplace_back(std::make_shared<Folder>(std::move(name), std::move(container)));

    this->processed = true;
}

std::optional<std::shared_ptr<Folder>> FileSystem::find_folder(const fs::path &path) {
    fs::path cur_path = "/";
    auto cur_dir = this->process_dir(cur_path);

    if (path == cur_path)
        return cur_dir;

    for (auto &loc: path.relative_path()) {
        cur_path /= loc;
        if (auto opt = this->process_dir(cur_path); !opt)
            return opt;
        else
            cur_dir = opt;
    }

    return cur_dir;
}

bool FileSystem::walk(const fs::path &location, std::size_t depth, const std::function<bool(const std::filesystem::path &)> &callback_folder,
        const std::function<bool(const std::filesystem::path &)> &callback_file) {
    if (!depth)
        return false;

    auto opt = this->get_folder(FileSystem::normalize_path(PATHSTR(location)));
    if (!opt)
        return true;

    for (auto &d: (*opt)->get_children()) {
        auto path = location / d->get_name();
        this->process_dir(path);

        if (callback_folder(path))
            return true;
        if (this->walk(path, depth - 1, callback_folder, callback_file))
            return true;
    }

    for (auto &f: (*opt)->get_files()) {
        if (callback_file(location / f->get_name()))
            return true;
    }

    return false;
}

} // namespace fnx
