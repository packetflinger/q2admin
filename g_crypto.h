/**
 * Q2Admin
 * Cryptographic functions
 */

#pragma once

void cryptoGenKeyPair(int bits);
bool cryptoLoadKeys(void);
void cryptoMessageDigest(byte *dest, byte *src, size_t src_len);
size_t cryptoPrivateDecrypt(byte *dest, byte *src, int src_len);
size_t cryptoPublicEncrypt(EVP_PKEY *key, byte *out, byte *in, size_t inlen);
void cryptoLogRSAErrors();
size_t cryptoSymmetricDecrypt(byte *dest, byte *src, size_t src_len);
size_t cryptoSymmetricEncrypt(byte *dest, byte *src, size_t src_len);
bool cryptoLoadKeys(void);