#include "g_local.h"

/**
 *
 */
void G_GenerateKeyPair(int bits)
{
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

    snprintf(filename, sizeof(filename), "public-%d.pem", seconds);
    fp = fopen(filename, "wb");
    PEM_write_RSAPublicKey(fp, rsa);
    fclose(fp);

    snprintf(filename, sizeof(filename), "private-%d.pem", seconds);
    fp = fopen(filename, "wb");
    PEM_write_RSAPrivateKey(fp, rsa, NULL, NULL, 0, NULL, NULL);
    fclose(fp);

    BN_free(e);
    RSA_free(rsa);
}

/**
 * Read public and private keys from filesystem into RSA structures
 */
qboolean G_LoadKeys(void)
{
    FILE *fp;
    ca_connection_t *c = &cloud.connection;
    char path[200];

    gi.cprintf(NULL, PRINT_HIGH, "[cloud] loading encryption keys...");

    // first load our private key
    sprintf(path, "%s/%s", moddir, cloud_privatekey);
    fp = fopen(path, "rb");
    if (!fp) {
        gi.cprintf(NULL, PRINT_HIGH, "failed, %s not found\n", path);
        return qfalse;
    }

    c->rsa_pr = PEM_read_PrivateKey(fp, NULL, NULL, NULL);
    fclose(fp);

    if (!c->rsa_pr) {
        gi.cprintf(NULL, PRINT_HIGH, "failed, problems with your private key: %s\n", path);
        return qfalse;
    }

    // then our public key
    sprintf(path, "%s/%s", moddir, cloud_publickey);
    fp = fopen(path, "rb");
    if (!fp) {
        gi.cprintf(NULL, PRINT_HIGH, "failed, %s not found\n", path);
        RSA_free(c->rsa_pr);
        return qfalse;
    }

    c->public_key = PEM_read_PUBKEY(fp, NULL, NULL, NULL);

    fclose(fp);

    if (!c->public_key) {
        gi.cprintf(NULL, PRINT_HIGH, "failed, problems with your public key: %s\n", path);
        RSA_free(c->rsa_pr);
        return qfalse;
    }

    // last the cloud admin server's public key
    sprintf(path, "%s/%s", moddir, cloud_serverkey);
    fp = fopen(path, "rb");
    if (!fp) {
        gi.cprintf(NULL, PRINT_HIGH, "failed, %s not found\n", path);
        RSA_free(c->rsa_pr);
        RSA_free(c->public_key);
        return qfalse;
    }

    c->rsa_sv_pu = PEM_read_PUBKEY(fp, NULL, NULL, NULL);
    fclose(fp);

    if (!c->rsa_sv_pu) {
        gi.cprintf(NULL, PRINT_HIGH, "failed, problems with the q2admin server's public key\n");
        RSA_free(c->rsa_pr);
        RSA_free(c->public_key);
        return qfalse;
    }

    gi.cprintf(NULL, PRINT_HIGH, "OK\n");

    return qtrue;
}

/**
 * Decrypt src using our private key
 */

size_t G_PrivateDecrypt(byte *dest, byte *src, int src_len)
{
    size_t len = 0;

    EVP_PKEY *key = cloud.connection.rsa_pr;
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

    if (EVP_PKEY_decrypt(ctx, NULL, &len, src, src_len) <= 0) {
        CA_printf("error getting decrypt size\n");
        return 0;
    }

    byte *newplain = OPENSSL_malloc(len);

    if (!newplain) {
        CA_printf("error mallocing in decrypt\n");
        return 0;
    }

    if (EVP_PKEY_decrypt(ctx, newplain, &len, src, src_len) <= 0) {
        CA_printf("error decrypting\n");
        return 0;
    }

    memcpy(dest, newplain, len);

    return len;
}

