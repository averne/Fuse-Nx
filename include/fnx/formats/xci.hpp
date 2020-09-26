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

#include <array>
#include <memory>

#include <fnx/crypto.hpp>
#include <fnx/io.hpp>
#include <fnx/utils.hpp>
#include <fnx/formats/base.hpp>
#include <fnx/formats/hfs.hpp>

namespace fnx::hac {

class Xci final: public FormatBase {
    public:
        constexpr static auto magic = utils::FourCC('H', 'E', 'A', 'D');

        enum class CartType: std::uint8_t {
            Gb1  = 0xfA,
            Gb2  = 0xf8,
            Gb4  = 0xf0,
            Gb8  = 0xe0,
            Gb16 = 0xe1,
            Gb32 = 0xe2,
        };

        enum class PartitionType {
            Update,
            Normal,
            Secure,
            Logo,
            Count,
        };

        class Partition {
            public:
                Partition(PartitionType type, std::unique_ptr<io::FileBase> &&base):
                    type(type), hfs(std::move(base)) { }

                PartitionType get_type() const {
                    return this->type;
                }

                std::string_view get_name() const;

                Hfs &get_hfs() {
                    return this->hfs;
                }

                const Hfs &get_hfs() const {
                    return this->hfs;
                }

            private:
                PartitionType type;
                Hfs hfs;
        };

    public:
        static inline bool match(const void *data, std::size_t size) {
            FNX_UNUSED(size);
            return reinterpret_cast<const Header *>(data)->magic == Xci::magic;
        }

        Xci(std::unique_ptr<io::FileBase> &&base);

        bool parse();

        bool is_valid() const {
            return this->header.magic == Xci::magic;
        }

        CartType get_cart_type() const {
            return this->header.cart_type;
        }

        std::size_t get_num_partitions() const {
            return this->partitions.size();
        }

        std::vector<Partition> &get_partitions() {
            return this->partitions;
        }

        const std::vector<Partition> &get_partitions() const {
            return this->partitions;
        }

        std::string_view get_name() const {
            return "Xci";
        }

    protected:
        struct Header {
            std::array<std::uint8_t, 0x100> sig;
            std::uint32_t                   magic;
            std::uint32_t                   secure_start;
            std::uint32_t                   backup_start;
            std::uint8_t                    keys_idx;
            CartType                        cart_type;
            std::uint8_t                    header_version;
            std::uint8_t                    flags;
            std::uint64_t                   package_id;
            std::uint64_t                   valid_end;
            crypt::AesCbc::Iv               iv;
            std::uint64_t                   hfs_offset;
            std::uint64_t                   hfs_size;
            crypt::Sha256Hash               header_hash;
            crypt::Sha256Hash               initial_hash;
            std::uint32_t                   security_mode;
            std::uint32_t                   t1_key_idx;
            std::uint32_t                   key_idx;
            std::uint32_t                   normal_end;
            std::array<std::uint8_t, 0x70>  encrypted_gc_info;
        };
        FNX_ASSERT_SIZE(Header, 0x200);
        FNX_ASSERT_LAYOUT(Header);

    protected:
        Header header;

        std::vector<Partition> partitions;
};

} // namespace fnx::hac
