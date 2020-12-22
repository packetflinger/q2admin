#include "g_local.h"

void G_GenerateKeyPair(void)
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
	RSA_generate_key_ex(rsa, 2048, e, NULL);

	sprintf(filename, "public-%d.pem", seconds);
	fp = fopen(filename, "wb");
	PEM_write_RSAPublicKey(fp, rsa);
	fclose(fp);

	sprintf(filename, "private-%d.pem", seconds);
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
	//ra_connection_t *c = &remote.connection;
	char path[200];

	gi.cprintf(NULL, PRINT_HIGH, "[RA] Loading encryption keys...");

	// first load our private key
	sprintf(path, "%s/%s", gamedir->string, remotePrivateKey);
	fp = fopen(path, "rb");
	if (!fp) {
		gi.cprintf(NULL, PRINT_HIGH, "failed, %s not found\n", path);
		return false;
	}
	remote.connection.rsa_pr = RSA_new();
	remote.connection.rsa_pr = PEM_read_RSAPrivateKey(fp, &remote.connection.rsa_pr, NULL, NULL);
	fclose(fp);

	// then our public key
	sprintf(path, "%s/%s", gamedir->string, remotePublicKey);
	fp = fopen(path, "rb");
	if (!fp) {
		gi.cprintf(NULL, PRINT_HIGH, "failed, %s not found\n", path);
		RSA_free(remote.connection.rsa_pr);
		return false;
	}
	remote.connection.rsa_pu = RSA_new();
	remote.connection.rsa_pu = PEM_read_RSA_PUBKEY(fp, &remote.connection.rsa_pu, NULL, NULL);
	fclose(fp);

	// last the remote admin server's public key
	sprintf(path, "%s/%s", gamedir->string, remoteServerPublicKey);
	fp = fopen(path, "rb");
	if (!fp) {
		gi.cprintf(NULL, PRINT_HIGH, "failed, %s not found\n", path);
		RSA_free(remote.connection.rsa_pr);
		RSA_free(remote.connection.rsa_pu);
		return false;
	}
	remote.connection.rsa_sv_pu = RSA_new();
	remote.connection.rsa_sv_pu = PEM_read_RSA_PUBKEY(fp, &remote.connection.rsa_sv_pu, NULL, NULL);
	fclose(fp);

	gi.cprintf(NULL, PRINT_HIGH, "OK\n");

	return true;
}

void G_PublicDecrypt(RSA *key, byte *dest, byte *src)
{
	int result;
	result = RSA_public_decrypt(256, src, dest, key, RSA_PKCS1_PADDING);
}

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
