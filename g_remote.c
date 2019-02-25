#include "g_local.h"

remote_t remote;
cvar_t	*udpport;

/**
 * Sends the contents of the msg buffer to the RA server
 *
 */
void RA_Send() {

	if (!(remote.enabled && remote.online)) {
		return;
	}

	int r = sendto(
		remote.socket, 
		remote.msg,
		remote.msglen, 
		MSG_DONTWAIT, 
		remote.addr->ai_addr, 
		remote.addr->ai_addrlen
	);
	
	if (r == -1) {
		gi.dprintf("[RA] error sending data: %s\n", strerror(errno));
	}
	
	remote.next_report = remote.frame_number + SECS_TO_FRAMES(10);

	// reset the msg buffer for the next one
	RA_InitBuffer();
}


/**
 * Sets up RA connection. Makes sure we have what we need:
 * - enabled in config
 * - rcon password is set
 *
 */
void RA_Init() {
	
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
	hints.ai_socktype       = SOCK_DGRAM;	// UDP
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
	
	int fd = socket(res->ai_family, res->ai_socktype, IPPROTO_UDP);
	if (fd == -1) {
		gi.cprintf(NULL, PRINT_HIGH, "[RA] Unable to open socket to %s:%d...disabling\n", remoteAddr, remotePort);
		remote.enabled = 0;
		return;
	}
	
	remote.socket = fd;
	remote.addr = res;
	remote.flags = remoteFlags;
	remote.enabled = 1;
	remote.online = 1;	// just for testing
}

/**
 * Run once per server frame
 *
 */
void RA_RunFrame() {
	
	if (!remote.enabled) {
		return;
	}

	static uint8_t i;

	for (i=0; i<=remote.maxclients; i++) {
		if (proxyinfo[i].inuse) {

			// replace player edict's die() pointer
			if (*proxyinfo[i].ent->die != PlayerDie_Internal) {
				proxyinfo[i].die = *proxyinfo[i].ent->die;
				proxyinfo[i].ent->die = &PlayerDie_Internal;
			}
		}
	}

	// report that we're still alive in case we're idle
	if (remote.next_report >= remote.frame_number) {
		RA_HeartBeat();
	}

	remote.frame_number++;
}

void RA_Shutdown() {
	if (!remote.enabled) {
		return;
	}

	gi.cprintf(NULL, PRINT_HIGH, "[RA] Unregistering with remote admin server\n\n");
	RA_Unregister();
	freeaddrinfo(remote.addr);
}

/**
 * Allows for RA to send frag notifications
 */
void PlayerDie_Internal(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point) {
	uint8_t id = getEntOffset(self) - 1;
	uint8_t aid = getEntOffset(attacker) - 1;
	
	if (self->deadflag != DEAD_DEAD) {
		if (strcmp(attacker->classname,"player") == 0) {
			RA_Frag(id, aid, proxyinfo[id].name, proxyinfo[aid].name);
		} else {
			RA_Frag(id, aid, proxyinfo[id].name, "");
		}
	}
	
	// call the player's real die() function
	proxyinfo[id].die(self, inflictor, attacker, damage, point);
}

