#include "g_local.h"
#include <openssl/bn.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <time.h>

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
	ra_connection_t *c = &remote.connection;

	// first load our private key
	fp = fopen(remotePrivateKey, "rb");
	if (!fp) {
		return false;
	}
	c->rsa_pr = RSA_new();
	c->rsa_pr = PEM_read_RSAPrivateKey(fp, &c->rsa_pr, NULL, NULL);
	fclose(fp);

	// then our public key
	fp = fopen(remotePublicKey, "rb");
	if (!fp) {
		RSA_free(c->rsa_pr);
		return false;
	}
	c->rsa_pu = RSA_new();
	c->rsa_pu = PEM_read_RSA_PUBKEY(fp, &c->rsa_pu, NULL, NULL);
	fclose(fp);

	// last the remote admin server's public key
	fp = fopen(remoteServerPublicKey, "rb");
	if (!fp) {
		RSA_free(c->rsa_pr);
		RSA_free(c->rsa_pu);
		return false;
	}
	c->rsa_sv_pu = RSA_new();
	c->rsa_sv_pu = PEM_read_RSA_PUBKEY(fp, &c->rsa_sv_pu, NULL, NULL);
	fclose(fp);

	return true;
}
