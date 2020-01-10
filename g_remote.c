#include "g_local.h"

remote_t remote;


/**
 * Sets up RA connection. Makes sure we have what we need:
 * - enabled in config
 * - rcon password is set
 *
 */
void RA_Init() {
	
	// we're already connected
	if (remote.socket) {
		return;
	}

	memset(&remote, 0, sizeof(remote));
	maxclients = gi.cvar("maxclients", "64", CVAR_LATCH);
	
	if (!remoteEnabled) {
		gi.cprintf(NULL, PRINT_HIGH, "Remote Admin is disabled in your config.\n");
		return;
	}
	
	gi.cprintf(NULL, PRINT_HIGH, "[RA] Remote Admin Init...\n");

	if (!rconpassword->string[0]) {
		gi.cprintf(NULL, PRINT_HIGH, "[RA] rcon_password is blank...disabling\n");
		return;
	}
	
	if (!remoteAddr[0]) {
		gi.cprintf(NULL, PRINT_HIGH, "[RA] remote_addr is not set...disabling\n");
		return;
	}

	if (!remotePort) {
		gi.cprintf(NULL, PRINT_HIGH, "[RA] remote_port is not set...disabling\n");
		return;
	}

	struct addrinfo hints, *res = 0;
	memset(&hints, 0, sizeof(hints));
	memset(&res, 0, sizeof(res));
	
	hints.ai_family         = AF_INET;   	// either v6 or v4
	hints.ai_socktype       = SOCK_STREAM;	// TCP
	hints.ai_protocol       = 0;
	hints.ai_flags          = AI_ADDRCONFIG;
	
	gi.cprintf(NULL, PRINT_HIGH, "[RA] looking up %s...", remoteAddr);

	int err = getaddrinfo(remoteAddr, va("%d",remotePort), &hints, &res);
	if (err != 0) {
		gi.cprintf(NULL, PRINT_HIGH, "error, disabling\n");
		remote.enabled = 0;
		return;
	} else {
		char address[INET_ADDRSTRLEN];
		q2a_inet_ntop(res->ai_family, &((struct sockaddr_in *)res->ai_addr)->sin_addr, address, sizeof(address));
		gi.cprintf(NULL, PRINT_HIGH, "%s\n", address);
	}
	
	remote.addr = res;
	remote.flags = remoteFlags;
	remote.enabled = 1;

	// delay connection by a few seconds
	remote.connect_retry_frame = RECONNECT(5);
}

/**
 * Replace the die() function pointer for each player edict.
 * For capturing frag events
 */
static void ra_replace_die(void)
{
	static uint8_t i;

	for (i=0; i<=remote.maxclients; i++) {
		if (proxyinfo[i].inuse) {

			// replace player edict's die() pointer
			if (proxyinfo[i].ent && *proxyinfo[i].ent->die != PlayerDie_Internal) {
				proxyinfo[i].die = *proxyinfo[i].ent->die;
				proxyinfo[i].ent->die = &PlayerDie_Internal;
			}
		}
	}
}


/**
 * Periodically ping the server to know if the connection is still open
 */
void RA_Ping(void)
{
	if (!remote.state == RA_STATE_CONNECTED) {
		return;
	}

	// not time yet
	if (remote.ping.frame_next > CURFRAME) {
		return;
	}

	// there is already an outstanding ping
	if (remote.ping.waiting) {
		if (remote.ping.miss_count == PING_MISS_MAX) {
			RA_DisconnectedPeer();
			return;
		}
		remote.ping.miss_count++;
	}

	// state stuff
	remote.ping.frame_sent = CURFRAME;
	remote.ping.waiting = true;
	remote.ping.frame_next = CURFRAME + SECS_TO_FRAMES(PING_FREQ_SECS);

	// send it
	RA_WriteByte(CMD_PING);
}

/**
 * Run once per server frame
 *
 */
