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

#define NS_INT16SZ      2
#define NS_INADDRSZ     4
#define NS_IN6ADDRSZ    16

#define QUEUE_SIZE      0x55FF

#define CURFRAME        (remote.frame_number)
#define RECONNECT(t)    (CURFRAME + SECS_TO_FRAMES(t))

extern cvar_t		*gamelib;
extern cvar_t		*udpport;

// Remote Admin flags
#define RFL_FRAGS      1 << 0 	// 1
#define RFL_CHAT       1 << 1	// 2
#define RFL_TELEPORT   1 << 2	// 4
#define RFL_INVITE     1 << 3	// 8
#define RFL_FIND       1 << 4	// 16
#define RFL_WHOIS      1 << 5	// 32
#define RFL_DEBUG      1 << 11	// 2047

#define MAX_MSG_LEN		1000

/**
 * The various states of the remote admin connection
 */
typedef enum {
	RA_STATE_DISCONNECTED,
	RA_STATE_CONNECTING,
	RA_STATE_CONNECTED
} ra_state_t;

typedef struct {
	byte      data[QUEUE_SIZE];
	size_t    length;
	uint32_t  index;    // for reading
} message_queue_t;

/**
 * Holds all info and state about the remote admin connection
 */
typedef struct {
	uint8_t          enabled;
	ra_state_t       state;
	uint32_t         connect_retry_frame;
	uint32_t         connection_attempts;
	uint32_t         socket;
	fd_set           set_r;    // read
	fd_set           set_w;    // write
	fd_set           set_e;    // error
	struct 	addrinfo *addr;
	uint32_t         flags;
	uint32_t         frame_number;
	char             mapname[32];
	uint32_t         next_report;
	char             rcon_password[32];
	uint8_t          maxclients;
	uint16_t         port;
	qboolean         online;
	byte             msg[MAX_MSG_LEN];
	uint16_t         msglen;
	message_queue_t  queue;
	message_queue_t  queue_in;
} remote_t;


typedef enum {
	CMD_REGISTER,		// server
	CMD_QUIT,			// server
	CMD_CONNECT,		// player
	CMD_DISCONNECT,		// player
	CMD_PLAYERLIST,
	CMD_PLAYERUPDATE,
	CMD_PRINT,
	CMD_TELEPORT,
	CMD_INVITE,
	CMD_SEEN,
	CMD_WHOIS,
	CMD_PLAYERS,
	CMD_FRAG,
	CMD_MAP,
	CMD_AUTHORIZE,
	CMD_HEARTBEAT
} remote_cmd_t;

void 		RA_Send(void);
void		RA_Init(void);
void		RA_Shutdown(void);
void		RA_RunFrame(void);
void		RA_Register(void);
void		RA_Unregister(void);
void		RA_PlayerConnect(edict_t *ent);
void		RA_PlayerDisconnect(edict_t *ent);
void		RA_PlayerCommand(edict_t *ent);

uint8_t     RA_ReadByte(void);
uint16_t    RA_ReadShort(void);
int32_t     RA_ReadLong(void);
char        *RA_ReadString(void);
void        RA_ReadData(void *out, size_t len);
void		RA_WriteString(const char *fmt, ...);
void		RA_WriteByte(uint8_t b);
void		RA_WriteLong(uint32_t i);
void		RA_InitBuffer(void);
uint16_t 	getport(void);

void 		RA_Print(uint8_t level, char *text);
void 		RA_Teleport(uint8_t client_id);
void		RA_Frag(uint8_t victim, uint8_t attacker, const char *vname, const char *aname);
void		RA_PlayerUpdate(uint8_t cl, const char *ui);
void		RA_Invite(uint8_t cl, const char *text);
void		RA_Whois(uint8_t cl, const char *name);
void		RA_Map(const char *mapname);
void 		RA_Authorize(const char *authkey);
void 		RA_HeartBeat(void);
void		RA_Encrypt(void);

void        RA_Connect(void);
void        RA_CheckConnection(void);
void        RA_SendMessages(void);
void        RA_ReadMessages(void);
void        RA_ParseMessage(void);
void        RA_DisconnectedPeer(void);

extern remote_t remote;

#endif
