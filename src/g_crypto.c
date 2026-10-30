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

#include "g_local.h"

/**
 * Read public and private keys from filesystem into RSA structures
 *
 * Loads the three keys the cloud connection needs, all from moddir and
 * all named by config settings: this server's own private and public
 * keys, plus the cloud admin backend's public key. Each has a distinct
 * job in the handshake - the backend's public key encrypts challenges
 * only the real backend can read (cryptoPublicEncrypt()), and this server's
 * private key decrypts what comes back (cryptoPrivateDecrypt()). Shipping
 * the backend's key with the server is what makes the backend
 * impersonation-resistant: an imposter without the matching private key
 * can't answer a challenge.
 *
 * Takes no parameters; fills cloud.connection's key fields. Returns true
 * only if all three loaded, false on the first one that's missing or
 * unreadable, freeing whichever were already loaded so a partial set is
 * never left behind.
 *
 * Progress is reported to the console, since a failure here disables the
 * cloud feature entirely and an admin needs to see which file was at
 * fault.
 *
 * Called from the cloud connection setup in g_cloud.c, which gives up on
 * connecting if this returns false.
 */
bool cryptoLoadKeys(void) {
    FILE *fp;
    cloud_connection_t *c = &cloud.connection;
    // moddir and each key's file name are up to 255 characters, so this
    // fits both plus the separator; Q_snprintf truncates rather than
    // overflowing if those ever grow.
    char path[sizeof(moddir) + sizeof(cloud_config.private) + 1];

    gi.cprintf(NULL, PRINT_HIGH, "[cloud] loading encryption keys...");

    // first load our private key
    Q_snprintf(path, sizeof(path), "%s/%s", moddir, cloud_config.private);
    fp = fopen(path, "rb");
    if (!fp) {
        gi.cprintf(NULL, PRINT_HIGH, "failed, %s not found\n", path);
        return false;
    }

    c->private_key = PEM_read_PrivateKey(fp, NULL, NULL, NULL);
    fclose(fp);

    if (!c->private_key) {
        gi.cprintf(NULL, PRINT_HIGH, "failed, problems with your private key: %s\n", path);
        return false;
    }

    // then our public key
    Q_snprintf(path, sizeof(path), "%s/%s", moddir, cloud_config.public);
    fp = fopen(path, "rb");
    if (!fp) {
        gi.cprintf(NULL, PRINT_HIGH, "failed, %s not found\n", path);
        EVP_PKEY_free(c->private_key);
        return false;
    }

    c->public_key = PEM_read_PUBKEY(fp, NULL, NULL, NULL);

    fclose(fp);

    if (!c->public_key) {
        gi.cprintf(NULL, PRINT_HIGH, "failed, problems with your public key: %s\n", path);
        EVP_PKEY_free(c->private_key);
        return false;
    }

    // last the cloud admin server's public key
    Q_snprintf(path, sizeof(path), "%s/%s", moddir, cloud_config.serverkey);
    fp = fopen(path, "rb");
    if (!fp) {
        gi.cprintf(NULL, PRINT_HIGH, "failed, %s not found\n", path);
        EVP_PKEY_free(c->private_key);
        EVP_PKEY_free(c->public_key);
        return false;
    }

    c->server_key = PEM_read_PUBKEY(fp, NULL, NULL, NULL);
    fclose(fp);

    if (!c->server_key) {
        gi.cprintf(NULL, PRINT_HIGH, "failed, problems with the q2admin server's public key\n");
        EVP_PKEY_free(c->private_key);
        EVP_PKEY_free(c->public_key);
        return false;
    }

    gi.cprintf(NULL, PRINT_HIGH, "OK\n");
    return true;
}

/**
 * Decrypt src using our private key
 *
 * The receiving half of the handshake: the backend encrypts its reply
 * with this server's *public* key, so only this server - holding the
 * matching private key - can read it. That reply carries both the
 * backend's answer to our challenge and the AES session key and IV that
 * all subsequent traffic is encrypted with, so this is the step that
 * bootstraps the symmetric channel.
 *
 * dest:    buffer to receive the plaintext. Not bounds checked - the
 *          caller must know it's big enough for the decrypted result.
 * src:     the ciphertext.
 * src_len: length of src.
 *
 * Returns the plaintext length, or 0 on any failure.
 *
 * Called from the cloud handshake in g_cloud.c, on the backend's reply
 * to our hello.
 *
 * Note both the key context and the OpenSSL-allocated plaintext buffer
 * are leaked on every call, including the successful path - the result
 * is copied out to dest but never freed.
 */