void RA_RunFrame(void)
{
	// keep some time
	remote.frame_number++;

	// remote admin is disabled, don't do anything
	if (!remote.enabled) {
		return;
	}

	// everything we need to do while RA is connected
	if (remote.state == RA_STATE_CONNECTED) {

		// send any buffered messages to the server
		RA_SendMessages();

		// receive any pending messages from server
		RA_ReadMessages();

		// update player die() pointers
		ra_replace_die();

		// periodically make sure connection is alive
		RA_Ping();
	}

	// connection started already, check for completion
	if (remote.state == RA_STATE_CONNECTING) {
		RA_CheckConnection();
	}

	// we're not connected, try again
	if (remote.state == RA_STATE_DISCONNECTED) {
		RA_Connect();
	}
}

void RA_Shutdown(void)
{
	if (!remote.enabled) {
		return;
	}

	/**
	 * We have to call RA_SendMessages() specifically here because there won't be another
	 * frame to send the buffered CMD_QUIT
	 */
	RA_WriteByte(CMD_QUIT);
	RA_SendMessages();

	closesocket(remote.socket);
	remote.state = RA_STATE_DISCONNECTED;
	freeaddrinfo(remote.addr);
}

/**
 * Make the connection to the remote admin server
 */
void RA_Connect(void)
{
	int flags, ret;

	// only if we've waited long enough
	if (remote.frame_number < remote.connect_retry_frame) {
		return;
	}

	// clear the in and out buffers
	q2a_memset(&remote.queue, 0, sizeof(message_queue_t));
	q2a_memset(&remote.queue_in, 0, sizeof(message_queue_t));

	remote.state = RA_STATE_CONNECTING;
	gi.cprintf(NULL, PRINT_HIGH, "[RA] Connecting to server...\n");
	remote.connection_attempts++;

	remote.socket = socket(remote.addr->ai_family, remote.addr->ai_socktype, remote.addr->ai_protocol);
	if (remote.socket == -1) {
		gi.cprintf(NULL, PRINT_HIGH, "[RA] Unable to open socket to %s:%d...disabling\n", remoteAddr, remotePort);
		remote.enabled = 0;
		remote.state = RA_STATE_DISCONNECTED;
		return;
	}

// preferred method is using POSIX O_NONBLOCK, if available
#if defined(O_NONBLOCK)
	flags = 0;
	ret = fcntl(remote.socket, F_SETFL, flags | O_NONBLOCK);
#else
	flags = 1;
	ret = ioctlsocket(remote.socket, FIONBIO, (long unsigned int *) &flags);
#endif

	if (ret == -1) {
		gi.cprintf(NULL, PRINT_HIGH, "[RA] Error setting socket to non-blocking: (%d) %s\n", errno, strerror(errno));
		remote.state = RA_STATE_DISCONNECTED;
		remote.connect_retry_frame = RECONNECT(30);
	}

	// make the actual connection
	connect(remote.socket, remote.addr->ai_addr, remote.addr->ai_addrlen);

	/**
	 * since we're non-blocking, the connection won't complete in this single server frame.
	 * We have to select() for it on a later runframe. See RA_CheckConnection()
	 */
}

/**
 * Check to see if the connection initiated by RA_Connect() has finished
 */
