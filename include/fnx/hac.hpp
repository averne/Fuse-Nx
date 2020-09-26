// Copyright (C) 2020 averne
//
// This file is part of Fuse-Nx.
//
// Fuse-Nx is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Fuse-Nx is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Fuse-Nx.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <type_traits>

#include <fnx/utils.hpp>
#include <fnx/formats/pfs.hpp>
#include <fnx/formats/hfs.hpp>
#include <fnx/formats/romfs.hpp>
#include <fnx/formats/nca.hpp>
#include <fnx/formats/xci.hpp>

namespace fnx::hac {

enum class Format {
    Pfs,
    Hfs,
    RomFs,
    Nca,
    Xci,
    Unknown,
};

// Matches a format against its header (magic, or constant fields)
template <typename T>
inline bool match(const utils::Container auto &data) {
    return T::match(data.data(), data.size() * sizeof(typename std::remove_cvref_t<decltype(data)>::value_type));
}

Format match(const utils::Container auto &data) {
    if (match<Pfs>(data))
        return Format::Pfs;
    else if (match<Hfs>(data))
        return Format::Hfs;
    else if (match<RomFs>(data))
        return Format::RomFs;
    else if (match<Xci>(data))
        return Format::Xci;
    else if (match<Nca>(data))
        return Format::Nca;
    return Format::Unknown;
}

} // namespace fnx::hac
