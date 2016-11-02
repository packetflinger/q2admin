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


char *pfva(const char *format, ...) __attribute__((format(printf, 1, 2)));

void	RA_Send(const char *format, ...);
void	RA_Init(void);
void	RA_Shutdown(void);
void	RA_RunFrame(void);

typedef struct {
	int 	enabled;
	int 	socket;
	struct 	addrinfo *addr;
	int 	key;
} remote_t;

extern remote_t remote;

#endif