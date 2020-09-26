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

#include "dump.hpp"

#include "vfs.hpp"

namespace fnx {

namespace fs = std::filesystem;

namespace {

fs::path operator +(const fs::path &lhs, const fs::path &rhs) {
    auto path = lhs;
    return path += rhs;
}

} // namespace

int DumpContext::run(const Options &options) {
    auto filesys = FileSystem(container, true);
    std::vector<std::uint8_t> buf(0x100000); // 1MiB

    std::function callback_folder = [&](const fs::path &path) -> bool {
        std::error_code rc;
        fs::create_directories(dest + path, rc);
        return static_cast<bool>(rc);
    };

    std::function callback_file = [&](const fs::path &path) -> bool {
        auto dest_file = dest + path;
        std::printf("Dumping \"%s\"\n", dest_file.c_str());

        auto src = *filesys.get_file(path);
        auto *fp = std::fopen(dest_file.c_str(), "wb");
        if (!fp)
            return true;

        std::size_t read = 0;
        for (std::size_t offset = 0; offset < src->get_size(); offset += read) {
            read = src->read(buf.data(), buf.size(), offset);
            if (read != buf.size())
                buf.resize(read);
            std::fwrite(buf.data(), read, 1, fp);
        }

        std::fclose(fp);
        return false;
    };

    for (auto &path: options.paths) {
        if (auto opt = filesys.find_folder(path); opt) {
            if (callback_folder(path) || filesys.walk(path, options.depth, callback_folder, callback_file))
                return 1;
        } else if (auto opt = filesys.get_file(path); opt) {
            if (callback_folder(path.parent_path()) || callback_file(path))
                return 1;
        } else {
            std::fprintf(stderr, "Could not find path \"%s\" inside container \"%s\"\n", path.c_str(), container.c_str());
        }
    }

    return 0;
}

} // namespace fnx
