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
#include <vector>

#include "context.hpp"

namespace fnx {

class DumpContext final: public Context {
    public:
        struct Options {
            std::size_t                        depth = -1;
            std::size_t                        jobs  =  1;
            std::vector<std::filesystem::path> paths;
        };

    public:
        DumpContext(const std::filesystem::path &container, const std::filesystem::path &dest):
                Context(container), dest(dest) {
            this->filesys->set_keep_raw(true);
        }
        int run(const Options &options);

    private:
        std::filesystem::path dest;
};

} // namespace fnx