void RA_CheckConnection(void)
{
	uint32_t ret;
	qboolean connected = false;
	qboolean exception = false;
	struct sockaddr_storage addr;
	socklen_t len;
	struct timeval tv;
	tv.tv_sec = tv.tv_usec = 0;

	FD_ZERO(&remote.set_w);
	FD_ZERO(&remote.set_e);
	FD_SET(remote.socket, &remote.set_w);
	FD_SET(remote.socket, &remote.set_e);

	// check if connection is fully established
	ret =	select((int)remote.socket + 1, NULL, &remote.set_w, &remote.set_e, &tv);

	if (ret == 1) {

#ifdef LINUX
		uint32_t number;
		socklen_t len;

		len = sizeof(number);
		getsockopt(remote.socket, SOL_SOCKET, SO_ERROR, &number, &len);
		if (number == 0) {
			connected = true;
		} else {
			exception = true;
		}
#else
		if (FD_ISSET(remote.socket, &remote.set_w)) {
			connected = true;
		}
#endif
	} else if (ret == -1) {
		gi.cprintf(NULL, PRINT_HIGH, "[RA] Connection unfinished: %s\n", strerror(errno));
		closesocket(remote.socket);
		remote.state = RA_STATE_DISCONNECTED;
		remote.connect_retry_frame = RECONNECT(10);
		return;
	}

	// we need to make sure it's actually connected
	if (connected) {
		errno = 0;
		getpeername(remote.socket, (struct sockaddr *)&addr, &len);

		if (errno) {
			remote.connect_retry_frame = RECONNECT(30);
			remote.state = RA_STATE_DISCONNECTED;
			closesocket(remote.socket);
		} else {
			gi.cprintf(NULL, PRINT_HIGH, "[RA] Connected\n");
			remote.state = RA_STATE_CONNECTED;
			remote.ping.frame_next = remote.frame_number + SECS_TO_FRAMES(10);
			RA_SayHello();
		}
	}
}

/**
 * Send the contents of our outgoing buffer to the server
 */
void RA_SendMessages(void)
{
	// nothing to send
	if (!remote.queue.length) {
		return;
	}

	// only once we're ready (unless trying to say hello)
	if (!remote.ready && remote.queue.data[0] != CMD_HELLO) {
		return;
	}

	uint32_t ret;
	struct timeval tv;
	tv.tv_sec = tv.tv_usec = 0;

	while (true) {
		FD_ZERO(&remote.set_w);
		FD_SET(remote.socket, &remote.set_w);

		// see if the socket is ready to send data
		ret = select((int) remote.socket + 1, NULL, &remote.set_w, NULL, &tv);

		// socket write buffer is ready, send
		if (ret == 1) {
			ret = send(remote.socket, remote.queue.data, remote.queue.length, 0);

			if (ret <= 0) {
				RA_DisconnectedPeer();
				return;
			}

			// shift off the data we just sent
			memmove(remote.queue.data, remote.queue.data + ret, remote.queue.length - ret);
			remote.queue.length -= ret;

		} else if (ret < 0) {
			RA_DisconnectedPeer();
			return;
		} else {
			break;
		}

		// processed the whole queue, we're done for now
		if (!remote.queue.length) {
			break;
		}
	}
}


/**
 * Accept any incoming messages from the server
 */
void RA_ReadMessages(void)
{
	uint32_t ret;
	size_t expectedLength = 0;
	struct timeval tv;
	tv.tv_sec = tv.tv_usec = 0;
	message_queue_t *in;

	// save some typing
	in = &remote.queue_in;

	while (true) {
		FD_ZERO(&remote.set_r);
		FD_SET(remote.socket, &remote.set_r);

		// see if there is data waiting in the buffer for us to read
		ret = select(remote.socket + 1, &remote.set_r, NULL, NULL, &tv);

		// socket read buffer has data waiting in it
		if (ret == 1) {
			ret = recv(remote.socket, in->data + in->length, QUEUE_SIZE - 1, 0);
			if (ret <= 0) {
				RA_DisconnectedPeer();
				return;
			}

			in->length += ret;
		} else if (ret < 0) {
			RA_DisconnectedPeer();
			return;
		} else {
			// no data has been sent to read
			break;
		}
	}

	// if we made it here, we have the whole message, parse it
	RA_ParseMessage();
}

/**
 * Parse a newly received message and act accordingly
 */
