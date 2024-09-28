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

#include <cstdlib>
#include <cinttypes>
#include <algorithm>
#include <array>

#include <fnx/crypto.hpp>

#include <fnx/formats/nca.hpp>

namespace fnx::hac {

Nca::Section::Section(const FsEntry &entry, const FsHeader &header, const crypt::AesKey &key, std::unique_ptr<io::FileBase> &&base):
        offset(entry.start_offset()), size(entry.end_offset() - entry.start_offset()) {
    this->type = ((header.fs_type == FsType::Pfs) && (header.hash_type == HashType::HierarchicalSha256)) ?
        Type::Pfs : Type::RomFs;
    auto nonce = __builtin_bswap64(header.nonce);

    std::size_t offset, size;
    if (this->type == Type::Pfs) {
        offset = header.pfs_superblock.pfs_offset + this->offset;
        size   = header.pfs_superblock.pfs_size;
    } else {
        offset = header.romfs_superblock.level_headers[RomFsSuperblock::ivfc_max_lvls - 1].offset + this->offset;
        size   = header.romfs_superblock.level_headers[RomFsSuperblock::ivfc_max_lvls - 1].size;
    }

    std::unique_ptr<io::FileBase> file;
    if (header.encryption_type == EncryptionType::AesCtr)
        file = std::make_unique<io::CtrFile>(std::move(base), crypt::AesCtr(key, nonce), size, offset);
    else
        file = std::make_unique<io::OffsetFile>(std::move(base), size, offset);

    if (this->type == Type::Pfs)
        std::construct_at(reinterpret_cast<Pfs *>  (&this->container), std::move(file));
    else
        std::construct_at(reinterpret_cast<RomFs *>(&this->container), std::move(file));
}

Nca::Section::Section(Section &&other) noexcept: type(other.type), offset(other.offset), size(other.size) {
    if (this->type == Type::Pfs)
        std::construct_at(reinterpret_cast<Pfs *>  (&this->container), std::move(other.get_pfs()));
    else
        std::construct_at(reinterpret_cast<RomFs *>(&this->container), std::move(other.get_romfs()));
}

Nca::Section::~Section() {
    if (this->type == Type::Pfs)
        std::destroy_at(reinterpret_cast<Pfs *>  (&this->container));
    else
        std::destroy_at(reinterpret_cast<RomFs *>(&this->container));
}

void Nca::decrypt_header(Header &header) {
    static bool init_ctx = false;
    static crypt::AesXtsNintendo ctx;
    if (!init_ctx) {
        ctx = crypt::AesXtsNintendo(crypt::KeySet::get()->header_key);
        init_ctx = true;
    } else {
        ctx.set_sector(0);
    }

    // Decrypt up to the fs headers
    ctx.decrypt(&header, 0x400);

    if (header.magic == Nca::nca3_magic) {
        ctx.decrypt(header.fs_headers.data(), sizeof(Header) - 0x400);
    } else if (header.magic == Nca::nca2_magic) {
        for (auto &fs_hdr: header.fs_headers) {
            ctx.set_sector(0);
            ctx.decrypt(fs_hdr);
        }
    }
}

bool Nca::match(const void *data, std::size_t size) {
    Header header;
    std::copy_n(reinterpret_cast<const std::uint8_t *>(data), size, reinterpret_cast<std::uint8_t *>(&header));
    Nca::decrypt_header(header);
    return (header.magic == Nca::nca2_magic) || (header.magic == Nca::nca3_magic);
}

Nca::Nca(std::unique_ptr<io::FileBase> &&base): FormatBase(std::move(base)) {
    this->base->read_at(0, this->header);
    Nca::decrypt_header(this->header);
}

bool Nca::decrypt_titlekey() {
    crypt::AesKey tkey;
    try {
        tkey = crypt::TitlekeySet::get()->get_key(this->header.right_id);
    } catch (const std::out_of_range &error) {
        auto *rights_id = reinterpret_cast<const std::uint64_t *>(this->get_rights_id().data());
        std::fprintf(stderr, "Title key for Rights ID %016" PRIx64 "%016" PRIx64 " missing\n",
            __builtin_bswap64(rights_id[0]), __builtin_bswap64(rights_id[1]));
        return false;
    }

    auto &tkek = crypt::KeySet::get()->titlekeks[this->crypto_type];
    crypt::AesEcb(tkek).decrypt(tkey, this->body_key);
    return true;
}

void Nca::decrypt_keyarea() {
    auto *set = crypt::KeySet::get();
    auto area_key = crypt::gen_aes_kek(set->get_kaek(this->header.kaek_idx), set->master_keys[this->crypto_type],
        set->aes_kek_generation_source, set->aes_key_generation_source);
    crypt::AesEcb(area_key).decrypt(this->header.key_area);
}

bool Nca::parse() {
    this->crypto_type = (this->header.crypto_type > this->header.crypto_gen) ?
        this->header.crypto_type : this->header.crypto_gen;
    this->crypto_type = std::clamp(this->crypto_type - 1, 0, 0x1f);

    if (utils::is_nonzero(this->header.right_id)) {
        this->has_rights_id = true;
        if (!this->decrypt_titlekey())
            return false;
    } else {
        this->decrypt_keyarea();
        this->body_key = this->header.key_area[2];
    }

    this->sections.reserve(Nca::max_sections);
    for (std::size_t i = 0; i < Nca::max_sections; ++i) {
        if (auto entry = this->header.fs_entries[i]; entry.media_start_offset != 0) { // Section exists
            auto enc_type = this->header.fs_headers[i].encryption_type;
            if ((enc_type == EncryptionType::None) || (enc_type == EncryptionType::AesCtr))
                this->sections.emplace_back(entry, this->header.fs_headers[i], this->body_key, this->clone_base());
            else
                std::fprintf(stderr, "Unsupported encryption scheme %u for section %u\n",
                    static_cast<std::uint8_t>(enc_type), static_cast<std::uint8_t>(i));
        }
    }

    return true;
}

} // namespace fnx::hac