/**
 * q2pro uses "net_port" while most other servers use "port". This
 * returns the value regardless.
 *
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
	
	port = gi.cvar("port", "27910", 0);
	return (int) port->value;
}

void RA_InitBuffer() {
	q2a_memset(&remote.msg, 0, sizeof(remote.msg));
	remote.msglen = 0;
}

void RA_WriteByte(uint8_t b) {
	remote.msg[remote.msglen++] = b & 0xff;
}

void RA_WriteShort(uint16_t s){
	remote.msg[remote.msglen++] = s & 0xff;
	remote.msg[remote.msglen++] = (s >> 8) & 0xff;
}

void RA_WriteLong(uint32_t i){
	remote.msg[remote.msglen++] = i & 0xff;
	remote.msg[remote.msglen++] = (i >> 8) & 0xff;
	remote.msg[remote.msglen++] = (i >> 16) & 0xff;
	remote.msg[remote.msglen++] = (i >> 24) & 0xff;
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
	
	for (i=0; i<len; i++) {
		remote.msg[remote.msglen++] = str[i] | 0x80;
	}

	RA_WriteByte(0);
}

void RA_Register(void) {
	RA_WriteLong(remoteKey);
	RA_WriteByte(CMD_REGISTER);
	RA_WriteLong(Q2A_REVISION);
	RA_WriteShort(remote.port);
	RA_WriteByte(remote.maxclients);
	RA_WriteString("%s", remote.rcon_password);
	RA_WriteString("%s", remote.mapname);
	RA_Send();
}

void RA_Unregister(void) {
	RA_WriteLong(remoteKey);
	RA_WriteByte(CMD_QUIT);
	RA_Send();
}

void RA_PlayerConnect(edict_t *ent) {
	int8_t cl;
	cl = getEntOffset(ent) - 1;
	RA_WriteLong(remoteKey);
	RA_WriteByte(CMD_CONNECT);
	RA_WriteByte(cl);
	RA_WriteString("%s", proxyinfo[cl].userinfo);
	RA_Send();
}

void RA_PlayerDisconnect(edict_t *ent) {
	int8_t cl;
	cl = getEntOffset(ent) - 1;
	RA_WriteLong(remoteKey);
	RA_WriteByte(CMD_DISCONNECT);
	RA_WriteByte(cl);
	RA_Send();
}

void RA_PlayerCommand(edict_t *ent) {
	
}

void RA_Print(uint8_t level, char *text) {
	RA_WriteLong(remoteKey);
	RA_WriteByte(CMD_PRINT);
	RA_WriteByte(level);
	RA_WriteString("%s",text);
	RA_Send();
}

void RA_Teleport(uint8_t client_id) {
	char *srv;
	if (gi.argc() > 1) {
		srv = gi.argv(1);
	} else {
		srv = "";
	}

	RA_WriteLong(remoteKey);
	RA_WriteByte(CMD_TELEPORT);
	RA_WriteByte(client_id);
	RA_WriteString("%s", srv);
	RA_Send();
}

void RA_PlayerUpdate(uint8_t cl, const char *ui) {
	RA_WriteLong(remoteKey);
	RA_WriteByte(CMD_PLAYERUPDATE);
	RA_WriteByte(cl);
	RA_WriteString("%s", ui);
	RA_Send();
}

void RA_Invite(uint8_t cl, const char *text) {
	RA_WriteLong(remoteKey);
	RA_WriteByte(CMD_INVITE);
	RA_WriteByte(cl);
	RA_WriteString(text);
	RA_Send();
}

void RA_Whois(uint8_t cl, const char *name) {
	RA_WriteLong(remoteKey);
	RA_WriteByte(CMD_WHOIS);
	RA_WriteByte(cl);
	RA_WriteString(name);
	RA_Send();
}

void RA_Frag(uint8_t victim, uint8_t attacker, const char *vname, const char *aname) {
	RA_WriteLong(remoteKey);
	RA_WriteByte(CMD_FRAG);
	RA_WriteByte(victim);
	RA_WriteString("%s", vname);
	RA_WriteByte(attacker);
	RA_WriteString("%s", aname);
	RA_Send();
}

void RA_Map(const char *mapname) {
	RA_WriteLong(remoteKey);
	RA_WriteByte(CMD_MAP);
	RA_WriteString("%s", mapname);
	RA_Send();
}

void RA_Authorize(const char *authkey) {
	RA_WriteLong(-1);
	RA_WriteByte(CMD_AUTHORIZE);
	RA_WriteString("%s", authkey);
	RA_WriteString("%s", remote.rcon_password);
	RA_Send();
}

void RA_HeartBeat(void) {
	RA_WriteLong(remoteKey);
	RA_WriteByte(CMD_HEARTBEAT);
	RA_Send();
}