size_t cryptoPrivateDecrypt(byte *dest, byte *src, int src_len) {
    size_t len = 0;

    EVP_PKEY *key = cloud.connection.private_key;
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(key, NULL);
    if (!ctx) {
        cloudPrintf("error creating private key context\n");
        return len;
    }

    if (EVP_PKEY_decrypt_init(ctx) <= 0) {
        cloudPrintf("error initializing decrypt\n");
        return len;
    }

    if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING) <= 0) {
        cloudPrintf("error adding decryption padding (PKCS1)\n");
        return len;
    }

    if (EVP_PKEY_decrypt(ctx, NULL, &len, (const unsigned char *)src, src_len) <= 0) {
        cloudPrintf("error getting decrypt size\n");
        return 0;
    }

    byte *newplain = OPENSSL_malloc(len);
    if (!newplain) {
        cloudPrintf("error mallocing in decrypt\n");
        return 0;
    }

    if (EVP_PKEY_decrypt(ctx, (unsigned char *)newplain, &len, (const unsigned char *)src, src_len) <= 0) {
        cloudPrintf("error decrypting\n");
        return 0;
    }

    q2a_memcpy(dest, newplain, len);
    return len;
}

/**
 * Encrypt a message using a public key. ONLY the matching private key
 * can decrypt the message.
 *
 * That property is the whole basis of the handshake's mutual
 * authentication. Encrypting a random nonce with the backend's public
 * key means only the real backend can read it back, so a correct answer
 * proves identity without either side ever transmitting a secret; the
 * same call then encrypts this server's answer to the backend's own
 * challenge, proving identity in the other direction.
 *
 * key:   the public key to encrypt to - in practice always the cloud
 *        backend's, loaded by cryptoLoadKeys().
 * out:   buffer to receive the ciphertext. Not bounds checked; callers
 *        size it at RSA_LEN.
 * in:    the plaintext.
 * inlen: length of in. RSA can only encrypt less than the key size, so
 *        callers pass small values - a nonce or a digest, never bulk
 *        data, which is what the AES channel is for.
 *
 * Returns the ciphertext length, or 0 if the context or encryption setup
 * failed.
 *
 * Called from the cloud handshake in g_cloud.c - once to send the
 * initial challenge, once more to answer the backend's.
 *
 * Note the key context is leaked on every call, and the post-allocation
 * NULL check tests `out` (the caller's buffer, always valid) rather than
 * the buffer just allocated, so an allocation failure prints a warning
 * and then carries on to use the NULL pointer.
 */
size_t cryptoPublicEncrypt(EVP_PKEY *key, byte *out, byte *in, size_t inlen) {
    size_t cipherlen = 0;
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(key, NULL);
    if (!ctx) {
        cloudPrintf("error creating context for encrypting\n");
        return 0;
    }

    if (EVP_PKEY_encrypt_init(ctx) <= 0) {
        cloudPrintf("encrypt init failed\n");
        return 0;
    }

    if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING) <= 0) {
        cloudPrintf("error adding padding type (PKCS1)\n");
        return 0;
    }

    if (EVP_PKEY_encrypt(ctx, NULL, &cipherlen, (const unsigned char *)in, inlen) <= 0) {
        cloudPrintf("encrypt error\n");
        return 0;
    }

    byte *out1 = OPENSSL_malloc(cipherlen);
    if (!out) {
        cloudPrintf("malloc error while encrypting\n");
    }

    if (EVP_PKEY_encrypt(ctx, (unsigned char *)out1, &cipherlen, (const unsigned char *)in, inlen) <= 0) {
        cloudPrintf("error encrypting\n");
    }

    q2a_memcpy(out, out1, cipherlen);
    OPENSSL_free(out1);
    return cipherlen;
}

/**
 * Drains OpenSSL's error queue and prints each entry, for turning an
 * opaque "encryption failed" into something diagnosable - OpenSSL
 * reports detail through that queue rather than through return values,
 * so without emptying it the reason is simply lost.
 *
 * Takes no parameters. Returns nothing.
 *
 * Currently unreachable: nothing calls it. The cloud code prints
 * ERR_error_string(ERR_get_error(), NULL) inline instead.
 *
 * Just as well, because as written it would crash: ERR_error_string()
 * writes into the caller's buffer, and it's handed a pointer to a string
 * literal with no room. It needs a writable buffer of at least 256
 * bytes, or NULL to use OpenSSL's own static one.
 */
