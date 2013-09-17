#include "ra_main.h"
#include "g_local.h"

ra_state_t ra;

// remote admin specific cvars
cvar_t	*remote_enabled;
cvar_t	*remote_addr;
cvar_t	*remote_port;
cvar_t	*remote_uniqid;

void ra_init()
{
	gi.dprintf("==== InitRemoteAdmin ====\n");
	ra.connected = 0;
	ra.connecting = 0;
	ra.ra_server_ip = remote_addr->string;
	ra.ra_server_port = remote_port->value;
	ra.connected_time = 0;
	ra.last_try = 181;
			
}

void ra_connect()
{
	ra.connecting = 1;
	ra.last_try = 0;
	gi.dprintf("== RemoteAdmin: Connecting to %s:%s ==\n", ra.ra_server_ip, ra.ra_server_port);
}

void ra_disconnect()
{
	ra.connected = 0;
	ra.connecting = 0;
	gi.dprintf("== RemoteAdmin: Disconnecting... ==\n");
}

// run every frame (1/10 second)
void ra_checkstatus()
{
	if (!ra.connected)
	{
		// try reconnecting every 2 minutes
		if (ra.last_try > 180)
		{
			ra_connect();
		}
		ra.last_try += 0.1f;
	}
}
