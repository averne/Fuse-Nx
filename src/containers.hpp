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

#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <fnx.hpp>

namespace fnx {

struct ContainerBase {
    using FileEntry = std::pair<std::string, std::unique_ptr<io::FileBase>>;
    using DirEntry  = std::pair<std::string, std::unique_ptr<ContainerBase>>;

    virtual ~ContainerBase() = default;

    virtual bool parse() = 0;

    virtual std::vector<FileEntry> read_files() {
        return {};
    }

    virtual std::vector<DirEntry> read_folders() {
        return {};
    }

    virtual std::string_view name() const = 0;
};

template <typename T>
struct Container: public ContainerBase {
    public:
        Container(std::unique_ptr<io::FileBase> &&base): container(std::make_shared<T>(std::move(base))) { }
        Container(std::shared_ptr<T> container): container(std::move(container)) { }

        virtual bool parse() override {
            return this->container->parse();
        }

        virtual std::string_view name() const override {
            return this->container->get_name();
        }

    protected:
        std::shared_ptr<T> container;
};

class PfsContainer final: public Container<hac::Pfs> {
    public:
        PfsContainer(std::unique_ptr<io::FileBase> &&base): Container(std::move(base)) { }
        virtual std::vector<FileEntry> read_files() override;
};

class HfsContainer final: public Container<hac::Hfs> {
    public:
        HfsContainer(std::unique_ptr<io::FileBase> &&base): Container(std::move(base)) { }
        virtual std::vector<FileEntry> read_files() override;
};

class RomFsContainer final: public Container<hac::RomFs> {
    public:
        RomFsContainer(std::unique_ptr<io::FileBase> &&base):
            Container(std::move(base)), parsed(true), parsed_mtx(std::make_shared<std::mutex>()), path("/") { }

        RomFsContainer(const RomFsContainer &other, std::string &&path):
            Container(other.container), parsed_mtx(other.parsed_mtx), path(std::move(path)) { }

        virtual std::vector<FileEntry> read_files()   override;
        virtual std::vector<DirEntry>  read_folders() override;

        void parse_dir(hac::RomFs::DirEntry *dir) {
            if (!this->parsed) {
                std::scoped_lock lk(*this->parsed_mtx);
                this->container->parse_dir(dir);
                this->parsed = true;
            }
        }

    private:
        bool parsed = false;
        std::shared_ptr<std::mutex> parsed_mtx;
        std::string path;
};

class NcaContainer final: public Container<hac::Nca> {
    public:
        NcaContainer(std::unique_ptr<io::FileBase> &&base): Container(std::move(base)) { }
        virtual std::vector<FileEntry> read_files() override;

    private:
        constexpr static std::array section_names = {
            std::string_view("section 0"),
            std::string_view("section 1"),
            std::string_view("section 2"),
            std::string_view("section 3"),
        };
};

class XciContainer final: public Container<hac::Xci> {
    public:
        XciContainer(std::unique_ptr<io::FileBase> &&base): Container(std::move(base)) { }
        virtual std::vector<FileEntry> read_files() override;
};

} // namespace fnx
