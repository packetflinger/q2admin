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

#define MAGIC_CLIENT    (('C' << 24) + ('A' << 16) + ('2' << 8) + 'Q')
#define MAGIC_PEER      (('P' << 24) + ('A' << 16) + ('2' << 8) + 'Q')

#define NS_INT16SZ      2
#define NS_INADDRSZ     4
#define NS_IN6ADDRSZ    16

#define QUEUE_SIZE      0x55FF

#define CURFRAME        (cloud.frame_number)

// arg in seconds
#define FUTURE_FRAME(t) (CURFRAME + SECS_TO_FRAMES(t))

#define CLOUDCMD_LAYOUT "sv !cloud <status|(dis|en)able|rule>"


// Remote Admin flags
#define RFL_FRAGS       1 << 0    // 1
#define RFL_CHAT        1 << 1    // 2
#define RFL_TELEPORT    1 << 2    // 4
#define RFL_INVITE      1 << 3    // 8
#define RFL_FIND        1 << 4    // 16
#define RFL_WHOIS       1 << 5    // 32
#define RFL_DEBUG       1 << 11   // 1024
#define RFL(f)          ((cloud.flags & RFL_##f) != 0)

#define MAX_MSG_LEN     1000

#define PING_FREQ_SECS  40
#define PING_MISS_MAX   3

#define RSA_LEN         256 // bytes, 2048 bits
#define CHALLENGE_LEN   16  // bytes
#define AESKEY_LEN      16  // bytes, 128 bits
#define AESBLOCK_LEN    16  // bytes, 128 bits
#define AES_IV_LEN      16
#define AUTH_FAIL_LIMIT 3   // stop trying after
#define DIGEST_LEN      (SHA256_DIGEST_LENGTH)


/**
 * The various states of the remote admin connection
 */
typedef enum {
    CA_STATE_DISABLED,      // not using RA at all
    CA_STATE_DISCONNECTED,  // will try to connect when possible
    CA_STATE_CONNECTING,    // mid connection
    CA_STATE_CONNECTED,     // connected
    CA_STATE_TRUSTED        // authenticated and ready to go
} ca_state_t;

#define STATE(s)    (remote.state == RA_STATE_##s)

typedef struct {
    byte      data[QUEUE_SIZE];
    size_t    length;
    uint32_t  index;    // for reading
} message_queue_t;


/**
 * For pinging the server, if no reply after x frames, assuming
 * connection is broken and reconnect.
 */
typedef struct {
    qboolean  waiting;      // we sent a ping, waiting for a pong
    uint32_t  frame_sent;   // when it was sent
    uint32_t  frame_next;   // when to send the next one
    uint8_t   miss_count;   // how many sent without a reply
} ping_t;


/**
 * Authenticating with a Remote Admin server is a 4-way handshake.
 * These describe those levels
 */
typedef enum {
    RA_AUTH_SENT_CL_NONCE,
    RA_AUTH_REC_KEY,
    RA_AUTH_SENT_SV_NONCE,
    RA_AUTH_REC_ACK,
} ca_auth_t;


/**
 * Connection specific stuff. Also holds the asymmetric and
 * symmetric encryption keys and nonces.
 */
typedef struct {
    uint32_t    socket;
    qboolean    ipv6;
    qboolean    trusted;    // is the server trusted?
    qboolean    encrypted;  // should we encrypt?
    qboolean    have_keys;  // do we have the shared keys?
    ca_auth_t   authstage;
    uint8_t     auth_fail_count;

    // auth and encryption stuff
    byte    cl_nonce[CHALLENGE_LEN];  // random data
    byte    sv_nonce[CHALLENGE_LEN];  // random data
    byte    aeskey[AESKEY_LEN];       // shared encryption key (128bit)
    byte    iv[AES_IV_LEN];        // GCM IV is 12 bytes

    EVP_PKEY *rsa_pu;      // our public key
    EVP_PKEY *rsa_pr;      // our private key
    EVP_PKEY *rsa_sv_pu;   // RA server's public key

    EVP_CIPHER_CTX *e_ctx;  // encryption context
    EVP_CIPHER_CTX *d_ctx;  // decryption context

    fd_set  set_r;    // read
    fd_set  set_w;    // write
    fd_set  set_e;    // error
} ca_connection_t;

