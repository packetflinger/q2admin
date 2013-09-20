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

#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define	PROTO_REGISTER	100;
#define PROTO_NOTAUTH	101;
#define PROTO_PING		102;
#define PROTO_PONG		103;

#define PROTO_JOIN		200;
#define PROTO_PART		201;
#define PROTO_FRAG		202;
#define PROTO_CHAT		203;

#define PROTO_MUTE		300;
#define PROTO_KICK		301;
#define PROTO_BAN		302;
#define PROTO_BLACKHOLE	303;



typedef struct {
    int		socket;
    SSL		*sslHandle;
    SSL_CTX	*sslContext;
} connection;

typedef struct {
	int			enabled;			// should we do anything?
	int			connected; 			// are we currently connected?
	int			connecting;			// are we trying to connect?
	char		*server_ip;			// the server we're connected to
	char		*server_port;		// tcp port
	int			report_only; 		// only send data, ignore server control msgs
	long		connected_time;		// how long have we been connected?
	float		last_try;			// (server) time of our last connection attempt
	float		last_msg;			// how long since our last msg from server?
	float		last_sent;			// when was the last time we sent a msg?
	char		*current_msg;		// the last msg sent from the server
	connection	*conn;				// the ssl network connection
	fd_set		fds;				// the file descriptors for the socket to monitor
	struct		timeval	timeout;	// the timeout for select
	//int			socket_ready		// the return value of select 
} ra_state_t;

typedef struct {
	char		msg[200];
	//ra_msg_q_t	*next;
} ra_msg_q_t;



