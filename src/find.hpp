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
#include <string>

#include "context.hpp"

namespace fnx {

class FindContext final: public Context {
    public:
        struct Options {
            std::filesystem::path start            = "/";
            std::size_t           max_count        = -1;
            std::size_t           depth            = -1;
            bool                  is_regex         = false;
            bool                  case_insensitive = false;
            bool                  null_terminator  = false;
        };

    public:
        FindContext(const std::filesystem::path &container, const std::string &pattern):
                Context(container), pattern(pattern) {
            this->filesys->set_keep_raw(true);
        }
        int run(const Options &options);

    private:
        std::string pattern;
};

} // namespace fnx
