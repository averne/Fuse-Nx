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

#include <cstdlib>
#include <algorithm>
#include <fstream>
#include <utility>

#include "keys.hpp"

namespace fnx {

namespace fs = std::filesystem;

namespace {

std::pair<std::string_view, std::string_view> parse_line(const std::string &line) {
    auto first_begin  = line.find_first_not_of(' '),             first_end  = line.find_first_of(" =", first_begin);
    auto second_begin = line.find_first_not_of(" =", first_end), second_end = std::min(line.find_first_of(" \r\n", second_begin), line.size());
    return { std::string_view(&line[first_begin], first_end - first_begin), std::string_view(&line[second_begin], second_end - second_begin) };
}

} // namespace

fs::path get_keyset_path(crypt::KeySetType type) {
#ifdef __MINGW32__
    fs::path path = getenv("USERPROFILE");
#else
    fs::path path = getenv("HOME");
#endif
    path /= ".switch";

    switch (type) {
        // TODO(C++20):
        // using enum crypt::KeySetType;
        case crypt::KeySetType::Prod:
            path /= "prod.keys";
            break;
        case crypt::KeySetType::Dev:
            path /= "dev.keys";
            break;
        case crypt::KeySetType::Title:
            path /= "title.keys";
            break;
    }

    return path;
}

std::unique_ptr<crypt::KeySet> parse_console(const fs::path &path) {
    auto set = std::make_unique<crypt::KeySet>();
    if (!fs::exists(path))
        return set;

    auto stream = std::ifstream(path);

    std::string line;
    while (std::getline(stream, line)) {
        auto [id, key] = parse_line(line);
        set->set_key(id, key);
    }

    return set;
}

std::unique_ptr<crypt::TitlekeySet> parse_title(const fs::path &path) {
    auto set = std::make_unique<crypt::TitlekeySet>();
    if (!fs::exists(path))
        return set;

    auto stream = std::ifstream(path);

    std::string line;
    while (std::getline(stream, line)) {
        auto [id, key] = parse_line(line);
        set->set_key(id, key);
    }

    return set;
}

void init_keyset(crypt::KeySetType type, const fs::path &path) {
    switch (type) {
        // TODO(C++20):
        // using enum crypt::KeySetType;
        case crypt::KeySetType::Prod:
        case crypt::KeySetType::Dev:
            crypt::KeySet::set(parse_console(path.empty() ? get_keyset_path(type) : path));
            return;
        case crypt::KeySetType::Title:
            crypt::TitlekeySet::set(parse_title(path.empty() ? get_keyset_path(type) : path));
            return;
    }
}

void set_cli_titlekey(const std::string_view &key) {
    crypt::TitlekeySet::get()->set_cli_key(key);
}

} // namespace fnx
