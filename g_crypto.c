#include "g_local.h"

/**
 * Generates an RSA keypair and writes both halves to PEM files in the
 * working directory, named public-<timestamp>.pem and
 * private-<timestamp>.pem.
 *
 * Every server talking to the cloud admin backend needs its own keypair
 * - the private key proves the server's identity during the handshake
 * and decrypts what the backend sends back (see G_PrivateDecrypt()), and
 * the public half is what the backend is given to identify it by. The
 * timestamp in the filenames keeps a new pair from silently overwriting
 * one already in use.
 *
 * bits: key size to generate, e.g. 2048.
 *
 * Returns nothing.
 *
 * Currently unreachable from the mod: nothing calls it. Key generation
 * is done by the standalone `genkeys` tool instead (genkeys.c, built by
 * `make genkeys`), which carries its own copy of this code - so the two
 * are duplicates that can drift apart.
 *
 * Note the file handles aren't checked, so a directory that can't be
 * written to gives a NULL FILE* straight to the PEM writers.
 */
void G_GenerateKeyPair(int bits) {
    FILE *fp;
    RSA *rsa;
    BIGNUM *e;
    time_t seconds;
    char filename[64];

    time(&seconds);

    rsa = RSA_new();
    e = BN_new();

    BN_set_word(e, RSA_F4);
    RSA_generate_key_ex(rsa, bits, e, NULL);

    snprintf(filename, sizeof(filename), "public-%ld.pem", seconds);
    fp = fopen(filename, "wb");
    PEM_write_RSAPublicKey(fp, rsa);
    fclose(fp);

    snprintf(filename, sizeof(filename), "private-%ld.pem", seconds);
    fp = fopen(filename, "wb");
    PEM_write_RSAPrivateKey(fp, rsa, NULL, NULL, 0, NULL, NULL);
    fclose(fp);

    BN_free(e);
    RSA_free(rsa);
}

/**
 * Read public and private keys from filesystem into RSA structures
 *
 * Loads the three keys the cloud connection needs, all from moddir and
 * all named by config settings: this server's own private and public
 * keys, plus the cloud admin backend's public key. Each has a distinct
 * job in the handshake - the backend's public key encrypts challenges
 * only the real backend can read (G_PublicEncrypt()), and this server's
 * private key decrypts what comes back (G_PrivateDecrypt()). Shipping
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
bool G_LoadKeys(void) {
    FILE *fp;
    cloud_connection_t *c = &cloud.connection;
    char path[200];

    gi.cprintf(NULL, PRINT_HIGH, "[cloud] loading encryption keys...");

    // first load our private key
    sprintf(path, "%s/%s", moddir, cloud_config.private);
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
    sprintf(path, "%s/%s", moddir, cloud_config.public);
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
    sprintf(path, "%s/%s", moddir, cloud_config.serverkey);
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
size_t G_PrivateDecrypt(byte *dest, byte *src, int src_len) {
    size_t len = 0;

    EVP_PKEY *key = cloud.connection.private_key;
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(key, NULL);
    if (!ctx) {
        CA_printf("error creating private key context\n");
        return len;
    }

    if (EVP_PKEY_decrypt_init(ctx) <= 0) {
        CA_printf("error initializing decrypt\n");
        return len;
    }

    if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING) <= 0) {
        CA_printf("error adding decryption padding (PKCS1)\n");
        return len;
    }

    if (EVP_PKEY_decrypt(ctx, NULL, &len, (const unsigned char *)src, src_len) <= 0) {
        CA_printf("error getting decrypt size\n");
        return 0;
    }

    byte *newplain = OPENSSL_malloc(len);
    if (!newplain) {
        CA_printf("error mallocing in decrypt\n");
        return 0;
    }

    if (EVP_PKEY_decrypt(ctx, (unsigned char *)newplain, &len, (const unsigned char *)src, src_len) <= 0) {
        CA_printf("error decrypting\n");
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
 *        backend's, loaded by G_LoadKeys().
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
size_t G_PublicEncrypt(EVP_PKEY *key, byte *out, byte *in, size_t inlen) {
    size_t cipherlen = 0;
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(key, NULL);
    if (!ctx) {
        CA_printf("error creating context for encrypting\n");
        return 0;
    }

    if (EVP_PKEY_encrypt_init(ctx) <= 0) {
        CA_printf("encrypt init failed\n");
        return 0;
    }

    if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING) <= 0) {
        CA_printf("error adding padding type (PKCS1)\n");
        return 0;
    }

    if (EVP_PKEY_encrypt(ctx, NULL, &cipherlen, (const unsigned char *)in, inlen) <= 0) {
        CA_printf("encrypt error\n");
        return 0;
    }

    byte *out1 = OPENSSL_malloc(cipherlen);
    if (!out) {
        CA_printf("malloc error while encrypting\n");
    }

    if (EVP_PKEY_encrypt(ctx, (unsigned char *)out1, &cipherlen, (const unsigned char *)in, inlen) <= 0) {
        CA_printf("error encrypting\n");
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
void G_RSAError() {
    int error = 0;
    char *msg = "";
    ERR_load_crypto_strings();
    while ((error = ERR_get_error()) != 0) {
        ERR_error_string(error, msg);
        printf("Error (%d): %s\n", error, msg);
    }
}

/**
 * Helper for printing out binary keys and ciphertext as hex
 *
 * Classic hex-dump format: 16 bytes per line as an offset, the hex
 * bytes, and the printable ASCII alongside. Everything this file handles
 * is binary - keys, nonces, digests, ciphertext - so when a handshake
 * fails the only way to see what was actually on the wire is to dump it
 * and compare the two ends byte for byte.
 *
 * desc: label printed above the dump, or NULL for none.
 * addr: start of the data.
 * len:  how many bytes to dump; zero and negative lengths are reported
 *       rather than walked, since a bad length here usually means a
 *       length calculation went wrong somewhere upstream.
 *
 * Returns nothing; writes to stdout via printf rather than the game's
 * console, so output goes to wherever the server process's stdout is
 * pointed.
 *
 * Currently unreachable: nothing calls it. It's a debugging aid kept
 * around to be dropped in temporarily while working on the protocol.
 */
