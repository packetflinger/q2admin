#ifndef RA_MAIN_H
#define RA_MAIN_H

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

extern cvar_t		*remote_enabled;
extern cvar_t		*remote_server;
extern cvar_t		*remote_port;
extern cvar_t		*remote_key;
extern cvar_t		*net_port;



typedef struct {
	int 	enabled;
	int 	socket;
	struct 	addrinfo *addr;
	int 	key;
} remote_t;

typedef enum {
	CMD_REGISTER,
	CMD_CONNECT,
	CMD_USERINFO,
	CMD_PRINT,
	CMD_CHAT,
	CMD_DISCONNECT,
	CMD_UNREGISTER
} remote_cmd_t;


void	RA_Send(remote_cmd_t cmd, const char *data);
void	RA_Init(void);
void	RA_Shutdown(void);
void	RA_RunFrame(void);

extern remote_t remote;

#endif