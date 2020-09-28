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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>

#include "fuse.hpp"

namespace fnx {

namespace {

FileSystem *s_fs;

FileSystem &get_fs() {
    return *reinterpret_cast<FileSystem *>(fuse_get_context()->private_data);
}

} // namespace

FuseContext::FuseContext(const std::filesystem::path &container, std::filesystem::path &mountpoint):
        Context(container), mountpoint(mountpoint) {
    if (this->mountpoint.empty())
        this->mountpoint = container.parent_path() / container.stem();
    std::filesystem::create_directories(this->mountpoint);
}

int FuseContext::run(const Options &options) {
    this->filesys->set_keep_raw(options.raw_containers);

    auto dir = this->filesys->get_folder("/");
    std::printf("Mounting \"%s\" to \"%s\" as %s\n", this->container.c_str(),
        this->mountpoint.c_str(), (*dir)->get_container_name().data());

    this->ops.getattr = FuseContext::wrap_getattr;
    this->ops.readdir = FuseContext::wrap_readdir;
    this->ops.read    = FuseContext::wrap_read;
    this->ops.init    = FuseContext::wrap_init;

    struct fuse_args args = FUSE_ARGS_INIT(0, nullptr);
    FNX_SCOPEGUARD([&args] { fuse_opt_free_args(&args); });

    fuse_opt_add_arg(&args, "");                    // argv[0] (executable)
    fuse_opt_add_arg(&args, mountpoint.c_str());    // argv[1] (mountpoint)

    if (!options.background)
        fuse_opt_add_arg(&args, "-f");              // Foreground

    for (auto &arg: options.fuse_args)
        fuse_opt_add_arg(&args, ("-o" + arg).c_str());

    fuse_opt_add_arg(&args, "-osync_read");         // Synchronous reads

    s_fs = this->filesys.get();
	return fuse_main(args.argc, args.argv, &this->ops, nullptr);
}

int FuseContext::wrap_getattr(const char *path, struct stat *stbuf) {
    auto *ctx = fuse_get_context();
    auto &fs  = get_fs();

    *stbuf = {};
    stbuf->st_uid = ctx->uid;
    stbuf->st_gid = ctx->gid;

	if (fs.get_folder(path)) {
		stbuf->st_mode = S_IFDIR | 0555;
    } else if (auto opt = fs.get_file(path); opt) {
        stbuf->st_size = (*opt)->get_size();
        stbuf->st_mode = S_IFREG | 0444;
    } else {
		return -ENOENT;
    }

    return 0;
}

int FuseContext::wrap_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *info) {
    FNX_UNUSED(offset, info);
    auto &fs = get_fs();

    struct stat st = {};
    if (auto opt = fs.process_dir(std::filesystem::path(path)); opt) {
        auto dir = opt.value();

        st.st_mode = S_IFREG | 0555;
        for (auto &d: dir->get_children())
            filler(buf, d->get_name().c_str(), &st, 0);

        st.st_mode = S_IFREG | 0444;
        for (auto &f: dir->get_files())
            filler(buf, f->get_name().c_str(), &st, 0);
    } else {
        return -ENOENT;
    }

    return 0;
}

int FuseContext::wrap_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *info) {
    if (info->flags & (O_WRONLY | O_RDWR | O_CREAT | O_EXCL | O_TRUNC | O_APPEND))
        return -EROFS;

    auto &fs = get_fs();
    if (auto opt = fs.get_file(path); opt)
        return (*opt)->read(buf, size, offset);

    return -ENOENT;
}

void *FuseContext::wrap_init(struct fuse_conn_info *conn) {
    FNX_UNUSED(conn);
    return s_fs;
}

} // namespace fnx
