#ifndef G_REMOTE_H
#define G_REMOTE_H

#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <fcntl.h>
#include <openssl/rsa.h>
#include <openssl/bn.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/err.h>

#define CLOUDFILE       "q2a_cloud.cfg"
#define MAGIC_CLIENT    (('C' << 24) + ('A' << 16) + ('2' << 8) + 'Q')
#define MAGIC_PEER      (('P' << 24) + ('A' << 16) + ('2' << 8) + 'Q')

#define NS_INT16SZ      2
#define NS_INADDRSZ     4
#define NS_IN6ADDRSZ    16

#define QUEUE_SIZE      0x55FF

#define CA_FRAME        (cloud.frame_number)

// arg in seconds
#define FUTURE_CA_FRAME(t) (CA_FRAME + SECS_TO_FRAMES(t))

#define CLOUDCMD_LAYOUT "sv !cloud <status|(dis|en)able|rule>"


// Cloud Admin flags, enabled features
#define CFL_FRAGS       BIT(0)
#define CFL_CHAT        BIT(1)
#define CFL_TELEPORT    BIT(2)
#define CFL_INVITE      BIT(3)
#define CFL_FIND        BIT(4)
#define CFL_WHOIS       BIT(5)
#define CFL_DEBUG       BIT(11)      // 1024
#define CFL(f)          ((cloud.flags & CFL_##f) != 0)

#define MAX_MSG_LEN     1000

#define PING_FREQ_SECS  40
#define PING_MISS_MAX   3

#define RSA_LEN         256          // bytes, 2048 bits
#define CHALLENGE_LEN   16           // bytes
#define AESKEY_LEN      16           // bytes, 128 bits
#define AESBLOCK_LEN    16           // bytes, 128 bits
#define AES_IV_LEN      16
#define AUTH_FAIL_LIMIT 3            // stop trying after
#define DIGEST_LEN      (SHA256_DIGEST_LENGTH)

#define TELE_NAME_MAX   20


/**
 * The various states of the cloud admin connection. A normal connection will
 * hang out in the TRUSTED state most of the time (unless disabled).
 */
typedef enum {
    CLOUD_STATE_DISABLED,            // not using CA at all
    CLOUD_STATE_DISCONNECTED,        // will try to connect when possible
    CLOUD_STATE_CONNECTING,          // mid connection
    CLOUD_STATE_CONNECTED,           // connected, but still authenticating
    CLOUD_STATE_TRUSTED              // authenticated and ready to go
} cloud_state_t;

#define CLOUD_OK    (cloud.state == CLOUD_STATE_TRUSTED)
#define STATE(s)    (cloud.state == CLOUD_STATE_##s)

typedef struct {
    byte      data[QUEUE_SIZE];
    size_t    length;
    uint32_t  index;                 // for reading
} message_queue_t;

/**
 * For pinging the server, if no reply after x frames, assuming connection is
 * broken and reconnect.
 */
typedef struct {
    bool  waiting;                   // we sent a ping, waiting for a pong
    uint32_t  frame_sent;            // when it was sent
    uint32_t  frame_next;            // when to send the next one
    uint8_t   miss_count;            // how many sent without a reply
} cloud_ping_t;

/**
 * Authenticating with a Cloud Admin server is a 4-way handshake. These
 * describe those levels.
 */
typedef enum {
    CLOUD_AUTH_SENT_CL_NONCE,
    CLOUD_AUTH_REC_KEY,
    CLOUD_AUTH_SENT_SV_NONCE,
    CLOUD_AUTH_REC_ACK,
} cloud_auth_t;

/**
 * Connection specific stuff. Also holds the asymmetric and symmetric
 * encryption keys and nonces.
 */
typedef struct {
    uint32_t socket;
    bool ipv6;                       // is this a v6 connection?
    bool trusted;                    // is the server trusted?
    bool encrypted;                  // should we encrypt?
    bool have_keys;                  // do we have the shared keys?
    cloud_auth_t authstage;
    uint8_t auth_fail_count;

    // auth and encryption stuff
    byte cl_nonce[CHALLENGE_LEN];    // random data
    byte sv_nonce[CHALLENGE_LEN];    // random data
    byte session_key[AESKEY_LEN];    // shared encryption key (128bit)
    byte initial_value[AES_IV_LEN];  // CBC IV is 16 bytes

    EVP_PKEY *public_key;            // our public key
    EVP_PKEY *private_key;           // our private key
    EVP_PKEY *server_key;            // CA server's public key

    EVP_CIPHER_CTX *e_ctx;           // encryption context
    EVP_CIPHER_CTX *d_ctx;           // decryption context

    fd_set set_r;                    // read
    fd_set set_w;                    // write
    fd_set set_e;                    // error
} cloud_connection_t;

/**
 * Holds all info and state about the cloud admin connection
 */
