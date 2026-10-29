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

/**
 * Onboards a game server to Cloud Admin: generates the key pair, fetches
 * the Cloud Admin server's public key, and writes a q2a_cloud.cfg filled
 * in with the answers. Everything lands in the current directory, for the
 * operator to copy into their mod directory.
 *
 * On the key encoding: the keys are RSA, because the handshake encrypts a
 * challenge with the peer's public key and only RSA can do that. But they
 * are written with PEM_write_PrivateKey()/PEM_write_PUBKEY() rather than
 * the RSA-specific PEM_write_RSAPrivateKey()/PEM_write_RSAPublicKey(), so
 * the files are PKCS#8 and SubjectPublicKeyInfo:
 *
 *     -----BEGIN PRIVATE KEY-----      not  BEGIN RSA PRIVATE KEY
 *     -----BEGIN PUBLIC KEY-----       not  BEGIN RSA PUBLIC KEY
 *
 * cryptoLoadKeys() already reads both with the generic PEM_read_PrivateKey
 * and PEM_read_PUBKEY, so nothing in the game library needs changing.
 *
 * Build: make cloudsetup
 * Usage: cloudsetup        (asks everything, writes into the cwd)
 */

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <curl/curl.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

// 2048 bits, because g_cloud.h's RSA_LEN is 256 bytes and the handshake
// sizes its buffers from it. Not a question to ask the operator: a different
// size builds a config that cannot complete a handshake.
#define KEY_BITS            2048

#define DEFAULT_KEY_NAME    "q2admin"
#define DEFAULT_KEY_URL     "https://cloud.q2admin.net/public-server.pem"
#define DEFAULT_SERVER_KEY  "server.pem"
#define DEFAULT_ADDRESS     "cloud.q2admin.net"
#define DEFAULT_PORT        "9988"
#define DEFAULT_TELEPORT    "!goto"
#define DEFAULT_INVITE      "!invite"
#define CONFIG_FILE         "q2a_cloud.cfg"
#define WEBSITE             "https://cloud.q2admin.net"

#define MAXINPUT            256

/**
 * Asks a question, offering a default the operator can take by pressing
 * enter. The answer is trimmed of the trailing newline and of surrounding
 * spaces, since a pasted value often brings one along.
 */
static void prompt(const char *question, const char *def, char *out, size_t size) {
    char line[MAXINPUT];
    char *start, *end;

    for (;;) {
        if (def && *def) {
            printf("%s [%s]: ", question, def);
        } else {
            printf("%s: ", question);
        }
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            // EOF, so there is nobody to answer the rest of the questions.
            printf("\n");
            exit(1);
        }

        start = line;
        while (*start == ' ' || *start == '\t') {
            start++;
        }
        end = start + strlen(start);
        while (end > start && (end[-1] == '\n' || end[-1] == '\r'
                               || end[-1] == ' ' || end[-1] == '\t')) {
            end--;
        }
        *end = 0;

        if (*start) {
            snprintf(out, size, "%s", start);
            return;
        }
        if (def) {
            snprintf(out, size, "%s", def);
            return;
        }
        printf("  this one has no default, please answer it\n");
    }
}

/**
 * Asks a yes/no question. Returns true for yes.
 */
static bool promptYesNo(const char *question, bool def) {
    char answer[MAXINPUT];

    for (;;) {
        prompt(question, def ? "yes" : "no", answer, sizeof(answer));
        if (!strcasecmp(answer, "yes") || !strcasecmp(answer, "y")) {
            return true;
        }
        if (!strcasecmp(answer, "no") || !strcasecmp(answer, "n")) {
            return false;
        }
        printf("  please answer yes or no\n");
    }
}

/**
 * Whether it is alright to write path, asking first if something is already
 * there. Overwriting a private key that a server is using would cut that
 * server off from Cloud Admin, so the default is to keep it.
 */
static bool mayWrite(const char *path) {
    char question[MAXINPUT + 64];

    if (access(path, F_OK) != 0) {
        return true;
    }
    snprintf(question, sizeof(question), "  %s already exists, overwrite it?", path);
    return promptYesNo(question, false);
}

/**
 * Generates the RSA pair and writes both halves.
 *
 * privPath gets mode 0600: it is written with umask applied by fopen, then
 * tightened, which leaves a brief window where it could be read. Acceptable
 * for a setup tool run by hand in a directory the operator owns, and the
 * alternative needs open()/fdopen() plumbing for no real gain here.
 */
