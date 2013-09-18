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
    int socket;
    SSL *sslHandle;
    SSL_CTX *sslContext;
} connection;

typedef struct {
	int enabled;
	int	connected; 		// are we currently connected?
	int	connecting;		// are we trying to connect?
	char	*ra_server_ip;	// the server we're connected to
	char    *ra_server_port;
	int	report_only; 		// only send data, ignore server control msgs
	long connected_time;
	float last_try;
	connection *conn;		// the ssl network connection
	//ra_msg_q_t	*msgs;
} ra_state_t;

typedef struct {
	char		msg[200];
	//ra_msg_q_t	*next;
} ra_msg_q_t;