void RA_ParseMessage(void)
{
	byte cmd;

	// no data
	if (!remote.queue_in.length) {
		return;
	}

	cmd = RA_ReadByte();

	switch (cmd) {
	case SCMD_PONG:
		remote.ping.waiting = false;
		remote.ping.miss_count = 0;
		break;
	case SCMD_COMMAND:
		RA_ParseCommand();
		break;
	case SCMD_HELLOACK:
		remote.ready = true;
		RA_PlayerList();
		break;
	case SCMD_SAYCLIENT:
		RA_SayClient();
		break;
	}

	// reset queue back to zero
	q2a_memset(&remote.queue_in, 0, sizeof(message_queue_t));
}

/**
 * Server sent over a command.
 */
void RA_ParseCommand(void)
{
	char *cmd;
	cmd = RA_ReadString();

	// cram it into the command buffer
	gi.AddCommandString(cmd);
}

/**
 * There was a sudden disconnection mid-stream. Reconnect after an
 * appropriate pause
 */
void RA_DisconnectedPeer(void)
{
	struct addrinfo *addr;
	uint32_t flags;

	if (!remote.enabled) {
		return;
	}

	// only work on connections that were considered connected
	if (!remote.state == RA_STATE_CONNECTED) {
		return;
	}

	gi.cprintf(NULL, PRINT_HIGH, "[RA] Connection lost\n");

	// save the server address, but start over
	addr = remote.addr;
	flags = remote.flags;

	q2a_memset(&remote, 0, sizeof(remote_t));

	remote.addr = addr;
	remote.enabled = 1;
	remote.flags = flags;
	remote.connect_retry_frame = RECONNECT(10);
}

/**
 * Send info about all the players connected
 */
void RA_PlayerList(void)
{
	uint8_t count, i;
	count = 0;

	for (i=0; i<remote.maxclients; i++) {
		if (proxyinfo[i].inuse) {
			count++;
		}
	}

	RA_WriteByte(CMD_PLAYERLIST);
	RA_WriteByte(count);

	for (i=0; i<remote.maxclients; i++) {
		if (proxyinfo[i].inuse) {
			RA_WriteByte(i);
			RA_WriteString("%s", proxyinfo[i].userinfo);
		}
	}
}

/**
 * Allows for RA to send frag notifications
 *
 * Self is the fragged player
 * inflictor is what did the damage (rocket, bolt, grenade)
 *   in the case of hitscan weapons, the inflictor will be the attacking player
 * attacker is the player doing the attacking
 * damage is how much dmg was done
 * point is where on the map the damage was dealt
 */
void PlayerDie_Internal(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point) {
	uint8_t id = getEntOffset(self) - 1;
	uint8_t aid = getEntOffset(attacker) - 1;
	gitem_t *weapon;

	if (self->deadflag != DEAD_DEAD) {
		if (strcmp(attacker->classname,"player") == 0) {
			RA_Frag(id, aid, proxyinfo[id].name, proxyinfo[aid].name);
		} else {
			RA_Frag(id, aid, proxyinfo[id].name, "");
		}
	}

	weapon = (gitem_t *)((attacker->client) + OTDM_CL_WEAPON_OFFSET);

	gi.dprintf("MOD: %s\n", weapon->classname);

	// call the player's real die() function
	proxyinfo[id].die(self, inflictor, attacker, damage, point);
}

/**
 * Immediately after connecting, we have to say hi, giving the server our
 * information. Once the server acknowledges we can start sending game data.
 */
void RA_SayHello(void)
{
	// don't bother if we're not fully connected yet
	if (remote.state != RA_STATE_CONNECTED) {
		return;
	}

	// we've already said hello
	if (remote.ready) {
		return;
	}

	RA_WriteByte(CMD_HELLO);
	RA_WriteLong(remoteKey);
	RA_WriteLong(Q2A_REVISION);
	RA_WriteShort(remote.port);
	RA_WriteByte(remote.maxclients);
}


/**
 * q2pro uses "net_port" while most other servers use "port". This
 * returns the value regardless.
 */
