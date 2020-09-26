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

#include <fnx/crypto.hpp>

namespace fnx::crypt {

AesKey gen_aes_kek(const AesKey &src, const AesKey &mkey, const AesKey &kek_seed, const AesKey &key_seed) {
    AesKey key, kek;
    AesEcb(mkey).decrypt(kek_seed, key);
    AesEcb(key).decrypt(src, kek);
    AesEcb(kek).decrypt(key_seed, key);
    return key;
}

} // namespace fnx::crypt