void hexDump (char *desc, void *addr, int len) {
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char*)addr;

    // Output description if given.
    if (desc != NULL) {
        printf ("%s:\n", desc);
    }
    if (len == 0) {
        printf("  ZERO LENGTH\n");
        return;
    }
    if (len < 0) {
        printf("  NEGATIVE LENGTH: %i\n",len);
        return;
    }

    // Process every byte in the data.
    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 16) == 0) {
            // Just don't print ASCII for the zeroth line.
            if (i != 0) {
                printf("  %s\n", buff);
            }
            printf("  %04x ", i);
        }

        // Now the hex code for the specific character.
        printf(" %02x", pc[i]);

        // And store a printable ASCII character for later.
        if ((pc[i] < 0x20) || (pc[i] > 0x7e)) {
            buff[i % 16] = '.';
        } else {
            buff[i % 16] = pc[i];
        }
        buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0) {
        printf("   ");
        i++;
    }
    printf("  %s\n", buff);
}


/**
 * Encrypt a buffer using 128bit AES
 *
 * This is called automatically from RA_SendMessages() if we're
 * set to encrypt traffic in the config
 *
 * Bulk traffic uses AES rather than RSA because RSA can only encrypt
 * less than a key's worth of data and is far too slow for a per-frame
 * message stream. The session key and IV this uses were handed over
 * during the RSA handshake (see G_PrivateDecrypt()), which is the point
 * of that handshake: establish a shared secret, then switch to fast
 * symmetric crypto for everything after.
 *
 * dest:    buffer to receive the ciphertext. Not bounds checked, and CBC
 *          output can exceed the input by up to a block, so it needs
 *          headroom beyond src_len.
 * src:     the plaintext.
 * src_len: length of src.
 *
 * Returns the ciphertext length, counting both the streamed output and
 * the final padded block.
 *
 * Called from the cloud message pump in g_cloud.c, on each queued
 * outbound message when encryption is enabled.
 *
 * Two things to be aware of: the guard is written `!(c || c->e_ctx)`,
 * and since c is the address of a static struct and so never NULL, it's
 * always false - a missing cipher context isn't actually caught. And the
 * cipher is re-initialised with the same IV for every message rather
 * than a fresh one, which under CBC means identical plaintext encrypts
 * to identical ciphertext across messages.
 */
size_t G_SymmetricEncrypt(byte *dest, byte *src, size_t src_len) {
    cloud_connection_t *c = &cloud.connection;
    int dest_len = 0;
    int written = 0;

    if (!(c || c->e_ctx)) {
        return 0;
    }

    EVP_EncryptInit_ex(c->e_ctx, EVP_aes_128_cbc(), NULL, (const unsigned char *)c->session_key, (const unsigned char *)c->initial_value);
    EVP_EncryptUpdate(c->e_ctx, (unsigned char *)(dest + dest_len), &dest_len, (const unsigned char *)src, src_len);
    written += dest_len;

    EVP_EncryptFinal_ex(c->e_ctx, (unsigned char *)(dest + dest_len), &dest_len);
    written += dest_len;

    return written;
}

/**
 * Decrypt a buffer using 128 bit AES
 *
 * Called automatically from RA_ReadMessages() if we're
 * set to encrypt traffic in the config
 *
 * The inbound counterpart to G_SymmetricEncrypt(), using the same
 * session key and IV agreed during the handshake, so anything the
 * backend sends over the established channel is readable only by this
 * server.
 *
 * dest:    buffer to receive the plaintext. Not bounds checked.
 * src:     the ciphertext.
 * src_len: length of src.
 *
 * Returns the plaintext length, counting both the streamed output and
 * the final block.
 *
 * Called from the cloud message pump in g_cloud.c, on each inbound
 * message when encryption is enabled.
 *
 * Carries the same two caveats as the encrypt side: the `!(c || c->d_ctx)`
 * guard can never fire, and the fixed IV is reused for every message.
 * Note also that a failure of the final block - which is where CBC
 * padding is validated, and so where tampered or truncated ciphertext
 * would be caught - isn't checked, so a corrupt message is reported the
 * same as a good one.
 */
size_t G_SymmetricDecrypt(byte *dest, byte *src, size_t src_len) {
    cloud_connection_t *c = &cloud.connection;
    int dest_len = 0;
    int written = 0;

    if (!(c || c->d_ctx)) {
        return 0;
    }
    EVP_DecryptInit_ex(c->d_ctx, EVP_aes_128_cbc(), NULL, (const unsigned char *)c->session_key, (const unsigned char *)c->initial_value);
    EVP_DecryptUpdate(c->d_ctx, (unsigned char *)(dest + dest_len), &dest_len, (const unsigned char *)src, src_len);
    written += dest_len;

    EVP_DecryptFinal_ex(c->d_ctx, (unsigned char *)(dest + dest_len), &dest_len);
    written += dest_len;

    return written;
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
void G_MessageDigest(byte *dest, byte *src, size_t src_len) {
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