uint16_t getport(void) {
	static cvar_t *port;

	port = gi.cvar("port", "0", 0);
	if ((int) port->value) {
		return (int) port->value;
	}
	
	port = gi.cvar("net_port", "0", 0);
	if ((int) port->value) {
		return (int) port->value;
	}
	
	// fallback to default
	port = gi.cvar("port", "27910", 0);
	return (int) port->value;
}

/**
 * Reset the outgoing message buffer to zero to start a new msg
 */
void RA_InitBuffer() {
	q2a_memset(&remote.queue, 0, sizeof(message_queue_t));
}

/**
 * Read a single byte from the message buffer
 */
uint8_t RA_ReadByte(void)
{
	unsigned char b = remote.queue_in.data[remote.queue_in.index++];
	return b & 0xff;
}

/**
 * Write a single byte to the message buffer
 */
void RA_WriteByte(uint8_t b)
{
	remote.queue.data[remote.queue.length++] = b & 0xff;
}

/**
 * Read a short (2 bytes) from the message buffer
 */
uint16_t RA_ReadShort(void)
{
	return	(remote.queue_in.data[remote.queue_in.index++] +
			(remote.queue_in.data[remote.queue_in.index++] << 8)) & 0xffff;
}

/**
 * Write 2 bytes to the message buffer
 */
void RA_WriteShort(uint16_t s)
{
	remote.queue.data[remote.queue.length++] = s & 0xff;
	remote.queue.data[remote.queue.length++] = (s >> 8) & 0xff;
}

/**
 * Read 4 bytes from the message buffer
 */
int32_t RA_ReadLong(void)
{
	return	remote.queue_in.data[remote.queue_in.index++] +
			(remote.queue_in.data[remote.queue_in.index++] << 8) +
			(remote.queue_in.data[remote.queue_in.index++] << 16) +
			(remote.queue_in.data[remote.queue_in.index++] << 24);
}

/**
 * Write 4 bytes (long) to the message buffer
 */
void RA_WriteLong(uint32_t i)
{
	remote.queue.data[remote.queue.length++] = i & 0xff;
	remote.queue.data[remote.queue.length++] = (i >> 8) & 0xff;
	remote.queue.data[remote.queue.length++] = (i >> 16) & 0xff;
	remote.queue.data[remote.queue.length++] = (i >> 24) & 0xff;
}

/**
 * Read a null terminated string from the buffer
 */
char *RA_ReadString(void)
{
	static char str[MAX_STRING_CHARS];
	static char character;
	size_t i, len = 0;

	do {
		len++;
	} while (remote.queue_in.data[(remote.queue_in.index + len)] != 0);

	memset(&str, 0, MAX_STRING_CHARS);

	for (i=0; i<=len; i++) {
		character = RA_ReadByte() & 0x7f;
		strcat(str,  &character);
	}

	return str;
}


// printf-ish
void RA_WriteString(const char *fmt, ...) {
	
	uint16_t i;
	size_t len;
	char str[MAX_MSG_LEN];
	va_list argptr;
	
	va_start(argptr, fmt);
    len = vsnprintf(str, sizeof(str), fmt, argptr);
	va_end(argptr);

	len = strlen(str);
	
	if (!str || len == 0) {
		RA_WriteByte(0);
		return;
	}
	
	if (len > MAX_MSG_LEN - remote.msglen) {
		RA_WriteByte(0);
		return;
	}
	
	// perhaps don't convert to consolechars...
	for (i=0; i<len; i++) {
		remote.queue.data[remote.queue.length++] = str[i] | 0x80;
	}

	RA_WriteByte(0);
}

/**
 * Read an arbitrary amount of data from the message buffer
 */
void RA_ReadData(void *out, size_t len)
{
	memcpy(out, &(remote.queue_in.data[remote.queue_in.index]), len);
	remote.queue_in.index += len;
}

/**
 * Called when a player connects
 */
void RA_PlayerConnect(edict_t *ent)
{
	int8_t cl;
	cl = getEntOffset(ent) - 1;

	RA_WriteByte(CMD_CONNECT);
	RA_WriteByte(cl);
	RA_WriteString("%s", proxyinfo[cl].userinfo);
}

