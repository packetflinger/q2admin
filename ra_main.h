//cvar_t	*remote_enabled;
//cvar_t	*remote_addr;
//cvar_t	*remote_port;
//cvar_t	*remote_uniqid;

typedef struct {
	int	connected; 		// are we currently connected?
	int	connecting;		// are we trying to connect?
	char	ra_server_ip[15];	// the server we're connected to
	int	report_only; 		// only send data, ignore server control msgs
	//ra_msg_q_t	*msgs;
} ra_state_t;

typedef struct {
	char		msg[200];
	//ra_msg_q_t	*next;
} ra_msg_q_t;
