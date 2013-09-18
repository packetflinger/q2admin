
typedef struct {
	int enabled;
	int	connected; 		// are we currently connected?
	int	connecting;		// are we trying to connect?
	char	*ra_server_ip;	// the server we're connected to
	char    *ra_server_port;
	int	report_only; 		// only send data, ignore server control msgs
	long connected_time;
	float last_try;
	//ra_msg_q_t	*msgs;
} ra_state_t;

typedef struct {
	char		msg[200];
	//ra_msg_q_t	*next;
} ra_msg_q_t;