void cryptoLogRSAErrors() {
    int error = 0;
    char *msg = "";
    ERR_load_crypto_strings();
    while ((error = ERR_get_error()) != 0) {
        ERR_error_string(error, msg);
        printf("Error (%d): %s\n", error, msg);
    }
}

/**
 * Builds the GCM nonce for one frame: the direction (CLOUD_DIR_*) in the
 * first 4 bytes, then how many frames have already gone that way under the
 * current key, both big-endian. See the framing comment in g_cloud.h for
 * why that never repeats. The backend (crypto.Stream) builds it the same
 * way.
 *
 * nonce:     GCM_NONCE_LEN bytes to fill in.
 * direction: CLOUD_DIR_TO_BACKEND or CLOUD_DIR_TO_FRONTEND.
 * count:     frames already sealed in that direction with this key.
 *
 * Returns nothing.
 */
static void cryptoGCMNonce(byte *nonce, uint32_t direction, uint64_t count) {
    for (int i = 0; i < 4; i++) {
        nonce[i] = (direction >> (24 - 8 * i)) & 0xff;
    }
    for (int i = 0; i < 8; i++) {
        nonce[4 + i] = (count >> (56 - 8 * i)) & 0xff;
    }
}

/**
 * Seals one frame's payload with AES-128-GCM: encrypts it and adds the
 * GCM_TAG_LEN-byte tag on the end. Doesn't touch the connection state, so
 * the tests can also seal frames the way the backend does.
 *
 * ctx:       cipher context to use.
 * key:       AESKEY_LEN-byte key.
 * direction: which way the frame goes (CLOUD_DIR_*).
 * count:     frames already sealed that way with this key.
 * dest:      receives the ciphertext and tag, src_len + GCM_TAG_LEN bytes.
 *            Not bounds checked.
 * dest_len:  set to how many bytes were written to dest.
 * src:       the plaintext.
 * src_len:   its length.
 *
 * Returns true if the frame was sealed, false if OpenSSL failed.
 */
bool cryptoGCMSeal(EVP_CIPHER_CTX *ctx, const byte *key, uint32_t direction, uint64_t count,
        byte *dest, size_t *dest_len, const byte *src, size_t src_len) {
    byte nonce[GCM_NONCE_LEN];
    int len = 0, final = 0;

    if (!ctx || src_len > INT_MAX) {
        return false;
    }
    cryptoGCMNonce(nonce, direction, count);
    if (EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, GCM_NONCE_LEN, NULL) != 1 ||
        EVP_EncryptInit_ex(ctx, NULL, NULL, key, nonce) != 1 ||
        EVP_EncryptUpdate(ctx, dest, &len, src, (int) src_len) != 1 ||
        EVP_EncryptFinal_ex(ctx, dest + len, &final) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, GCM_TAG_LEN, dest + len + final) != 1) {
        return false;
    }
    *dest_len = (size_t) (len + final) + GCM_TAG_LEN;
    return true;
}

/**
 * Opens one frame sealed by cryptoGCMSeal() (or the backend): checks the
 * tag and decrypts. Nothing is trusted unless this returns true; a frame
 * that was altered, or sealed with a different key, direction or count,
 * fails.
 *
 * ctx:       cipher context to use.
 * key:       AESKEY_LEN-byte key.
 * direction: which way the frame came (CLOUD_DIR_*).
 * count:     frames already opened that way with this key.
 * dest:      receives the plaintext, src_len - GCM_TAG_LEN bytes. Not
 *            bounds checked.
 * dest_len:  set to how many bytes were written to dest.
 * src:       the ciphertext and tag.
 * src_len:   their length, at least GCM_TAG_LEN.
 *
 * Returns true if the frame opened, false if it didn't.
 */