static bool generateKeyPair(const char *privPath, const char *pubPath) {
    EVP_PKEY *key = NULL;
    FILE *fp;
    bool ok = false;

    printf("  generating a %d bit RSA key, this can take a few seconds\n", KEY_BITS);

    key = EVP_RSA_gen(KEY_BITS);
    if (!key) {
        fprintf(stderr, "  could not generate the key\n");
        ERR_print_errors_fp(stderr);
        return false;
    }

    fp = fopen(privPath, "wb");
    if (!fp) {
        fprintf(stderr, "  %s: %s\n", privPath, strerror(errno));
        goto out;
    }
    if (!PEM_write_PrivateKey(fp, key, NULL, NULL, 0, NULL, NULL)) {
        fprintf(stderr, "  could not write %s\n", privPath);
        ERR_print_errors_fp(stderr);
        fclose(fp);
        goto out;
    }
    fclose(fp);
    if (chmod(privPath, 0600) != 0) {
        fprintf(stderr, "  warning: could not restrict %s: %s\n",
                privPath, strerror(errno));
    }

    fp = fopen(pubPath, "wb");
    if (!fp) {
        fprintf(stderr, "  %s: %s\n", pubPath, strerror(errno));
        goto out;
    }
    if (!PEM_write_PUBKEY(fp, key)) {
        fprintf(stderr, "  could not write %s\n", pubPath);
        ERR_print_errors_fp(stderr);
        fclose(fp);
        goto out;
    }
    fclose(fp);

    printf("  wrote %s (private, mode 0600) and %s (public)\n", privPath, pubPath);
    ok = true;

out:
    EVP_PKEY_free(key);
    return ok;
}

static size_t writeToFile(void *data, size_t size, size_t count, void *fp) {
    return fwrite(data, size, count, (FILE *)fp);
}

/**
 * Points curl at a CA store so the HTTPS download can be verified.
 *
 * The bundled libcurl is built against the bundled OpenSSL, which does not
 * read the macOS keychain and has no bundle compiled in, so verification
 * fails everywhere until it is told where to look. g_http.c solves this
 * with the http_cacert_path setting; a standalone tool has no config file
 * yet, so it tries the usual locations instead.
 *
 * CURL_CA_BUNDLE in the environment wins: libcurl reads it itself, so if it
 * is set this leaves well alone.
 */
static void useSystemCertificates(CURL *curl) {
    // Bundle files first; a single file is what most systems now ship.
    static const char *files[] = {
        "/etc/ssl/cert.pem",                            // macOS, FreeBSD
        "/etc/ssl/certs/ca-certificates.crt",            // Debian, Ubuntu
        "/etc/pki/tls/certs/ca-bundle.crt",              // RHEL, Fedora
        "/etc/ssl/ca-bundle.pem",                        // openSUSE
        "/opt/homebrew/etc/ca-certificates/cert.pem",    // Homebrew
        "/usr/local/etc/openssl@3/cert.pem",
        NULL
    };
    static const char *dirs[] = { "/etc/ssl/certs", NULL };
    int i;

    if (getenv("CURL_CA_BUNDLE")) {
        return;
    }
    for (i = 0; files[i]; i++) {
        if (access(files[i], R_OK) == 0) {
            curl_easy_setopt(curl, CURLOPT_CAINFO, files[i]);
            return;
        }
    }
    for (i = 0; dirs[i]; i++) {
        if (access(dirs[i], R_OK) == 0) {
            curl_easy_setopt(curl, CURLOPT_CAPATH, dirs[i]);
            return;
        }
    }
    // Nothing found, so leave curl's own default and let it report.
}

/**
 * Fetches the Cloud Admin server's public key over HTTPS.
 *
 * Certificate verification is left on. This downloads something the server
 * is then trusted to authenticate itself with, so fetching it over a
 * connection nobody checked would undo the point of having a key at all.
 * If verification fails the error says so rather than retrying without it.
 *
 * The result has to parse as a public key before it is kept: a captive
 * portal or a 404 page will download perfectly happily otherwise, and the
 * failure would not surface until the first handshake.
 */
static bool downloadServerKey(const char *url, const char *path) {
    CURL *curl;
    CURLcode res;
    FILE *fp;
    EVP_PKEY *check;
    long status = 0;

    curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "  could not initialise curl\n");
        return false;
    }

    fp = fopen(path, "wb");
    if (!fp) {
        fprintf(stderr, "  %s: %s\n", path, strerror(errno));
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToFile);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    useSystemCertificates(curl);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "q2admin-cloudsetup");

    res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    fclose(fp);

    if (res != CURLE_OK) {
        fprintf(stderr, "  download failed: %s\n", curl_easy_strerror(res));
        if (res == CURLE_PEER_FAILED_VERIFICATION || res == CURLE_SSL_CACERT) {
            fprintf(stderr, "  the certificate could not be verified. Point"
                            " CURL_CA_BUNDLE at a CA bundle and try again,"
                            " or fetch the file by hand.\n");
        }
        curl_easy_cleanup(curl);
        unlink(path);
        return false;
    }
    curl_easy_cleanup(curl);

    fp = fopen(path, "rb");
    check = fp ? PEM_read_PUBKEY(fp, NULL, NULL, NULL) : NULL;
    if (fp) {
        fclose(fp);
    }
    if (!check) {
        fprintf(stderr, "  %s downloaded (HTTP %ld) but is not a PEM public"
                        " key, so it has not been kept\n", path, status);
        ERR_clear_error();
        unlink(path);
        return false;
    }
    EVP_PKEY_free(check);

    printf("  wrote %s\n", path);
    return true;
}

