#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

typedef struct {
    int		socket;
    SSL		*sslHandle;
    SSL_CTX	*sslContext;
} connection;

typedef struct {
	int		enabled;			// should we do anything?
	int		connected; 			// are we currently connected?
	int		connecting;			// are we trying to connect?
	char	*server_ip;			// the server we're connected to
	char    *server_port;		// tcp port
	int		report_only; 		// only send data, ignore server control msgs
	long	connected_time;		// how long have we been connected?
	float	last_try;			// (server) time of our last connection attempt
	float	last_msg;			// how long since our last msg from server?
	float	last_sent;			// when was the last time we sent a msg?
	char	*current_msg;		// the last msg sent from the server
	connection	*conn;			// the ssl network connection
} ra_state_t;

typedef struct {
	char		msg[200];
	//ra_msg_q_t	*next;
} ra_msg_q_t;



