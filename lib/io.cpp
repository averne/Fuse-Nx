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

#include <cstring>
#include <cinttypes>

#include <fnx/io.hpp>

#ifdef __MINGW32__
#   define fseek fseeko64
#endif

namespace fnx::io {

bool File::open(const std::string_view &path, const char *mode) {
    if (this->fp.get())
        fclose(this->fp.get());
    this->path = path;
    this->fp   = std::shared_ptr<FILE>(fopen(path.data(), mode), FileDeleter());
    return this->fp != nullptr;
}

std::uint64_t File::update_size() {
    fseek(this->fp.get(), 0, SEEK_END);
    FNX_SCOPEGUARD([this] { fseek(this->fp.get(), this->pos, SEEK_SET); });
    return this->fsize = ftell(this->fp.get());
}

std::size_t File::read(void *dest, std::uint64_t size) {
#ifdef __MINGW32__
    _lock_file(this->fp.get());
#else
    flockfile(this->fp.get());
#endif

    FNX_SCOPEGUARD([this] {
#ifdef __MINGW32__
        _unlock_file(this->fp.get());
#else
        funlockfile(this->fp.get());
#endif
    });

    if (auto rc = fseek(this->fp.get(), this->pos, SEEK_SET); rc != 0)
        std::fprintf(stderr, "Failed to seek in %s: %d (%d -> %s)\n", this->path.c_str(), rc, errno, std::strerror(errno));

    auto read = std::fread(dest, 1, size, this->fp.get());
    if (read != size)
        std::fprintf(stderr, "Failed to read %s at %#" PRIx64 " (expected %#" PRIx64 ", got %#" PRIx64 "): %d (%s), %d (%s)\n",
            this->path.c_str(), this->pos, size, read, std::ferror(this->fp.get()), std::strerror(std::ferror(this->fp.get())),
            errno, std::strerror(errno));
    this->pos += read;
    return read;
}

std::size_t File::write(const void *src, std::uint64_t size) {
#ifdef __MINGW32__
    _lock_file(this->fp.get());
#else
    flockfile(this->fp.get());
#endif

    FNX_SCOPEGUARD([this] {
#ifdef __MINGW32__
        _unlock_file(this->fp.get());
#else
        funlockfile(this->fp.get());
#endif
    });

    auto written = std::fwrite(src, 1, size, this->fp.get());
    this->pos += written;
    return written;
}

std::size_t OffsetFile::read(void *dest, std::uint64_t size) {
    auto clamped_pos  = std::clamp(static_cast<std::uint64_t>(this->pos), static_cast<std::uint64_t>(0), this->fsize);
    auto clamped_size = std::clamp(size, static_cast<std::uint64_t>(0), this->fsize - clamped_pos);

    this->pos += size;
    this->base->seek(clamped_pos + this->offset);
    return this->base->read(dest, clamped_size);
}

std::size_t CtrFile::read(void *dest, std::uint64_t size) {
    auto aligned_pos  = utils::align_down(std::clamp(static_cast<std::uint64_t>(this->pos),
        static_cast<std::uint64_t>(0), this->fsize), crypt::AesCtr::block_size), pos_diff = this->pos - aligned_pos;
    auto aligned_size = utils::align_up(std::clamp(size + pos_diff, static_cast<std::uint64_t>(0),
        this->fsize - aligned_pos), crypt::AesCtr::block_size);

    std::scoped_lock lk(*this->cipher_mtx);
    this->base->seek(aligned_pos + this->offset);
    this->cipher->set_ctr((aligned_pos + this->offset) >> 4);
    if (aligned_size == size) {
        this->base->read(dest, aligned_size);
        this->cipher->decrypt(dest, size);
    } else { // Sad path, data doesn't fit and we have to allocate a new buffer
        auto read = this->base->read(aligned_size);
        this->cipher->decrypt(read);
        std::copy_n(read.begin() + pos_diff, std::min(size, read.size() - pos_diff), reinterpret_cast<std::uint8_t *>(dest));
    }

    this->pos += size;
    return size;
}

} // namespace fnx::io
