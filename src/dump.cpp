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

#include <functional>

#include "thread_pool.hpp"
#include "vfs.hpp"
#include "utils.hpp"

#include "dump.hpp"

namespace fnx {

namespace fs = std::filesystem;

namespace {

fs::path operator +(const fs::path &lhs, const fs::path &rhs) {
    auto path = lhs;
    return path += rhs;
}

} // namespace

int DumpContext::run(const Options &options) {
    std::mutex stdout_mtx;
    auto worker = [&](const fs::path &path) {
        auto dest_file = dest + path;

        std::unique_lock lk(stdout_mtx);
        std::printf("Dumping \"%s\"\n", PATHSTR(dest_file).c_str());
        lk.unlock();

        auto src = *this->filesys->get_file(FileSystem::normalize_path(PATHSTR(path)));
        auto *fp = std::fopen(PATHSTR(dest_file).c_str(), "wb");
        if (!fp)
            return;

        std::size_t read = 0;
        std::vector<std::uint8_t> buf(0x400000); // 4MiB
        for (std::size_t offset = 0; offset < src->get_size(); offset += read) {
            read = src->read(buf.data(), buf.size(), offset);
            std::fwrite(buf.data(), read, 1, fp);
        }

        std::fclose(fp);
    };

    auto pool = ThreadPool<fs::path>(worker);
    pool.start_workers(options.jobs);

    auto callback_folder = [&](const fs::path &path) -> bool {
        std::error_code rc;
        fs::create_directories(dest + path, rc);
        return static_cast<bool>(rc);
    };

    auto callback_file = [&](const fs::path &path) -> bool {
        pool.queue_item(path);
        return false;
    };

    for (auto &path: options.paths) {
        if (auto opt = this->filesys->find_folder(path); opt) {
            if (callback_folder(path) || this->filesys->walk(path, options.depth, callback_folder, callback_file))
                return 1;
        } else if (auto opt = this->filesys->get_file(FileSystem::normalize_path(PATHSTR(path))); opt) {
            if (callback_folder(path.parent_path()) || callback_file(path))
                return 1;
        } else {
            std::fprintf(stderr, "Could not find path \"%s\" inside container \"%s\"\n",
                PATHSTR(path).c_str(), PATHSTR(container).c_str());
        }
    }

    pool.wait();
    return 0;
}

} // namespace fnx
