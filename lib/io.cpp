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

#include <fnx/io.hpp>

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
    flockfile(this->fp.get());
    FNX_SCOPEGUARD([this] { funlockfile(this->fp.get()); });

    fseek(this->fp.get(), this->pos, SEEK_SET);
    auto read = fread(dest, 1, size, this->fp.get());
    this->pos += read;
    return read;
}

std::size_t File::write(const void *src, std::size_t size) {
    flockfile(this->fp.get());
    FNX_SCOPEGUARD([this] { funlockfile(this->fp.get()); });

    auto written = fwrite(src, 1, size, this->fp.get());
    this->pos += written;
    return written;
}

std::size_t OffsetFile::read(void *dest, std::uint64_t size) {
    auto clamped_pos  = std::clamp(this->pos, 0l, static_cast<std::int64_t>(this->fsize));
    auto clamped_size = std::clamp(size, 0ul, this->fsize - clamped_pos);

    this->pos += size;
    this->base->seek(clamped_pos + this->offset);
    return this->base->read(dest, clamped_size);
}

std::size_t CtrFile::read(void *dest, std::uint64_t size) {
    auto aligned_pos  = utils::align_down(std::clamp(this->pos, 0l, static_cast<std::int64_t>(this->fsize)), crypt::AesCtr::block_size),
        pos_diff = this->pos - aligned_pos;
    auto aligned_size = utils::align_up(std::clamp(size + pos_diff, 0ul, this->fsize - aligned_pos), crypt::AesCtr::block_size);

    this->base->seek(aligned_pos + this->offset);
    this->cipher.set_ctr((aligned_pos + this->offset) >> 4);
    if (aligned_size == size) {
        this->base->read(dest, aligned_size);
        this->cipher.decrypt(dest, size);
    } else { // Sad path, data doesn't fit and we have to allocate a new buffer
        auto read = this->base->read(aligned_size);
        this->cipher.decrypt(read);
        std::copy_n(read.begin() + pos_diff, std::min(size, read.size() - pos_diff), reinterpret_cast<std::uint8_t *>(dest));
    }

    this->pos += size;
    return size;
}

} // namespace fnx::io