bool cryptoGCMOpen(EVP_CIPHER_CTX *ctx, const byte *key, uint32_t direction, uint64_t count,
        byte *dest, size_t *dest_len, const byte *src, size_t src_len) {
    byte nonce[GCM_NONCE_LEN];
    byte tag[GCM_TAG_LEN];
    int len = 0, final = 0;
    size_t ct_len;

    if (!ctx || src_len < GCM_TAG_LEN || src_len > INT_MAX) {
        return false;
    }
    ct_len = src_len - GCM_TAG_LEN;
    q2a_memcpy(tag, src + ct_len, GCM_TAG_LEN);
    cryptoGCMNonce(nonce, direction, count);
    if (EVP_DecryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, GCM_NONCE_LEN, NULL) != 1 ||
        EVP_DecryptInit_ex(ctx, NULL, NULL, key, nonce) != 1 ||
        EVP_DecryptUpdate(ctx, dest, &len, src, (int) ct_len) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, GCM_TAG_LEN, tag) != 1 ||
        EVP_DecryptFinal_ex(ctx, dest + len, &final) != 1) {
        return false;
    }
    *dest_len = (size_t) (len + final);
    return true;
}

/**
 * Seals a frame we're about to send, with the connection's send key and
 * the next nonce in the to-backend direction.
 *
 * dest:     receives the ciphertext and tag, src_len + GCM_TAG_LEN bytes.
 * dest_len: set to their length.
 * src:      the frame's payload.
 * src_len:  its length.
 *
 * Returns true if the frame was sealed. False means it can't be sent: the
 * backend will expect this nonce on the next frame, so the connection
 * can't go on.
 *
 * Called from cloudFrameQueue() (g_cloud.c).
 */
bool cryptoSymmetricEncrypt(byte *dest, size_t *dest_len, const byte *src, size_t src_len) {
    cloud_connection_t *c = &cloud.connection;

    if (c->send_count == UINT64_MAX) {
        return false;
    }
    if (!cryptoGCMSeal(c->e_ctx, c->send_key, CLOUD_DIR_TO_BACKEND, c->send_count,
            dest, dest_len, src, src_len)) {
        return false;
    }
    c->send_count++;
    return true;
}

/**
 * Opens a frame from the backend, with the connection's receive key and the
 * next nonce in the to-frontend direction.
 *
 * dest:     receives the plaintext, src_len - GCM_TAG_LEN bytes.
 * dest_len: set to its length.
 * src:      the frame's payload.
 * src_len:  its length.
 *
 * Returns true if the frame opened. False means it can't be trusted, and
 * the counts no longer agree, so the connection can't go on.
 *
 * Called from cloudParseFrame() (g_cloud.c).
 */
bool cryptoSymmetricDecrypt(byte *dest, size_t *dest_len, const byte *src, size_t src_len) {
    cloud_connection_t *c = &cloud.connection;

    if (c->recv_count == UINT64_MAX) {
        return false;
    }
    if (!cryptoGCMOpen(c->d_ctx, c->recv_key, CLOUD_DIR_TO_FRONTEND, c->recv_count,
            dest, dest_len, src, src_len)) {
        return false;
    }
    c->recv_count++;
    return true;
}

/**
 * Hash the input using the specified algorithm. We're using
 * SHA256 for now.
 *
 * If you change the digest, make sure to change the length
 * of the DIGEST_LEN macro in g_cloud.h too!!
 *
 * Hashing is what makes the challenge-response work without either side
 * echoing a secret back verbatim: each end hashes the nonce it knows and
 * compares digests, so a match proves the other side decrypted the
 * challenge correctly while the nonce itself never travels in the clear.
 *
 * dest:    buffer to receive the digest. Must be at least DIGEST_LEN
 *          bytes - the length isn't reported back, which is why that
 *          macro has to be kept in step with the algorithm above.
 * src:     the data to hash.
 * src_len: length of src.
 *
 * Returns nothing.
 *
 * Called from the cloud handshake in g_cloud.c - once to hash our own
 * nonce for comparison against the backend's reply, and once to hash the
 * backend's challenge for the answer we send back.
 */
void cryptoMessageDigest(byte *dest, byte *src, size_t src_len) {
    const EVP_MD *md;
    EVP_MD_CTX *ctx;
    unsigned int md_len;

    md = EVP_get_digestbyname("SHA256");
    ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex2(ctx, md, NULL);
    EVP_DigestUpdate(ctx, src, src_len);
    EVP_DigestFinal_ex(ctx, (unsigned char *)dest, &md_len);
    EVP_MD_CTX_free(ctx);
}
