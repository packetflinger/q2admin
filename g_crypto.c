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
	ra_connection_t *c = &remote.connection;
	char path[200];

	gi.cprintf(NULL, PRINT_HIGH, "[RA] Loading encryption keys...");

	// first load our private key
	sprintf(path, "%s/%s", gamedir->string, remotePrivateKey);
	fp = fopen(path, "rb");
	if (!fp) {
		gi.cprintf(NULL, PRINT_HIGH, "failed, %s not found\n", path);
		return false;
	}
	c->rsa_pr = RSA_new();
	c->rsa_pr = PEM_read_RSAPrivateKey(fp, &c->rsa_pr, NULL, NULL);
	fclose(fp);

	if (!c->rsa_pr) {
	    gi.cprintf(NULL, PRINT_HIGH, "failed, problems with your private key\n");
	    return false;
	}


	// then our public key
	sprintf(path, "%s/%s", gamedir->string, remotePublicKey);
	fp = fopen(path, "rb");
	if (!fp) {
		gi.cprintf(NULL, PRINT_HIGH, "failed, %s not found\n", path);
		RSA_free(c->rsa_pr);
		return false;
	}
	c->rsa_pu = RSA_new();
	c->rsa_pu = PEM_read_RSAPublicKey(fp, &c->rsa_pu, NULL, NULL);
	fclose(fp);

	if (!c->rsa_pu) {
        gi.cprintf(NULL, PRINT_HIGH, "failed, problems with your public key\n");
        RSA_free(c->rsa_pr);
        return false;
    }

	// last the remote admin server's public key
	sprintf(path, "%s/%s", gamedir->string, remoteServerPublicKey);
	fp = fopen(path, "rb");
	if (!fp) {
		gi.cprintf(NULL, PRINT_HIGH, "failed, %s not found\n", path);
		RSA_free(c->rsa_pr);
		RSA_free(c->rsa_pu);
		return false;
	}
	c->rsa_sv_pu = RSA_new();
	c->rsa_sv_pu = PEM_read_RSAPublicKey(fp, &c->rsa_sv_pu, NULL, NULL);
	fclose(fp);

	if (!c->rsa_sv_pu) {
        gi.cprintf(NULL, PRINT_HIGH, "failed, problems with the q2admin server's public key\n");
        RSA_free(c->rsa_pr);
        RSA_free(c->rsa_pu);
        return false;
    }

	gi.cprintf(NULL, PRINT_HIGH, "OK\n");

	return true;
}

/**
 * Wrapper
 */
void G_PublicDecrypt(RSA *key, byte *dest, byte *src)
{
	int result;
	result = RSA_public_decrypt(RSA_size(key), src, dest, key, RSA_PKCS1_PADDING);
}

/**
 * Wrapper
 */
void G_PrivateEncrypt(RSA *key, byte *dest, byte *src)
{
    int result;
    result = RSA_private_encrypt(RSA_size(key), src, dest, key, RSA_PKCS1_PADDING);
}

/**
 *
 */
void G_RSAError()
{
	int error = 0;
	char *msg;

	//SSL_load_error_strings();
	ERR_load_crypto_strings();

	while ((error = ERR_get_error()) != 0) {
		ERR_error_string(error, msg);
		printf("Error (%d): %s\n", error, msg);
	}
}