/**
 * Called when a player disconnects
 */
void RA_PlayerDisconnect(edict_t *ent)
{
	int8_t cl;
	cl = getEntOffset(ent) - 1;

	RA_WriteByte(CMD_DISCONNECT);
	RA_WriteByte(cl);
}

void RA_PlayerCommand(edict_t *ent) {
	
}

/**
 * Called for every broadcast print (bprintf), but only
 * on dedicated servers
 */
void RA_Print(uint8_t level, char *text)
{
	if (!(remote.flags & RFL_CHAT)) {
		return;
	}
	
	RA_WriteByte(CMD_PRINT);
	RA_WriteByte(level);
	RA_WriteString("%s",text);
}

/**
 * Called when a player issues the teleport command
 */
void RA_Teleport(uint8_t client_id)
{
	if (!(remote.flags & RFL_TELEPORT)) {
		return;
	}

	char *srv;
	if (gi.argc() > 1) {
		srv = gi.argv(1);
	} else {
		srv = "";
	}

	RA_WriteByte(CMD_COMMAND);
	RA_WriteByte(CMD_COMMAND_TELEPORT);
	RA_WriteByte(client_id);
	RA_WriteString("%s", srv);
}

/**
 * Called when a player changes part of their userinfo.
 * ex: name, skin, gender, rate, etc
 */
void RA_PlayerUpdate(uint8_t cl, const char *ui)
{
	RA_WriteByte(CMD_PLAYERUPDATE);
	RA_WriteByte(cl);
	RA_WriteString("%s", ui);
}

/**
 * Called when a player issues the invite command
 */
void RA_Invite(uint8_t cl, const char *text)
{
	if (!(remote.flags & RFL_INVITE)) {
		return;
	}

	RA_WriteByte(CMD_COMMAND);
	RA_WriteByte(CMD_COMMAND_INVITE);
	RA_WriteByte(cl);
	RA_WriteString(text);
}

/**
 * Called when a player issues the whois command
 */
void RA_Whois(uint8_t cl, const char *name)
{
	if (!(remote.flags & RFL_WHOIS)) {
		return;
	}

	RA_WriteByte(CMD_COMMAND);
	RA_WriteByte(CMD_COMMAND_WHOIS);
	RA_WriteByte(cl);
	RA_WriteString(name);
}

/**
 * Called when a player dies
 */
void RA_Frag(uint8_t victim, uint8_t attacker, const char *vname, const char *aname)
{
	if (!(remote.flags & RFL_FRAGS)) {
		return;
	}

	RA_WriteByte(CMD_FRAG);
	RA_WriteByte(victim);
	RA_WriteString("%s", vname);
	RA_WriteByte(attacker);
	RA_WriteString("%s", aname);
}

/**
 * Called when the map changes
 */
void RA_Map(const char *mapname)
{
	RA_WriteByte(CMD_MAP);
	RA_WriteString("%s", mapname);
}


/**
 * XOR part of the message with a secret key only known to this q2 server
 * and the remote admin server.
 */
void RA_Encrypt(void) {
	uint16_t i;

	/*
	 * Start 4 bytes into the message. We need the remoteKey (first 4 bytes)
	 * to be unencrypted so we know which server the message is from and
	 * we can pick the appropriate key to decrypt the rest of the message
	 */
	for (i=3; i<remote.queue.length; i++) {
		remote.queue.data[i] ^= encryptionKey[i];
	}
}


/**
 * Write something to a client
 */
void RA_SayClient(void)
{
	uint8_t client_id;
	uint8_t level;
	char *string;
	edict_t *ent;

	client_id = RA_ReadByte();
	level = RA_ReadByte();
	string = RA_ReadString();

	ent = proxyinfo[client_id].ent;

	if (!ent) {
		return;
	}

	gi.cprintf(ent, level, string);
}
