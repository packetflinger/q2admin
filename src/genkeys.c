#include <openssl/rsa.h>
#include <openssl/bn.h>
#include <openssl/pem.h>

void GenerateKeyPair(int bits)
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

int main(int argc, char **argv) {
	GenerateKeyPair(2048);
	return 0;
}