/**
 * Writes q2a_cloud.cfg. Keeps the shipped file's comments, so the result
 * reads the same as the one in config/ and an operator editing it later
 * sees the same explanations.
 */
static bool writeConfig(const char *path, const char *address, const char *port,
                        bool encryption, const char *privKey, const char *pubKey,
                        const char *serverKey, const char *uuid, const char *dns,
                        const char *teleport, const char *invite) {
    FILE *fp = fopen(path, "wb");

    if (!fp) {
        fprintf(stderr, "  %s: %s\n", path, strerror(errno));
        return false;
    }

    fprintf(fp,
        ";\n"
        "; Cloud Admin settings, written by cloudsetup.\n"
        ";\n"
        "\n"
        "; Utilize an admin server\n"
        "cloud_enabled \"yes\"\n"
        "\n"
        "; Admin server address. IPv4, IPv6, DNS name\n"
        "cloud_address \"%s\"\n"
        "\n"
        "; Admin server port, default is 9988\n"
        "cloud_port \"%s\"\n"
        "\n"
        "; Use AES encryption for data to/from admin server\n"
        "cloud_encryption \"%s\"\n"
        "\n"
        "; Our private key (don't share with anyone)\n"
        "cloud_privatekey \"%s\"\n"
        "\n"
        "; Our public key (give to the cloud admin server)\n"
        "cloud_publickey \"%s\"\n"
        "\n"
        "; The server's public key\n"
        "cloud_serverkey \"%s\"\n"
        "\n"
        "; Identifies this game server with the admin server.\n"
        "; Get this from the admin server website\n"
        "cloud_uuid \"%s\"\n"
        "\n"
        "; Data to send to admin server, add up values.\n"
        "; Default of 4095 is future proofing for everything\n"
        ";    1 = frags\n"
        ";    2 = player chat\n"
        ";    4 = teleport commands from players\n"
        ";    8 = invite commands from players\n"
        ";   16 = find commands from players\n"
        ";   32 = whois commands from players\n"
        "; 1024 = debug messages\n"
        "cloud_flags \"4095\"\n"
        "\n"
        "; Use ipv6 first then ipv4\n"
        "; 46 would prefer ipv4 first\n"
        "; 4 for only ipv4\n"
        "; 6 for only ipv6\n"
        "cloud_dns \"%s\"\n"
        "\n"
        "; The command players use for teleporting, 24 chars max\n"
        "cloud_cmd_teleport \"%s\"\n"
        "\n"
        "; The command players use for inviting, 24 chars max\n"
        "cloud_cmd_invite \"%s\"\n"
        "\n"
        "; The command players use for looking up aliases, 24 chars max\n"
        "cloud_cmd_whois \"!whois\"\n"
        "\n"
        "; The command players use for finding other players, 24 chars max\n"
        "cloud_cmd_seen \"!find\"\n",
        address, port, encryption ? "yes" : "no", privKey, pubKey, serverKey,
        uuid, dns, teleport, invite);

    if (ferror(fp)) {
        fprintf(stderr, "  could not write %s\n", path);
        fclose(fp);
        return false;
    }
    fclose(fp);
    printf("  wrote %s\n", path);
    return true;
}

/**
 * Warns when the Server ID does not look like the UUID the website hands
 * out. Only a warning: a wrong one is not rejected until the first
 * handshake, which is a slow way to find a typo, but the format is the
 * website's to decide and not worth refusing over.
 */
static void checkUuid(const char *uuid) {
    static const int dashes[] = { 8, 13, 18, 23 };
    size_t i, d;

    if (strlen(uuid) != 36) {
        printf("  note: that is %zu characters, a Server ID is usually 36\n",
               strlen(uuid));
        return;
    }
    for (d = 0; d < sizeof(dashes) / sizeof(dashes[0]); d++) {
        if (uuid[dashes[d]] != '-') {
            printf("  note: that does not look like a UUID, check it is the"
                   " Server ID and not something else\n");
            return;
        }
    }
    for (i = 0; i < 36; i++) {
        if (uuid[i] == '-') {
            continue;
        }
        if (!isxdigit((unsigned char)uuid[i])) {
            printf("  note: that has characters a UUID would not, check it\n");
            return;
        }
    }
}

