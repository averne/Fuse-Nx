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
#include <filesystem>
#include <string>
#include <vector>
#include <fuse.h>

#include "context.hpp"

namespace fnx {

class FuseContext final: public Context {
    public:
        struct Options {
            std::vector<std::string> fuse_args;
            bool                     raw_containers = false;
            bool                     background     = false;
        };

    public:
        FuseContext(const std::filesystem::path &container, std::filesystem::path &mountpoint);
        int run(const Options &options);

    private:
        static int   wrap_getattr(const char *, struct stat *);
        static int   wrap_readdir(const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info *);
        static int   wrap_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *info);
        static void *wrap_init(struct fuse_conn_info *);

    private:
        struct fuse_operations ops = {};
        std::filesystem::path mountpoint;
};

} // namespace fnx