/**
 * Holds all info and state about the cloud admin connection
 */
typedef struct {
    ca_state_t       state;
    ca_connection_t  connection;
    uint32_t         connect_retry_frame;
    uint32_t         connection_attempts;
    uint32_t         disconnect_count;
    uint32_t         connected_frame;  // the frame when we connected
    struct addrinfo  *addr;
    uint32_t         flags;
    uint32_t         frame_number;
    char             mapname[32];
    uint8_t          maxclients;
    uint16_t         port;
    message_queue_t  queue;     // messages outgoing to RA server
    message_queue_t  queue_in;  // messages incoming from RA server
    ping_t           ping;
} cloud_t;


/**
 * Major client (q2server) to server (q2admin server)
 * commands.
 */
typedef enum {
    CMD_NULL,
    CMD_HELLO,
    CMD_QUIT,          // server quit
    CMD_CONNECT,       // player
    CMD_DISCONNECT,    // player
    CMD_PLAYERLIST,
    CMD_PLAYERUPDATE,
    CMD_PRINT,
    CMD_COMMAND,       // teleport, invite, etc
    CMD_PLAYERS,
    CMD_FRAG,          // someone fragged someone else
    CMD_MAP,           // map changed
    CMD_PING,          //
    CMD_AUTH
} ca_client_cmd_t;


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
} ca_server_cmd_t;


/**
 * Sub commands. These are initiated by players
 */
typedef enum {
    CMD_COMMAND_TELEPORT,
    CMD_COMMAND_INVITE,
    CMD_COMMAND_WHOIS
} remote_cmd_command_t;


void        CA_Send(void);
void        CA_Init(void);
void        CA_Shutdown(void);
void        CA_RunFrame(void);
void        CA_Register(void);
void        CA_Unregister(void);
void        CA_PlayerConnect(edict_t *ent);
void        CA_PlayerDisconnect(edict_t *ent);
void        CA_PlayerCommand(edict_t *ent);

uint8_t     CA_ReadByte(void);
uint16_t    CA_ReadShort(void);
int32_t     CA_ReadLong(void);
char        *CA_ReadString(void);
void        CA_ReadData(void *out, size_t len);
void        CA_WriteString(const char *fmt, ...);
void        CA_WriteByte(uint8_t b);
void        CA_WriteLong(uint32_t i);
void        CA_WriteShort(uint16_t s);
void        CA_WriteData(const void *data, size_t length);
void        CA_InitBuffer(void);
uint16_t    getport(void);

void        CA_Print(uint8_t level, char *text);
void        CA_Teleport(uint8_t client_id);
void        CA_Frag(uint8_t victim, uint8_t attacker);
void        CA_PlayerUpdate(uint8_t cl, const char *ui);
void        CA_Invite(uint8_t cl, const char *text);
void        CA_Whois(uint8_t cl, const char *name);
void        CA_Map(const char *mapname);
void        CA_Authorize(const char *authkey);
void        CA_HeartBeat(void);
void        CA_Encrypt(void);

void        CA_Connect(void);
void        CA_Disconnect(void);
void        CA_CheckConnection(void);
void        CA_SendMessages(void);
void        CA_ReadMessages(void);
void        CA_ParseMessage(void);
void        CA_ParseCommand(void);
void        CA_DisconnectedPeer(void);
void        CA_Ping(void);
void        CA_PlayerList(void);
void        CA_LookupAddress(void);
void        G_StartThread(void *func, void *arg);
void        CA_SayHello(void);
void        CA_ParsePong(void);
void        CA_ParseError(void);
void        CA_SayClient(void);
void        CA_SayAll(void);
qboolean    G_LoadKeys(void);
qboolean    CA_VerifyServerAuth(void);
void        G_RSAError(void);
void        hexDump (char *desc, void *addr, int len);
void        CA_RotateKeys(void);
void        debug_print(char *str);
void        G_SHA256Hash(byte *dest, byte *src, size_t src_len);
void        cloudRun(int startarg, edict_t *ent, int client);
void        CA_printf(char *fmt, ...);
void        CA_dprintf(char *fmt, ...);
void        ReadCloudConfigFile(char *filename);
size_t      G_PublicEncrypt(EVP_PKEY *key, byte *out, byte *in, size_t inlen);


extern cloud_t  cloud;
extern cvar_t    *gamelib;


#endif
