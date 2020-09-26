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
#include <filesystem>
#include <gcrypt.h>
#include <CLI/CLI.hpp>
#include <fnx.hpp>

#include "vfs.hpp"
#include "keys.hpp"
#include "options.hpp"

int main(int argc, char **argv) {
    auto opts = fnx::ProgramOptions("Fuse-Nx v" FUSENX_VERSION "\nBuilt on: " __DATE__ " " __TIME__);
    CLI11_PARSE(opts.app, argc, argv);
    return opts.run();
}
