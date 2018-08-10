#ifndef G_REMOTE_H
#define G_REMOTE_H

#include <glib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>

extern cvar_t		*gamelib;
extern cvar_t		*udpport;

// flags
#define REMOTE_FL_LOG_FRAGS			1 << 0 	// 1
#define REMOTE_FL_LOG_CHAT			1 << 1	// 2
#define REMOTE_FL_CMD_TELEPORT		1 << 2	// 4
#define REMOTE_FL_CMD_INVITE		1 << 3	// 8
#define REMOTE_FL_CMD_FIND			1 << 4	// 16
#define REMOTE_FL_CMD_WHOIS			1 << 5	// 32
#define REMOTE_FL_DEBUG				1 << 11	// 2048

#define MAX_MSG_LEN		1000

typedef struct {
	uint8_t 		enabled;
	uint32_t 		socket;
	struct 	addrinfo *addr;
	uint32_t		flags;
	uint32_t		frame_number;
	char			mapname[32];
	uint32_t		next_report;
	char			rcon_password[32];
	uint8_t			maxclients;
	uint16_t		port;
	qboolean		online;
	byte			msg[MAX_MSG_LEN];
	uint16_t		msglen;
} remote_t;


typedef enum {
	CMD_REGISTER,		// server
	CMD_QUIT,			// server
	CMD_CONNECT,		// player
	CMD_DISCONNECT,		// player
	CMD_PRINT,
	CMD_TELEPORT,
	CMD_INVITE,
	CMD_SEEN,
	CMD_WHOIS
} remote_cmd_t;


void 		RA_Send(remote_cmd_t cmd, const char *fmt, ...);
void		RA_Init(void);
void		RA_RunFrame(void);
void 		RA_Register(void);
void 		RA_Unregister(void);
void 		RA_PlayerConnect(edict_t *ent);
void 		RA_PlayerDisconnect(edict_t *ent);
uint16_t 	getport(void);

extern remote_t remote;

#endif