/**
 * Encrypt a message using a public key. ONLY the matching private key
 * can decrypt the message.
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

    if (EVP_PKEY_encrypt(ctx, NULL, &cipherlen, in, inlen) <= 0) {
        CA_printf("encrypt error\n");
        return 0;
    }

    byte *out1 = OPENSSL_malloc(cipherlen);

    if (!out) {
        CA_printf("malloc error while encrypting\n");
    }

    if (EVP_PKEY_encrypt(ctx, out1, &cipherlen, in, inlen) <= 0) {
        CA_printf("error encrypting\n");
    }

    memcpy(out, out1, cipherlen);
    OPENSSL_free(out1);

    return cipherlen;
}

/**
 *
 */
void G_RSAError()
{
    int error = 0;
    char *msg;

    ERR_load_crypto_strings();

    while ((error = ERR_get_error()) != 0) {
        ERR_error_string(error, msg);
        printf("Error (%d): %s\n", error, msg);
    }
}

/**
 * Helper for printing out binary keys and ciphertext as hex
 */
void hexDump (char *desc, void *addr, int len)
{
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char*)addr;

    // Output description if given.
    if (desc != NULL)
        printf ("%s:\n", desc);

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
            if (i != 0)
                printf ("  %s\n", buff);

            // Output the offset.
            printf ("  %04x ", i);
        }

        // Now the hex code for the specific character.
        printf (" %02x", pc[i]);

        // And store a printable ASCII character for later.
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))
            buff[i % 16] = '.';
        else
            buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0) {
        printf ("   ");
        i++;
    }

    // And print the final ASCII bit.
    printf ("  %s\n", buff);
}


/**
 * Encrypt a buffer using 128bit AES
 *
 * This is called automatically from RA_SendMessages() if we're
 * set to encrypt traffic in the config
 */
size_t G_SymmetricEncrypt(byte *dest, byte *src, size_t src_len)
{
    ca_connection_t *c = &cloud.connection;
    int dest_len = 0;
    int written = 0;

    if (!(c || c->e_ctx)) {
        return 0;
    }

    EVP_EncryptInit_ex(c->e_ctx, EVP_aes_128_cbc(), NULL, c->aeskey, c->iv);
    EVP_EncryptUpdate(c->e_ctx, dest + dest_len, &dest_len, src, src_len);
    written += dest_len;

    EVP_EncryptFinal_ex(c->e_ctx, dest + dest_len, &dest_len);
    written += dest_len;

    return written;
}

/**
 * Decrypt a buffer using 128 bit AES
 *
 * Called automatically from RA_ReadMessages() if we're
 * set to encrypt traffic in the config
 */
size_t G_SymmetricDecrypt(byte *dest, byte *src, size_t src_len)
{
    ca_connection_t *c = &cloud.connection;
    int dest_len = 0;
    int written = 0;

    if (!(c || c->d_ctx)) {
        return 0;
    }

    EVP_DecryptInit_ex(c->d_ctx, EVP_aes_128_cbc(), NULL, c->aeskey, c->iv);
    EVP_DecryptUpdate(c->d_ctx, dest + dest_len, &dest_len, src, src_len);
    written += dest_len;

    EVP_DecryptFinal_ex(c->d_ctx, dest + dest_len, &dest_len);
    written += dest_len;

    return written;
}

/**
 * Hash the input using the specified algorithm. We're using
 * SHA256 for now.
 *
 * If you change the digest, make sure to change the length
 * of the DIGEST_LEN macro in g_cloud.h too!!
 */
void G_MessageDigest(byte *dest, byte *src, size_t src_len) {
    EVP_MD *md;
    EVP_MD_CTX *ctx;
    unsigned int md_len;

    md = EVP_get_digestbyname("SHA256");
    ctx = EVP_MD_CTX_new();

    EVP_DigestInit_ex2(ctx, md, NULL);
    EVP_DigestUpdate(ctx, src, src_len);
    EVP_DigestFinal_ex(ctx, dest, &md_len);

    EVP_MD_CTX_free(ctx);
}
