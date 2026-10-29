/**
 * Copyright (C) The Q2Admin contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

bool cryptoLoadKeys(void);
void cryptoMessageDigest(byte *dest, byte *src, size_t src_len);
size_t cryptoPrivateDecrypt(byte *dest, byte *src, int src_len);
size_t cryptoPublicEncrypt(EVP_PKEY *key, byte *out, byte *in, size_t inlen);
void cryptoLogRSAErrors();
size_t cryptoSymmetricDecrypt(byte *dest, byte *src, size_t src_len);
size_t cryptoSymmetricEncrypt(byte *dest, byte *src, size_t src_len);
bool cryptoLoadKeys(void);