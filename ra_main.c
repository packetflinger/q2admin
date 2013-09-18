#include "ra_main.h"
#include "g_local.h"


ra_state_t ra;

// remote admin specific cvars
cvar_t	*remote_enabled;
cvar_t	*remote_addr;
cvar_t	*remote_port;
cvar_t	*remote_uniqid;

void RA_Init()
{
	if (Cvar_Match(remote_enabled->string, "0"))
	{
		return;
	}

	gi.dprintf("==== InitRemoteAdmin ====\n");
	ra.connected = 0;
	ra.connecting = 0;
	ra.ra_server_ip = remote_addr->string;
	ra.ra_server_port = remote_port->string;
	ra.connected_time = 0;
	ra.last_try = 999;	// fire connect immediately on next frame
}

void RA_Connect()
{
	ra.connecting = 1;
	ra.last_try = 0;
	gi.dprintf("== RemoteAdmin: Connecting to %s:%s ==\n", ra.ra_server_ip, ra.ra_server_port);
}

void RA_Disconnect()
{
	ra.connected = 0;
	ra.connecting = 0;
	gi.dprintf("== RemoteAdmin: Disconnecting... ==\n");
}




void RA_CheckStatus()
{
	if (!ra.connected)
	{
		// try reconnecting every 2 minutes
		if (ra.last_try > 180)
		{
			RA_Connect();
		}
		ra.last_try += 0.1f;
	}
}




// run every frame (1/10 second)
void RA_RunFrame()
{
	if (Cvar_Match(remote_enabled->string, "1"))
	{
		//gi.dprintf("running frame\n");
		RA_CheckStatus();
	}
}



int Cvar_Match(char *cvar, char *val)
{
	if (!strncmp(cvar, val, 10))
	{
		return 1;
	}
	return 0;
}