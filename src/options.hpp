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
#include <thread>
#include <CLI/CLI.hpp>

#include "dump.hpp"
#include "find.hpp"
#include "fuse.hpp"
#include "keys.hpp"

namespace fnx {

struct KeyOptions {
    bool                  keyset_dev;
    std::filesystem::path keyset_path;
    std::string           titlekey;
    CLI::Option          *keyset_path_option;

    KeyOptions(CLI::App &app) {
        app.add_flag("-d,--dev,!--prod", this->keyset_dev, "Decrypt with development keys instead of retail")
            ->default_val(false);
        app.add_option("-k,--keyset", this->keyset_path, "Load keys from an external file")
            ->check(CLI::ExistingFile);
        app.add_option("--titlekey", this->titlekey, "Set title key for Rights ID crypto titles");
    }

    void init() {
        init_keyset(this->keyset_dev ? crypt::KeySetType::Dev : crypt::KeySetType::Prod, this->keyset_path);
        init_keyset(crypt::KeySetType::Title);
        if (!this->titlekey.empty())
            set_cli_titlekey(this->titlekey);
    }
};

struct FuseOptions {
    CLI::App             *fuse_cmd;
    std::filesystem::path container;
    std::filesystem::path mountpoint;
    FuseContext::Options  opts;

    FuseOptions(CLI::App &app) {
        this->fuse_cmd = app.add_subcommand("mount", "Mount container as filesystem");
        this->fuse_cmd->add_flag("-r,--keep-raw", this->opts.raw_containers, "Expose raw subcontainers");
        this->fuse_cmd->add_option("-o", this->opts.fuse_args, "Additional arguments forwarded to FUSE");
        this->fuse_cmd->add_option("container", this->container, "Path of the container to mount")
            ->check(CLI::ExistingFile)
            ->required();
        this->fuse_cmd->add_option("mountpoint", this->mountpoint, "Path of the mountpoint");
    }

    int run() {
        return FuseContext(this->container, this->mountpoint).run(this->opts);
    }
};

struct FindOptions {
    CLI::App             *find_cmd;
    std::filesystem::path container;
    std::string           exp;
    FindContext::Options  opts;

    FindOptions(CLI::App &app) {
        this->find_cmd = app.add_subcommand("find", "Find file or folder in archive and print its full path");
        this->find_cmd->add_flag("-e,--regex", this->opts.is_regex, "Treat pattern as regular expression");
        this->find_cmd->add_flag("-i,--ignore-case", this->opts.case_insensitive, "Ignore case distinctions");
        this->find_cmd->add_option("-m,--max-count", this->opts.max_count, "Stop after N matches")
            ->type_name("N")
            ->check(CLI::NonNegativeNumber);
        this->find_cmd->add_option("-d,--depth", this->opts.depth, "Stop after N levels into the filesystem hierarchy")
            ->type_name("N")
            ->check(CLI::NonNegativeNumber);
        this->find_cmd->add_flag("-0", this->opts.null_terminator, "Terminate paths will a null character");
        this->find_cmd->add_option("expression", this->exp, "Expression to match")
            ->required();
        this->find_cmd->add_option("container", this->container, "Path of the container to mount")
            ->check(CLI::ExistingFile)
            ->required();
        this->find_cmd->add_option("path", this->opts.start, "Path to search in inside the container");
    }

    int run() {
        return FindContext(this->container, this->exp).run(this->opts);
    }
};

struct DumpOptions {
    CLI::App             *dump_cmd;
    std::filesystem::path container;
    std::filesystem::path dest;
    DumpContext::Options  opts;

    DumpOptions(CLI::App &app) {
        this->dump_cmd = app.add_subcommand("dump", "Dump file or folders");
        this->dump_cmd->add_option("-d,--depth", this->opts.depth, "Stop after N levels into the filesystem hierarchy")
            ->type_name("N")
            ->check(CLI::NonNegativeNumber);
        this->dump_cmd->add_option("-j,--jobs", this->opts.jobs, "Max number of jobs to spawn")
            ->check(CLI::NonNegativeNumber);
        this->dump_cmd->add_option("container", this->container, "Path of the container to mount")
            ->check(CLI::ExistingFile)
            ->required();
        this->dump_cmd->add_option("destination", this->dest, "Folder where to dump the files")
            ->check(CLI::ExistingDirectory)
            ->required();
        this->dump_cmd->add_option("paths", this->opts.paths, "Paths of the files and folder to dump inside the container");
    }

    int run() {
        return DumpContext(this->container, this->dest).run(this->opts);
    }
};

class ProgramOptions {
    public:
        template <typename ...Args>
        ProgramOptions(Args &&...args):
                app(std::forward<Args>(args)...),
                keyopts(this->app), fuseopts(this->app), findopts(this->app), dumpopts(this->app) {
            this->app.set_help_all_flag("--help-all", "Expand all help");
            this->app.require_subcommand(1);
        }

        int run() {
            this->keyopts.init();
            if (this->fuseopts.fuse_cmd->parsed())
                return this->fuseopts.run();
            else if (this->findopts.find_cmd->parsed())
                return this->findopts.run();
            else if (this->dumpopts.dump_cmd->parsed())
                return this->dumpopts.run();
            return 0;
        }

    public:
        CLI::App    app;
        KeyOptions  keyopts;
        FuseOptions fuseopts;
        FindOptions findopts;
        DumpOptions dumpopts;
};

} // namespace fnx