typedef struct {
    cloud_state_t state;
    cloud_connection_t connection;
    uint32_t connect_retry_frame;
    uint32_t connection_attempts;
    uint32_t disconnect_count;
    uint32_t connected_frame;        // the frame when we connected
    struct addrinfo *addr;
    uint32_t flags;
    uint32_t frame_number;
    char mapname[32];
    uint8_t maxclients;
    uint16_t port;
    message_queue_t queue_out;       // messages outgoing to central server
    message_queue_t queue_in;        // messages incoming from central server
    cloud_ping_t ping;
} cloud_t;

/**
 * Major client (q2server) to server (q2admin server) commands.
 */
typedef enum {
    CMD_NULL,
    CMD_HELLO,
    CMD_QUIT,                        // server quit
    CMD_CONNECT,                     // player
    CMD_DISCONNECT,                  // player
    CMD_PLAYERLIST,
    CMD_PLAYERUPDATE,
    CMD_PRINT,
    CMD_COMMAND,                     // teleport, invite, etc
    CMD_PLAYERS,
    CMD_FRAG,                        // someone fragged someone else
    CMD_MAP,                         // map changed
    CMD_PING,
    CMD_AUTH
} cloud_client_cmd_t;

/**
 * Server to client commands
 */
typedef enum {
    SCMD_NULL,
    SCMD_HELLOACK,
    SCMD_ERROR,
    SCMD_PONG,
    SCMD_COMMAND,
    SCMD_SAYCLIENT,
    SCMD_SAYALL,
    SCMD_AUTH,
    SCMD_TRUSTED,
    SCMD_KEY,
    SCMD_GETPLAYERS,
} cloud_server_cmd_t;

/**
 * Sub commands. These are initiated by players
 */
typedef enum {
    CMD_COMMAND_TELEPORT,
    CMD_COMMAND_INVITE,
    CMD_COMMAND_WHOIS
} cloud_cl_command_t;

/**
 * Cloud admin configuration
 */
typedef struct {
    char address[256];
    char cmd_invite[25];
    char cmd_seen[25];
    char cmd_teleport[25];
    char cmd_whois[25];
    char dns[3];
    bool enabled;
    bool encryption;
    int flags;
    int port;
    char private[256];
    char public[256];
    char serverkey[256];
    char uuid[37];
} cloud_config_t;

void        cloudInit(void);
void        cloudShutdown(void);
void        cloudFrame(void);
void        cloudPlayerConnect(edict_t *ent);
void        cloudPlayerDisconnect(edict_t *ent);
void        CA_PlayerCommand(edict_t *ent);
uint8_t     cloudReadByte(void);
uint16_t    cloudReadShort(void);
int32_t     cloudReadLong(void);
char        *cloudReadString(void);
void        cloudReadData(void *out, size_t len);
#if defined(__GNUC__) || defined(__clang__)
void        cloudWriteString(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#else
void        cloudWriteString(const char *fmt, ...);
#endif
void        cloudWriteByte(uint8_t b);
void        cloudWriteLong(uint32_t i);
void        cloudWriteShort(uint16_t s);
void        cloudWriteData(const void *data, size_t length);
uint16_t    getport(void);

void        cloudPrint(uint8_t level, char *text);
void        cloudTeleport(uint8_t client_id, char *where);
void        cloudFrag(uint8_t victim, uint8_t attacker);
void        cloudPlayerUpdate(uint8_t cl, const char *ui);
void        cloudInvite(uint8_t cl, const char *text);
void        cloudWhois(uint8_t cl, const char *name);
void        cloudMap(const char *mapname);
void        cloudConnect(void);
void        cloudDisconnect(void);
void        cloudCheckConnection(void);
void        cloudSendMessages(void);
void        cloudReadMessages(void);
void        cloudParseMessage(void);
void        cloudParseCommand(void);
void        cloudPeerDisconnected(void);
void        cloudPing(void);
void        cloudPlayerList(void);
void        cloudLookupAddress(void);
void        cloudStartThread(void *func, void *arg);
void        cloudSayHello(void);
void        cloudParsePong(void);
void        cloudParseError(void);
void        cloudSayClient(void);
void        cloudSayAll(void);
bool        cloudVerifyServerAuth(void);
void        cryptoLogRSAErrors(void);
void        cloudRotateKeys(void);
void        cloudDebugPrint(char *str);
void        cloudRun(int startarg, edict_t *ent, int client);
void        cloudPrintf(char *fmt, ...);
void        cloudDPrintf(char *fmt, ...);
void        cloudReadConfigFile(void);
void        Cmd_Invite_f(edict_t *ent);
void        Cmd_Teleport_f(edict_t *ent);

extern cloud_t cloud;
extern cloud_config_t cloud_config;
extern cvar_t *gamelib;

#endif
