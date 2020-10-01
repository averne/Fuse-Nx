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

#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
#include <fuse.h>

#include <fnx.hpp>

#include "containers.hpp"

namespace fnx {

class File;
class Folder;

struct Node {
    Node() = default;
    Node(std::string &&name): name(std::move(name)) { }

    const std::string &get_name() const {
        return this->name;
    }

    void set_name(const std::string &name) {
        this->name = name;
    }

    private:
        std::string name;
};

class File final: public Node {
    public:
        File() = default;
        File(std::string &&name): Node(std::move(name)) {}
        File(std::string &&name, std::unique_ptr<io::FileBase> &&base):
            Node(std::move(name)), base(std::move(base)) { }

        std::optional<std::shared_ptr<Folder>> make_container() const;

        std::size_t get_size() const {
            return this->base->size();
        }

        std::size_t read(void *buf, std::size_t size, std::size_t offset) const {
            return this->base->read_at(offset, buf, size);
        }

    private:
        std::unique_ptr<io::FileBase> base;
};

class Folder final: public Node {
    public:
        Folder() = default;
        Folder(std::string &&name): Node(std::move(name)) { }
        Folder(std::string &&name, std::unique_ptr<ContainerBase> &&base):
            Node(std::move(name)), base(std::move(base)) { }

        void process(bool keep_raw);

        bool is_processed() const {
            return this->processed;
        }

        const std::vector<std::shared_ptr<File>> &get_files() const {
            return this->files;
        }

        const std::vector<std::shared_ptr<Folder>> &get_children() const {
            return this->children;
        }

        void add_file(std::shared_ptr<File> file) {
            this->files.emplace_back(std::move(file));
        }

        void add_child(std::shared_ptr<Folder> folder) {
            this->children.emplace_back(std::move(folder));
        }

        bool has_container() const {
            return static_cast<bool>(this->base);
        }

        std::string_view get_container_name() const {
            return this->base->name();
        }

    private:
        bool processed = false;
        std::mutex processed_mtx;
        std::unique_ptr<ContainerBase> base;
        std::vector<std::shared_ptr<File>>   files;
        std::vector<std::shared_ptr<Folder>> children;
};

class FileSystem {
    public:
        FileSystem() = default;
        FileSystem(const std::filesystem::path &path): base("", std::make_unique<io::File>(path.c_str())) {
            if (auto root = this->base.make_container(); root)
                this->add_folder("/", std::move(*root));
        }

        void set_keep_raw(bool keep) {
            this->keep_raw = keep;
        }

        std::optional<std::shared_ptr<Folder>> process_dir(const std::filesystem::path &path);

        void add_file(const std::filesystem::path &&path, std::shared_ptr<File> node) {
            std::unique_lock lk(this->files_lock);
            this->files.try_emplace(std::move(path), std::move(node));
        }

        void add_folder(const std::filesystem::path &&path, std::shared_ptr<Folder> node) {
            std::unique_lock lk(this->folders_lock);
            this->folders.try_emplace(std::move(path), std::move(node));
        }

        std::optional<std::shared_ptr<File>> get_file(const std::string &path) const {
            std::shared_lock lk(this->files_lock);
            auto it = this->files.find(path);
            return (it != this->files.end()) ? std::make_optional(it->second) : std::nullopt;
        }

        std::optional<std::shared_ptr<Folder>> get_folder(const std::string &path) const {
            std::shared_lock lk(this->folders_lock);
            auto it = this->folders.find(path);
            return (it != this->folders.end()) ? std::make_optional(it->second) : std::nullopt;
        }

        std::optional<std::shared_ptr<Folder>> find_folder(const std::filesystem::path &path);

        bool walk(const std::filesystem::path &location, std::size_t depth,
            const std::function<bool(const std::filesystem::path &)> &callback_folder,
            const std::function<bool(const std::filesystem::path &)> &callback_file);

    private:
        File base;
        bool keep_raw;
        mutable std::shared_mutex files_lock, folders_lock;
        std::unordered_map<std::string, std::shared_ptr<File>>   files;
        std::unordered_map<std::string, std::shared_ptr<Folder>> folders;
};

} // namespace fnx
