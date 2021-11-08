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
#include <array>
#include <vector>
#include <utility>

#ifdef USE_GCRYPT
#   include <gcrypt.h>
#else
#   define MBEDTLS_ALLOW_PRIVATE_ACCESS
#   include <mbedtls/cipher.h>
#endif

#include <fnx/keyset.hpp>
#include <fnx/utils.hpp>

namespace fnx::crypt {

using Sha256Hash = std::array<std::uint8_t, 0x20>;

#ifdef USE_GCRYPT
template <int Algo, int Mode, typename Key>
#else
template <mbedtls_cipher_type_t Algo, typename Key>
#endif
class CipherBase {
    public:
#ifdef USE_GCRYPT
        using Error = gcry_error_t;
#else
        using Error = int;
#endif

        constexpr static std::size_t block_size = 0x10;

    public:
        CipherBase() {
#ifdef USE_GCRYPT
            gcry_cipher_open(&this->handle, Algo, Mode, 0);
#else
            mbedtls_cipher_init(&this->ctx);
            mbedtls_cipher_setup(&this->ctx, mbedtls_cipher_info_from_type(Algo));
#endif
        }

        CipherBase(const Key &key): CipherBase() {
            this->set_key(key);
        }

        CipherBase(const CipherBase &other): CipherBase(other.key) { }

#ifdef USE_GCRYPT
        CipherBase(CipherBase &&other): handle(std::exchange(other.handle, nullptr)), key(other.key) { }
#else
        CipherBase(CipherBase &&other): ctx(other.ctx), key(other.key) {
            other.ctx.cipher_ctx = nullptr;
        }
#endif

        virtual ~CipherBase() {
#ifdef USE_GCRYPT
            if (this->handle)
                gcry_cipher_close(this->handle);
#else
            if (this->ctx.cipher_ctx)
                mbedtls_cipher_free(&this->ctx);
#endif
        }

        CipherBase &operator =(CipherBase &&other) {
#ifdef USE_GCRYPT
            this->handle = std::exchange(other.handle, nullptr);
#else
            this->ctx    = other.ctx;
            other.ctx.cipher_ctx = nullptr;
#endif
            this->key    = other.key;
            return *this;
        }

        Error set_key(const Key &key) {
            this->key = key;
#ifdef USE_GCRYPT
            return gcry_cipher_setkey(this->handle, key.data(), key.size());
#else
            return mbedtls_cipher_setkey(&this->ctx, key.data(), key.size() * 8, MBEDTLS_DECRYPT);
#endif
        }

        virtual Error decrypt(const void *src, std::uint64_t src_size, void *dst, std::uint64_t dst_size) {
#ifdef USE_GCRYPT
            return gcry_cipher_decrypt(this->handle, dst, dst_size, src, src_size);
#else
            FNX_UNUSED(src_size);
            return mbedtls_cipher_update(&this->ctx, reinterpret_cast<const std::uint8_t *>(src), dst_size,
                reinterpret_cast<std::uint8_t *>(dst), &dst_size);
#endif
        }

        Error decrypt(void *data, std::uint64_t size) { // In-place decryption
#ifdef USE_GCRYPT
            return this->decrypt(nullptr, 0, data, size);
#else
            return this->decrypt(data, size, data, size);
#endif
        }

        template <typename T, typename U>
        Error decrypt(const T *src, std::uint64_t src_size, U *dst, std::uint64_t dst_size)  {
            return this->decrypt(static_cast<const void *>(src), src_size, static_cast<void *>(dst), dst_size);
        }

        template <typename T>
        Error decrypt(T *data, std::uint64_t size) {
#ifdef USE_GCRYPT
            return this->decrypt(nullptr, 0, static_cast<void *>(data), size);
#else
            return this->decrypt(static_cast<const void *>(data), size, static_cast<void *>(data), size);
#endif
        }

        template <typename T>
        Error decrypt(const void *in, T &dest) {
            return this->decrypt(in, sizeof(dest), static_cast<void *>(&dest), sizeof(dest));
        }

        template <typename T>
        Error decrypt(T &dest) requires (std::is_trivial_v<T>) && (!utils::Container<T>) {
            return this->decrypt(static_cast<void *>(&dest), sizeof(T));
        }

        template <typename T, typename U>
        Error decrypt(const T &src, U &dst) requires utils::Container<T> && utils::Container<U> {
            return this->decrypt(src.data(), src.size() * sizeof(typename T::value_type),
                dst.data(), dst.size() * sizeof(typename U::value_type));
        }

        template <typename T>
        Error decrypt(T &dst) requires utils::Container<T> {
            return this->decrypt(dst.data(), dst.size() * sizeof(typename T::value_type));
        }

    protected:
#ifdef USE_GCRYPT
        gcry_cipher_hd_t handle;
#else
        mbedtls_cipher_context_t ctx;
#endif
        Key key;
};

#ifdef USE_GCRYPT
class AesEcb final: public CipherBase<GCRY_CIPHER_AES128, GCRY_CIPHER_MODE_ECB, AesKey> {
#else
class AesEcb final: public CipherBase<MBEDTLS_CIPHER_AES_128_ECB, AesKey> {
#endif
    public:
        AesEcb() = default;
        AesEcb(const AesKey &key): CipherBase(key) { }

