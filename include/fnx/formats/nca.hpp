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
#include <algorithm>
#include <memory>
#include <type_traits>

#include <fnx/io.hpp>
#include <fnx/types.hpp>
#include <fnx/formats/base.hpp>
#include <fnx/formats/pfs.hpp>
#include <fnx/formats/romfs.hpp>

namespace fnx::hac {

class Nca final: public FormatBase {
    public:
        constexpr static auto magic = utils::FourCC('N', 'C', 'A', '3');

        enum class DistributionType: std::uint8_t {
            System,
            Gamecard,
        };

        enum class ContentType: std::uint8_t {
            Program,
            Meta,
            Control,
            Manual,
            Data,
            PublicData,
        };

        class Section;

    public:
        static bool match(const void *data, std::size_t size);

        Nca(std::unique_ptr<io::FileBase> &&base);

        bool parse();

        bool is_valid() const {
            return this->header.magic == Nca::magic;
        }

        DistributionType get_distribution_type() const {
            return this->header.distribution_type;
        }

        ContentType get_content_type() const {
            return this->header.content_type;
        }

        std::uint64_t get_size() const {
            return this->header.size;
        }

        std::uint64_t get_title_id() const {
            return this->header.title_id;
        }

        std::array<std::uint8_t, 4> get_sdk_ver() const {
            return {
                static_cast<std::uint8_t>(this->header.sdk_ver >> 24 & 0xff),
                static_cast<std::uint8_t>(this->header.sdk_ver >> 16 & 0xff),
                static_cast<std::uint8_t>(this->header.sdk_ver >>  8 & 0xff),
                static_cast<std::uint8_t>(this->header.sdk_ver >>  0 & 0xff),
            };
        }

        const RightsId &get_rights_id() const {
            return this->header.right_id;
        }

        std::size_t get_num_sections() const {
            return this->sections.size();
        }

        std::vector<Section> &get_sections() {
            return this->sections;
        }

        const std::vector<Section> &get_sections() const {
            return this->sections;
        }

        Section &get_section(std::size_t idx) {
            return this->sections[idx];
        }

        const Section &get_section(std::size_t idx) const {
            return this->sections[idx];
        }

        std::string_view get_name() const {
            return "Nca";
        }

    protected:
        struct PfsSuperblock {
            crypt::Sha256Hash              master_hash;
            std::uint32_t                  block_size;
            std::uint32_t                  always_2;
            std::uint64_t                  hash_table_offset;
            std::uint64_t                  hash_table_size;
            std::uint64_t                  pfs_offset;
            std::uint64_t                  pfs_size;
            std::array<std::uint8_t, 0xf0> _res1;
        };
        FNX_ASSERT_SIZE(PfsSuperblock, 0x138);
        FNX_ASSERT_LAYOUT(PfsSuperblock);

        struct IvfcLvlHeader {
            std::uint64_t offset;
            std::uint64_t size;
            std::uint32_t block_size;
            std::uint32_t _res1;
        };

        struct RomFsSuperblock {
            constexpr static std::size_t ivfc_max_lvls = 6;

            std::uint32_t                  magic;
            std::uint32_t                  id;
            std::uint32_t                  master_hash_size;
            std::uint32_t                  num_levels;
            IvfcLvlHeader                  level_headers[ivfc_max_lvls];
            std::array<std::uint8_t, 0x20> _res1;
            crypt::Sha256Hash              master_hash;
            std::array<std::uint8_t, 0x58> _res2;
        };
        FNX_ASSERT_SIZE(RomFsSuperblock, 0x138);
        FNX_ASSERT_LAYOUT(RomFsSuperblock);

        enum class FsType: std::uint8_t {
            RomFs,
            Pfs,
        };

        enum class HashType: std::uint8_t {
            Auto,
            HierarchicalSha256    = 2, // Pfs
            HierarchicalIntegrity = 3, // RomFs
        };

        enum class EncryptionType: std::uint8_t {
            Auto,
            None,
            AesCtrOld,
            AesCtr,
            AesCtrEx,
        };

        struct FsEntry {
            constexpr static std::size_t media_size = 0x200;

            std::uint32_t media_start_offset, media_end_offset;
            std::uint32_t _res1, _res2;

            constexpr std::size_t start_offset() const {
                return this->media_start_offset * this->media_size;
            }

            constexpr std::size_t end_offset() const {
                return this->media_end_offset * this->media_size;
            }
        };
        FNX_ASSERT_SIZE(FsEntry, 0x10);
        FNX_ASSERT_LAYOUT(FsEntry);

        struct FsHeader {
            std::uint16_t                    version;
            FsType                           fs_type;
            HashType                         hash_type;
            EncryptionType                   encryption_type;
            std::array<std::uint8_t, 3>      _res1;
            union {
                PfsSuperblock                pfs_superblock;
                RomFsSuperblock              romfs_superblock;
            };
            std::uint64_t                    nonce;
            std::array<std::uint8_t, 0xb8>   _res2;
        };
        FNX_ASSERT_SIZE(FsHeader, 0x200);
        FNX_ASSERT_LAYOUT(FsHeader);

        constexpr static std::size_t max_sections = 4;

        struct Header {
            std::array<std::uint8_t, 0x100>  fixed_key_sig;
            std::array<std::uint8_t, 0x100>  npdm_key_sig;
            std::uint32_t                    magic;
            DistributionType                 distribution_type;
            ContentType                      content_type;
            std::uint8_t                     crypto_type;
            std::uint8_t                     kaek_idx;
            std::uint64_t                    size;
            std::uint64_t                    title_id;
            std::uint32_t                    _res1;
            std::uint32_t                    sdk_ver;
            std::uint8_t                     crypto_gen;
            std::uint8_t                     sig_gen;
            std::array<std::uint8_t, 0xe>    _res2;
            RightsId                         right_id;
            std::array<FsEntry, 4>           fs_entries;
            std::array<crypt::Sha256Hash, 4> hashes;
            std::array<crypt::AesKey, 4>     key_area;
            std::array<std::uint8_t, 0xc0>   _res3;
            std::array<FsHeader, 4>          fs_headers;
        };
        FNX_ASSERT_SIZE(Header, 0xc00);
        FNX_ASSERT_LAYOUT(Header);

    public:
        class Section {
            public:
                enum class Type {
                    Pfs,
                    RomFs,
                };

            public:
                Section(const FsEntry &entry, const FsHeader &header, const crypt::AesKey &key, std::unique_ptr<io::FileBase> &&base);
                Section(Section &&other) noexcept;
                ~Section();

                Type get_type() const {
                    return this->type;
                }

                std::size_t get_size() const {
                    return this->size;
                }

                Pfs &get_pfs() {
                    return *reinterpret_cast<Pfs *>(&this->container);
                }

                const Pfs &get_pfs() const {
                    return *reinterpret_cast<const Pfs *>(&this->container);
                }

                RomFs &get_romfs() {
                    return *reinterpret_cast<RomFs *>(&this->container);
                }

                const RomFs &get_romfs() const {
                    return *reinterpret_cast<const RomFs *>(&this->container);
                }

            protected:
                Type type;
                std::size_t offset, size;
                std::aligned_union_t<0, Pfs, RomFs> container;
        };

    private:
        bool decrypt_titlekey();
        void decrypt_keyarea();

        static void decrypt_header(Header &header);

    protected:
        Header        header;
        bool          has_rights_id = false;
        std::uint8_t  crypto_type;
        crypt::AesKey body_key;

        std::vector<Section> sections;
};

} // namespace fnx::hac
