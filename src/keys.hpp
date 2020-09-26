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
#include <string_view>

#include <fnx.hpp>

namespace fnx {

std::filesystem::path get_keyset_path(crypt::KeySetType type);

std::unique_ptr<crypt::KeySet> parse_console(const std::filesystem::path &path);
std::unique_ptr<crypt::TitlekeySet> parse_title(const std::filesystem::path &path);

void init_keyset(crypt::KeySetType type, const std::filesystem::path &path = "");

void set_cli_titlekey(const std::string_view &key);

} // namespace fnx
