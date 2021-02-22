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
#include <re2/re2.h>

#ifdef __MINGW32__
#   include <shlwapi.h>
#else
#   include <fnmatch.h>
#endif

#include "vfs.hpp"
#include "utils.hpp"

#include "find.hpp"

namespace fnx {

int FindContext::run(const Options &options) {
    if (!options.max_count)
        return 0;

    std::size_t cur_count = 0;
    std::string format = options.null_terminator ? "%s" : "%s\n";

    std::unique_ptr<RE2> regex;
    std::function<bool(const std::filesystem::path &)> callback;
    if (options.is_regex) {
        RE2::Options opts(RE2::Quiet);
        opts.set_case_sensitive(!options.case_insensitive);
        opts.set_never_capture(true);
        regex = std::make_unique<RE2>(this->pattern, opts);
        if (!regex->ok()) {
            std::fprintf(stderr, "Failed to compile regex: %s\n", regex->error().c_str());
            return 1;
        }

        callback = [&](const std::filesystem::path &path) -> bool {
            if (RE2::FullMatch(path.filename().string(), *regex)) {
                std::printf(format.c_str(), FileSystem::normalize_path(PATHSTR(path)).c_str());
                if (options.null_terminator)
                    std::putchar(0);
                ++cur_count;
            }
            return cur_count >= options.max_count;
        };
    } else {
        callback = [&, this](const std::filesystem::path &path) -> bool {
#ifdef __MINGW32__
            if (PathMatchSpecA(path.filename().string().c_str(), this->pattern.c_str())) {
#else
            if (fnmatch(this->pattern.c_str(), path.filename().c_str(), options.case_insensitive ? FNM_CASEFOLD : 0) == 0) {
#endif
                std::printf(format.c_str(), FileSystem::normalize_path(PATHSTR(path)).c_str());
                if (options.null_terminator)
                    std::putchar(0);
                ++cur_count;
            }
            return cur_count >= options.max_count;
        };
    }

    auto opt = this->filesys->find_folder(options.start);
    if (!opt) {
        std::fprintf(stderr, "Could not find path \"%s\" inside container \"%s\"\n",
            PATHSTR(options.start).c_str(), PATHSTR(container).c_str());
        return 1;
    }

    this->filesys->walk(options.start, options.depth, callback, callback);
    return 0;
}

} // namespace fnx
