/**
 * Q2Admin
 * Cryptographic functions
 */

#pragma once

void G_GenerateKeyPair(int bits);
bool G_LoadKeys(void);
void G_MessageDigest(byte *dest, byte *src, size_t src_len);
size_t G_PrivateDecrypt(byte *dest, byte *src, int src_len);
size_t G_PublicEncrypt(EVP_PKEY *key, byte *out, byte *in, size_t inlen);
void G_RSAError();
size_t G_SymmetricDecrypt(byte *dest, byte *src, size_t src_len);
size_t G_SymmetricEncrypt(byte *dest, byte *src, size_t src_len);
void hexDump(char *desc, void *addr, int len);
