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

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include <fnx/crypto.hpp>
#include <fnx/utils.hpp>

namespace fnx::io {

enum class Whence {
    Set,
    Cur,
    End,
};

class FileBase {
    public:
        virtual ~FileBase() = default;

        virtual std::unique_ptr<FileBase> clone() const = 0;

        std::uint64_t size() const {
            return this->fsize;
        }

        void seek(std::int64_t pos, Whence whence = Whence::Set) {
            switch (whence) {
                case Whence::Set:
                    this->pos = pos;
                    break;
                case Whence::Cur:
                    this->pos += pos;
                    break;
                case Whence::End:
                    this->pos = this->fsize + pos;
                    break;
            }
        }

        void rewind() {
            this->seek(0);
        }

        std::int64_t tell() const {
            return this->pos;
        }

        virtual std::size_t read(void *dest, std::uint64_t size) = 0;

        std::vector<std::uint8_t> read(std::uint64_t size) {
            std::vector<std::uint8_t> data(size);
            this->read(data.data(), size);
            return data;
        }

        template <typename T>
        std::size_t read(T &dest) requires (std::is_trivial_v<T>) && (!utils::Container<T>) {
            return this->read(&dest, sizeof(T));
        }

        template <typename T>
        std::size_t read(T &dest) requires utils::Container<T> {
            return this->read(dest.data(), dest.size() * sizeof(typename T::value_type));
        }

        template <typename ...Args>
        auto read_at(std::int64_t offset, Args &&...args) {
            this->seek(offset, Whence::Set);
            return this->read(std::forward<Args>(args)...);
        }

        virtual std::size_t write(const void *data, std::size_t size) = 0;

        template <typename T>
        std::size_t write(const T &src) requires (std::is_trivial_v<T>) && (!utils::Container<T>) {
            return this->write(&src, sizeof(T));
        }

        template <typename T>
        std::size_t write(const T &src) requires utils::Container<T> {
            return this->write(src.data(), src.size() * sizeof(typename T::value_type));
        }

        template <typename ...Args>
        auto write_at(std::int64_t offset, Args &&...args) {
            this->seek(offset, Whence::Set);
            return this->write(std::forward<Args>(args)...);
        }

    protected:
        std::int64_t  pos   = 0;
        std::uint64_t fsize = 0;
};

class File final: public FileBase {
    public:
        File(const std::string_view &path, const char *mode = "r") {
            this->open(path, mode);
        }

        std::unique_ptr<FileBase> clone() const override {
            return std::make_unique<File>(*this);
        }

        bool good() const {
            return this->fp.get() != nullptr;
        }

        const std::string_view get_path() const {
            return this->path;
        }

        bool open(const std::string_view &path, const char *mode = "r");

        std::uint64_t update_size();

        virtual std::size_t read(void *dest, std::uint64_t size) override;
        virtual std::size_t write(const void *src, std::uint64_t size) override;

        using FileBase::read;
        using FileBase::write;

    protected:
        struct FileDeleter {
            void operator()(FILE *fp) const {
                if (fp)
                    fclose(fp);
            }
        };

        std::shared_ptr<FILE> fp;
        std::string path;
};

class OffsetFile final: public FileBase {
    public:
        OffsetFile() = default;
        OffsetFile(std::unique_ptr<FileBase> &&base, std::uint64_t size, std::int64_t offset):
                base(std::move(base)), offset(offset) {
            this->fsize = size;
        }

        OffsetFile(const OffsetFile &other): base(other.base->clone()), offset(other.offset) {
            this->fsize = other.fsize;
        }

        OffsetFile(OffsetFile &&other): base(std::move(other.base)), offset(other.offset) {
            this->fsize = other.fsize;
        }

        virtual std::unique_ptr<FileBase> clone() const override {
            return std::make_unique<OffsetFile>(*this);
        }

        virtual std::size_t read(void *dest, std::uint64_t size) override;

        virtual std::size_t write(const void *src, std::uint64_t size) override {
            FNX_UNUSED(src, size, offset);
            return 0;
        }

        using FileBase::read;
        using FileBase::write;

    private:
        std::unique_ptr<FileBase> base;
        std::size_t offset;
};

class CtrFile final: public FileBase {
    public:
        CtrFile() = default;
        CtrFile(std::unique_ptr<FileBase> &&base, crypt::AesCtr &&cipher, std::uint64_t size, std::int64_t offset = 0):
                base(std::move(base)), cipher(std::move(cipher)), offset(offset) {
            this->fsize = size;
        }

        CtrFile(const CtrFile &other): base(other.base->clone()), cipher(other.cipher), offset(other.offset) {
            this->fsize = other.fsize;
        }

        CtrFile(CtrFile &&other): base(std::move(other.base)), cipher(std::move(other.cipher)), offset(other.offset) {
            this->fsize = other.fsize;
        }

        virtual std::unique_ptr<FileBase> clone() const override {
            return std::make_unique<CtrFile>(*this);
        }

        virtual std::size_t read(void *dest, std::uint64_t size) override;

        virtual std::size_t write(const void *src, std::uint64_t size) override {
            FNX_UNUSED(src, size, offset);
            return 0;
        }

        using FileBase::read;
        using FileBase::write;

        crypt::AesCtr &get_cipher() {
            return this->cipher;
        }

        const crypt::AesCtr &get_cipher() const {
            return this->cipher;
        }

    private:
        std::unique_ptr<FileBase> base;
        crypt::AesCtr cipher;
        std::size_t   offset;
};

} // namespace fnx::io
