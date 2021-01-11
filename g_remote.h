#ifndef G_REMOTE_H
#define G_REMOTE_H

#include <glib.h>
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

#define NS_INT16SZ      2
#define NS_INADDRSZ     4
#define NS_IN6ADDRSZ    16

#define QUEUE_SIZE      0x55FF

#define CURFRAME        (remote.frame_number)

// arg in seconds
#define FUTURE_FRAME(t) (CURFRAME + SECS_TO_FRAMES(t))


// Remote Admin flags
#define RFL_FRAGS       1 << 0    // 1
#define RFL_CHAT        1 << 1    // 2
#define RFL_TELEPORT    1 << 2    // 4
#define RFL_INVITE      1 << 3    // 8
#define RFL_FIND        1 << 4    // 16
#define RFL_WHOIS       1 << 5    // 32
#define RFL_DEBUG       1 << 11   // 1024
#define RFL(f)          ((remote.flags & RFL_##f) != 0)

#define MAX_MSG_LEN     1000

#define PING_FREQ_SECS  10
#define PING_MISS_MAX   3

#define RSA_LEN         256 // bytes, 2048 bits
#define CHALLENGE_LEN   16  // bytes
#define AESKEY_LEN      16  // bytes, 128 bits
#define AESBLOCK_LEN    16  // bytes, 128 bits
#define AUTH_FAIL_LIMIT 3   // stop trying after


/**
 * The various states of the remote admin connection
 */
typedef enum {
    RA_STATE_DISABLED,      // not using RA at all
    RA_STATE_DISCONNECTED,  // will try to connect when possible
    RA_STATE_CONNECTING,    // mid connection
    RA_STATE_CONNECTED,     // connected
    RA_STATE_TRUSTED        // authenticated and ready to go
} ra_state_t;

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
} ra_auth_t;


/**
 * Connection specific stuff. Also holds the asymmetric and
 * symmetric encryption keys and nonces.
 */
typedef struct {
    uint32_t    socket;
    qboolean    ipv6;
    qboolean    trusted;      // is the server trusted?
    qboolean    encrypted;    // should we encrypt?
    ra_auth_t   authstage;
    uint8_t     auth_fail_count;

    // auth and encryption stuff
    byte    cl_nonce[CHALLENGE_LEN];  // random data
    byte    sv_nonce[CHALLENGE_LEN];  // random data
    byte    aeskey[AESKEY_LEN];       // shared encryption key (128bit)
    byte    iv[AESBLOCK_LEN];         // aes block size

    RSA     *rsa_pu;      // our public key
    RSA     *rsa_pr;      // our private key
    RSA     *rsa_sv_pu;   // RA server's public key

    EVP_CIPHER_CTX *e_ctx;  // encryption context
    EVP_CIPHER_CTX *d_ctx;  // decryption context

    fd_set  set_r;    // read
    fd_set  set_w;    // write
    fd_set  set_e;    // error
} ra_connection_t;

/**
 * Holds all info and state about the remote admin connection
 */
typedef struct {
    ra_state_t       state;
    ra_connection_t  connection;
    uint32_t         connect_retry_frame;
    uint32_t         connection_attempts;
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
} remote_t;


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
} ra_client_cmd_t;


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
} ra_server_cmd_t;


/**
 * Sub commands. These are initiated by players
 */
typedef enum {
    CMD_COMMAND_TELEPORT,
    CMD_COMMAND_INVITE,
    CMD_COMMAND_WHOIS
} remote_cmd_command_t;


void        RA_Send(void);
void        RA_Init(void);
void        RA_Shutdown(void);
void        RA_RunFrame(void);
void        RA_Register(void);
void        RA_Unregister(void);
void        RA_PlayerConnect(edict_t *ent);
void        RA_PlayerDisconnect(edict_t *ent);
void        RA_PlayerCommand(edict_t *ent);

uint8_t     RA_ReadByte(void);
uint16_t    RA_ReadShort(void);
int32_t     RA_ReadLong(void);
char        *RA_ReadString(void);
void        RA_ReadData(void *out, size_t len);
void        RA_WriteString(const char *fmt, ...);
void        RA_WriteByte(uint8_t b);
void        RA_WriteLong(uint32_t i);
void        RA_WriteShort(uint16_t s);
void        RA_WriteData(const void *data, size_t length);
void        RA_InitBuffer(void);
uint16_t    getport(void);

void        RA_Print(uint8_t level, char *text);
void        RA_Teleport(uint8_t client_id);
void        RA_Frag(uint8_t victim, uint8_t attacker, const char *vname, const char *aname);
void        RA_PlayerUpdate(uint8_t cl, const char *ui);
void        RA_Invite(uint8_t cl, const char *text);
void        RA_Whois(uint8_t cl, const char *name);
void        RA_Map(const char *mapname);
void        RA_Authorize(const char *authkey);
void        RA_HeartBeat(void);
void        RA_Encrypt(void);

void        RA_Connect(void);
void        RA_Disconnect(void);
void        RA_CheckConnection(void);
void        RA_SendMessages(void);
void        RA_ReadMessages(void);
void        RA_ParseMessage(void);
void        RA_ParseCommand(void);
void        RA_DisconnectedPeer(void);
void        RA_Ping(void);
void        RA_PlayerList(void);
void        RA_LookupAddress(void);
void        G_StartThread(void *func, void *arg);
void        RA_SayHello(void);
void        RA_ParsePong(void);
void        RA_ParseError(void);
void        RA_SayClient(void);
void        RA_SayAll(void);
qboolean    G_LoadKeys(void);
qboolean    RA_VerifyServerAuth(void);
void        G_RSAError(void);
void        hexDump (char *desc, void *addr, int len);


extern remote_t  remote;
extern cvar_t    *gamelib;


#endif
