//cvar_t	*remote_enabled;
//cvar_t	*remote_addr;
//cvar_t	*remote_port;
//cvar_t	*remote_uniqid;

typedef struct {
	int enabled;
	int	connected; 		// are we currently connected?
	int	connecting;		// are we trying to connect?
	char	*ra_server_ip;	// the server we're connected to
	float	ra_server_port;
	int	report_only; 		// only send data, ignore server control msgs
	long connected_time;
	float last_try;
	//ra_msg_q_t	*msgs;
} ra_state_t;

typedef struct {
	char		msg[200];
	//ra_msg_q_t	*next;
} ra_msg_q_t;