        virtual Error decrypt(const void *src, std::uint64_t src_size, void *dst, std::uint64_t dst_size) override {
#ifdef USE_GCRYPT
            return gcry_cipher_decrypt(this->handle, dst, dst_size, src, src_size);
#else
            for (std::uint64_t i = 0; i < dst_size; i += CipherBase::block_size) { // In AES-ECB mode, mbedtls only decrypts one block max
                auto rc = mbedtls_cipher_update(&this->ctx, reinterpret_cast<const std::uint8_t *>(src) + i, CipherBase::block_size,
                    reinterpret_cast<std::uint8_t *>(dst) + i, &src_size);
                if (rc)
                    return rc;
            }
            return 0;
#endif
        }

        using CipherBase::decrypt;
};

#ifdef USE_GCRYPT
class AesCbc final: public CipherBase<GCRY_CIPHER_AES128, GCRY_CIPHER_MODE_ECB, AesKey> {
#else
class AesCbc final: public CipherBase<MBEDTLS_CIPHER_AES_128_CBC, AesKey> {
#endif
    public:
        using Iv = std::array<std::uint8_t, CipherBase::block_size / sizeof(std::uint8_t)>;
};

#ifdef USE_GCRYPT
class AesCtr: public CipherBase<GCRY_CIPHER_AES128, GCRY_CIPHER_MODE_CTR, AesKey> {
#else
class AesCtr: public CipherBase<MBEDTLS_CIPHER_AES_128_CTR, AesKey> {
#endif
    public:
        using Ctr = std::array<std::uint64_t, CipherBase::block_size / sizeof(std::uint64_t)>;
        using CipherBase::Error;

    public:
        AesCtr() = default;

        AesCtr(const AesKey &key, std::uint64_t nonce = 0): CipherBase(key), nonce(nonce) {
            this->set_ctr(0);
        }

        AesCtr(const AesKey &key, const Ctr &ctr): CipherBase(key), nonce(ctr[0]) {
            this->set_ctr(ctr);
        }

        Error set_ctr(std::uint64_t val) {
            return this->set_ctr(Ctr{ this->nonce, __builtin_bswap64(val) });
        }

        Error set_ctr(const Ctr &ctr) {
#ifdef USE_GCRYPT
            return gcry_cipher_setctr(this->handle, ctr.data(), ctr.size() * sizeof(Ctr::value_type));
#else
            return mbedtls_cipher_set_iv(&this->ctx, reinterpret_cast<const std::uint8_t *>(ctr.data()), ctr.size() * sizeof(Ctr::value_type));
#endif
        }

    private:
        std::uint64_t nonce;
};

#ifdef USE_GCRYPT
class AesXtsNintendo final: public CipherBase<GCRY_CIPHER_AES128, GCRY_CIPHER_MODE_XTS, AesXtsKey> {
#else
class AesXtsNintendo final: public CipherBase<MBEDTLS_CIPHER_AES_128_XTS, AesXtsKey> {
#endif
    public:
        constexpr static std::size_t sector_size = 0x200;

    public:
        AesXtsNintendo(std::uint64_t sector = 0): sector(sector) { }

        AesXtsNintendo(const AesXtsKey &key, std::uint64_t sector = 0): CipherBase(key), sector(sector) { }

        Error set_sector(std::uint64_t sector) {
            this->sector = sector;
            std::array<std::uint64_t, 2> tweak = { 0, __builtin_bswap64(sector) }; // Nintendo uses a byteswapped tweak
#ifdef USE_GCRYPT
            return gcry_cipher_setiv(this->handle, tweak.data(), tweak.size() * sizeof(std::uint64_t));
#else
            return mbedtls_cipher_set_iv(&this->ctx, reinterpret_cast<const std::uint8_t *>(tweak.data()), tweak.size() * sizeof(std::uint64_t));
#endif
        }

        virtual Error decrypt(const void *src, std::uint64_t src_size, void *dst, std::uint64_t dst_size) override {
            auto *_src = reinterpret_cast<const std::uint8_t *>(src);
            auto *_dst = reinterpret_cast<std::uint8_t *>(dst);
            std::uint64_t size = (!_src) ? dst_size : std::min(src_size, dst_size); // In-place decryption

            for (std::uint64_t i = 0; i < size; i += this->sector_size) {
                if (auto rc = this->set_sector(this->sector); rc)
                    return rc;
#ifdef USE_GCRYPT
                if (!_src) {
                    if (auto rc = this->decrypt_sector(&_dst[i]); rc)
                        return rc;
                } else {
                    if (auto rc = this->decrypt_sector(&_src[i], &_dst[i]); rc)
                        return rc;
                }
#else
                if (auto rc = this->decrypt_sector(&_src[i], &_dst[i]); rc)
                    return rc;
#endif
                ++this->sector;
            }
            return 0;
        }

        using CipherBase::decrypt;

    private:
        Error decrypt_sector(const void *in, void *out) {
            return CipherBase::decrypt(in, this->sector_size, out, this->sector_size);
        }

#ifdef USE_GCRYPT
        Error decrypt_sector(void *data) {
            return CipherBase::decrypt(nullptr, 0, data, this->sector_size);
        }
#endif

    protected:
        std::uint64_t sector;
};

AesKey gen_aes_kek(const AesKey &src, const AesKey &mkey, const AesKey &kek_seed, const AesKey &key_seed);

} // namespace fnx::crypt
