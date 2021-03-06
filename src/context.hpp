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

#include "vfs.hpp"
#include "utils.hpp"

namespace fnx {

struct Context {
    Context(const std::filesystem::path &container): container(container) {
        this->filesys = std::make_unique<FileSystem>(container);
        if (auto dir = this->filesys->get_folder("/"); !dir) {
            std::fprintf(stderr, "Unrecognized file type for \"%s\"\n", PATHSTR(container).c_str());
            std::exit(EXIT_FAILURE);
        }
    }

    protected:
        std::filesystem::path container;
        std::unique_ptr<FileSystem> filesys;
};

} // namespace fnx