int main(void) {
    char keyName[MAXINPUT], privKey[MAXINPUT + 16], pubKey[MAXINPUT + 16];
    char keyUrl[MAXINPUT], serverKey[MAXINPUT];
    char uuid[MAXINPUT], address[MAXINPUT], port[MAXINPUT];
    char dns[MAXINPUT], teleport[MAXINPUT], invite[MAXINPUT];
    bool encryption;

    printf("\nq2admin Cloud Admin setup\n"
           "=========================\n"
           "\n"
           "Everything is written into the current directory. Press enter to\n"
           "take the default shown in brackets.\n");

    // --- 1. our key pair ---
    printf("\n1. Key pair\n\n");
    prompt("   Key file base name", DEFAULT_KEY_NAME, keyName, sizeof(keyName));
    snprintf(privKey, sizeof(privKey), "%s-private.pem", keyName);
    snprintf(pubKey, sizeof(pubKey), "%s-public.pem", keyName);

    if (!mayWrite(privKey) || !mayWrite(pubKey)) {
        printf("\n   keeping the existing keys, nothing written. Re-run with a"
               " different base name to make a new pair.\n");
        return 1;
    }
    if (!generateKeyPair(privKey, pubKey)) {
        return 1;
    }

    // --- 2. the Cloud Admin server's public key ---
    printf("\n2. Cloud Admin server public key\n\n");
    prompt("   URL to download it from", DEFAULT_KEY_URL, keyUrl, sizeof(keyUrl));
    snprintf(serverKey, sizeof(serverKey), "%s", DEFAULT_SERVER_KEY);
    if (!mayWrite(serverKey)) {
        printf("\n   keeping the existing %s\n", serverKey);
    } else if (!downloadServerKey(keyUrl, serverKey)) {
        printf("\n   Without the server's public key q2admin cannot verify the"
               " Cloud Admin\n   server. Download %s by hand, save it as %s"
               " here, and re-run.\n", keyUrl, serverKey);
        return 1;
    }

    // --- 3. the Server ID ---
    printf("\n3. Server ID\n"
           "\n"
           "   Go to %s, log in, create a new server,\n"
           "   and copy the Server ID from its settings page.\n"
           "\n", WEBSITE);
    prompt("   Server ID", NULL, uuid, sizeof(uuid));
    checkUuid(uuid);

    // --- 4. the rest of the config ---
    printf("\n4. Connection settings\n\n");
    prompt("   Cloud Admin address", DEFAULT_ADDRESS, address, sizeof(address));
    prompt("   Cloud Admin port", DEFAULT_PORT, port, sizeof(port));
    encryption = promptYesNo("   Encrypt traffic to the Cloud Admin server?", true);
    prompt("   Our private key file", privKey, privKey, sizeof(privKey));
    prompt("   Our public key file", pubKey, pubKey, sizeof(pubKey));
    prompt("   Cloud Admin server's public key file", serverKey, serverKey,
           sizeof(serverKey));

    for (;;) {
        prompt("   DNS resolution: 64 = IPv6 then IPv4, 6 = IPv6 only,"
               " 4 = IPv4 only", "64", dns, sizeof(dns));
        if (!strcmp(dns, "64") || !strcmp(dns, "46")
                || !strcmp(dns, "6") || !strcmp(dns, "4")) {
            break;
        }
        printf("  please answer 64, 46, 6 or 4\n");
    }

    prompt("   Teleport command", DEFAULT_TELEPORT, teleport, sizeof(teleport));
    prompt("   Invite command", DEFAULT_INVITE, invite, sizeof(invite));

    // --- 5. write it out ---
    printf("\n5. Writing the config\n\n");
    if (!mayWrite(CONFIG_FILE)) {
        printf("\n   keeping the existing %s. The keys are still written.\n",
               CONFIG_FILE);
        return 1;
    }
    if (!writeConfig(CONFIG_FILE, address, port, encryption, privKey, pubKey,
                     serverKey, uuid, dns, teleport, invite)) {
        return 1;
    }

    printf("\nDone. Copy these into your q2 mod directory, next to the other\n"
           "q2a_*.cfg files:\n"
           "\n"
           "    %s\n"
           "    %s\n"
           "    %s\n"
           "    %s\n"
           "\n"
           "Keep %s to yourself; give %s to the Cloud Admin\n"
           "server through its website if it asks for it.\n",
           CONFIG_FILE, privKey, pubKey, serverKey, privKey, pubKey);

    return 0;
}
