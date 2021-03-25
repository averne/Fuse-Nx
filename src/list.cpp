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

#include "utils.hpp"

#include "list.hpp"

namespace fnx {

int ListContext::run(const Options &options) {
    auto callback = [&](const std::filesystem::path &path) -> bool {
        std::filesystem::path::iterator it;
        for (it = path.begin(); it != --path.end(); ++it)
            std::printf("%s", ListContext::indent.data());
        std::puts(PATHSTR(*it).c_str());
        return false;
    };

    if (auto opt = this->filesys->find_folder("/"); !opt)
        return 1;

    std::puts("/");
    this->filesys->walk("/", options.depth, callback, callback);
    return 0;
}

} // namespace fnx
