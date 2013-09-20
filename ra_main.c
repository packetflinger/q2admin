#include "ra_main.h"
#include "g_local.h"


ra_state_t ra;

// remote admin specific cvars
cvar_t		*remote_enabled;
cvar_t		*remote_addr;
cvar_t		*remote_port;
cvar_t		*remote_uniqid;

pthread_t	ra_recthread;
fd_set		fds;

void RA_ParseInput()
{

}

// Read all available text from the connection
char *sslRead (connection *c)
{
	const int readSize = 1024;
	char *rc = NULL;
	int received, count = 0;
	char buffer[1024];

	if (c)
	{
		while (1)
		{
			if (!rc)
				rc = malloc (readSize * sizeof (char) + 1);
			else
				rc = realloc (rc, (count + 1) *
						  readSize * sizeof (char) + 1);

			received = SSL_read (c->sslHandle, buffer, readSize);
			buffer[received] = '\0';

			if (received > 0)
				strcat (rc, buffer);

			if (received < readSize)
				break;
			count++;
		}
	}
	else
	{
		gi.dprintf("not connected\n");
	}

	return rc;
}

void *RA_Receive_Thread(void *remote)
{
	char *reply;
	while (1)
	{
		//gi.dprintf("ra.connected = %d\n", ra.connected);
		if (ra.connected)
		{
			reply = sslRead(ra.conn);
			ra.current_msg = reply;
			//gi.dprintf ("%s\n", reply);
			RA_ParseInput();
		}
	}
	free(reply);
}



void RA_Init()
{
	if (Cvar_Match(remote_enabled->string, "0"))
	{
		return;
	}

	gi.dprintf("==== InitRemoteAdmin ====\n");
	ra.connected = 0;
	ra.connecting = 0;
	ra.server_ip = remote_addr->string;
	ra.server_port = remote_port->string;
	ra.connected_time = 0;
	ra.last_try = 999;			// fire connect immediately on next frame

	// setup the socket 
	ra.timeout.tv_sec	= 0;		// don't wait to return
	ra.timeout.tv_usec	= 0;		// ""
	//ra.socket_ready		= 0;
	FD_ZERO(&ra.fds);
	
	// start a new thread to handle the networking
	//pthread_create(&ra_recthread, NULL, RA_Receive_Thread, &ra);
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
	//gi.dprintf("checkstatus\n");
}


void RA_ReadPackets()
{
	if (ra.connected || ra.connecting)
	{
		
		FD_SET(ra.conn->socket, &ra.fds);
		//gi.dprintf("readpacketes\n");
		if (select(sizeof(ra.fds)*8, &ra.fds, NULL, NULL, &ra.timeout) > 0)
		{
			char *reply;
	
			//gi.dprintf("ra.connected = %d\n", ra.connected);
	
			reply = sslRead(ra.conn);
			ra.current_msg = reply;
			gi.dprintf ("%s\n", reply);
			//RA_ParseInput();
			free(reply);
		}
	}
}

// run every frame (1/10 second)
void RA_RunFrame()
{
	if (Cvar_Match(remote_enabled->string, "1"))
	{
		//gi.dprintf("running frame\n");
		RA_ReadPackets();
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



int tcpConnect ()
{
	int error, handle;
	struct hostent *host;
	struct sockaddr_in server;

	host = gethostbyname (ra.server_ip);
	handle = socket (AF_INET, SOCK_STREAM, 0);
	if (handle == -1)
	{
		gi.dprintf("* RA: couldn't create socket\n");
		perror ("Socket");
		handle = 0;
	}
	else
	{
		server.sin_family = AF_INET;
		server.sin_port = htons (atoi(ra.server_port));
		server.sin_addr = *((struct in_addr *) host->h_addr);
		bzero (&(server.sin_zero), 8);

		error = connect (handle, (struct sockaddr *) &server,
					   sizeof (struct sockaddr));
		if (error == -1)
		{
			gi.dprintf("* RA: couldn't connect using created socket\n");
			perror ("Connect");
			handle = 0;
		}
	}

	return handle;
}



connection *sslConnect (void)
{
	connection *c;

	c = malloc (sizeof (connection));
	c->sslHandle = NULL;
	c->sslContext = NULL;

	c->socket = tcpConnect ();
	if (c->socket)
	{
		// Register the error strings for libcrypto & libssl
		SSL_load_error_strings ();
		// Register the available ciphers and digests
		SSL_library_init ();

		// New context saying we are a client, and using SSL 2 or 3
		c->sslContext = SSL_CTX_new (SSLv23_client_method ());
		if (c->sslContext == NULL)
			ERR_print_errors_fp (stderr);

		// Create an SSL struct for the connection
		c->sslHandle = SSL_new (c->sslContext);
		if (c->sslHandle == NULL)
			ERR_print_errors_fp (stderr);

		// Connect the SSL struct to our connection
		if (!SSL_set_fd (c->sslHandle, c->socket))
			ERR_print_errors_fp (stderr);

		// Initiate SSL handshake
		if (SSL_connect (c->sslHandle) != 1)
			ERR_print_errors_fp (stderr);
	}
	else
	{
		gi.dprintf("* RA: SSL connection failed\n");
		perror ("Connect failed");
	}

	return c;
}


// Disconnect & free connection struct
void sslDisconnect (connection *c)
{
	if (c->socket)
		close (c->socket);
	if (c->sslHandle)
	{
		SSL_shutdown (c->sslHandle);
		SSL_free (c->sslHandle);
	}
	if (c->sslContext)
		SSL_CTX_free (c->sslContext);

	free (c);
}



// Write text to the connection
void sslWrite (connection *c, char *text)
{
	if (c)
	{
		SSL_write (c->sslHandle, text, strlen (text));
	}
}

void RA_Connect()
{
	//char *reply;

	ra.connecting = 1;
	ra.last_try = 0;
	gi.dprintf("== RemoteAdmin: Connecting to %s:%s ==\n", ra.server_ip, ra.server_port);
	//connection *c;
	ra.conn = sslConnect();
	ra.connected = 1;
	ra.connecting = 0;
	sslWrite(ra.conn, "GET /\r\n\r\n");
	//reply = sslRead (ra.conn);

	//gi.dprintf ("%s\n", reply);

	
	//free (reply);

}

void RA_Disconnect()
{
	ra.connected = 0;
	ra.connecting = 0;
	gi.dprintf("== RemoteAdmin: Disconnecting... ==\n");
	sslDisconnect (ra.conn);
}