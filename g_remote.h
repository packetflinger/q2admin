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
extern cvar_t		*remote_enabled;
extern cvar_t		*remote_server;
extern cvar_t		*remote_port;
extern cvar_t		*remote_key;
extern cvar_t		*remote_flags;
extern cvar_t		*net_port;


// flags
#define REMOTE_FL_CMD_TELEPORT		1
#define REMOET_FL_CMD_INVITE		2
#define REMOTE_FL_CMD_FIND			4
#define REMOTE_FL_DEBUG				8


typedef struct {
	uint8_t 	enabled;
	uint32_t 	socket;
	struct 	addrinfo *addr;
	uint32_t	flags;
} remote_t;


typedef enum {
	CMD_REGISTER,
	CMD_CONNECT,
	CMD_USERINFO,
	CMD_PRINT,
	CMD_CHAT,
	CMD_DISCONNECT,
	CMD_UNREGISTER,
	CMD_TELEPORT,
	CMD_INVITE,
	CMD_FIND,
	CMD_FRAG
} remote_cmd_t;


void 	RA_Send(remote_cmd_t cmd, const char *fmt, ...);
void	RA_Init(void);
void	Remote_RunFrame(void);

extern remote_t remote;

#endif
