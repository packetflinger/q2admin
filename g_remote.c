#include "g_local.h"

remote_t remote;
cvar_t	*udpport;

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
	
	RA_InitBuffer();
}


void RA_Init() {
	
	memset(&remote, 0, sizeof(remote));
	maxclients = gi.cvar("maxclients", "64", CVAR_LATCH);
	
	if (!remoteEnabled) {
		gi.dprintf("Remote Admin is disabled in your config file.\n");
		return;
	}
	
	gi.dprintf("[RA] Remote Admin Init...\n");
	
	struct addrinfo hints, *res = 0;
	memset(&hints, 0, sizeof(hints));
	memset(&res, 0, sizeof(res));
	
	hints.ai_family         = AF_INET;   	// either v6 or v4
	hints.ai_socktype       = SOCK_DGRAM;	// UDP
	hints.ai_protocol       = 0;
	hints.ai_flags          = AI_ADDRCONFIG;
	
	gi.dprintf("[RA] looking up %s... ", remoteAddr);

	int err = getaddrinfo(remoteAddr, va("%d",remotePort), &hints, &res);
	if (err != 0) {
		gi.dprintf("error, disabling\n");
		remote.enabled = 0;
		return;
	} else {
		char address[INET_ADDRSTRLEN];
		inet_ntop(res->ai_family, &((struct sockaddr_in *)res->ai_addr)->sin_addr, address, sizeof(address));
		gi.dprintf("%s\n", address);
	}
	
	int fd = socket(res->ai_family, res->ai_socktype, IPPROTO_UDP);
	if (fd == -1) {
		gi.dprintf("Unable to open socket to %s:%d...disabling remote admin\n", remoteAddr, remotePort);
		remote.enabled = 0;
		return;
	}
	
	remote.socket = fd;
	remote.addr = res;
	remote.flags = remoteFlags;
	remote.enabled = 1;
	remote.online = 1;	// just for testing
}


void RA_RunFrame() {
	
	if (!remote.enabled) {
		return;
	}

	uint8_t i;

	// report server if necessary
	if (remote.next_report <= remote.frame_number) {
		//RA_Send(CMD_SHEARTBEAT, "%s\\%d\\%s\\%d\\%d", remote.mapname, remote.maxclients, remote.rcon_password, remote.port, remote.flags);
		remote.next_report = remote.frame_number + SECS_TO_FRAMES(60);
	}


	for (i=0; i<=remote.maxclients; i++) {
		if (proxyinfo[i].inuse) {

			if (proxyinfo[i].next_report <= remote.frame_number) {
				//RA_Send(CMD_PHEARTBEAT, "%d\\%s", i, proxyinfo[i].userinfo);
				proxyinfo[i].next_report = remote.frame_number + SECS_TO_FRAMES(60);
			}

			/*
			if (!proxyinfo[i].remote_reported) {
				RA_Send(CMD_CONNECT, "%d\\%s", i, proxyinfo[i].userinfo);
				proxyinfo[i].remote_reported = 1;
			}

			// replace player edict's die() pointer
			if (*proxyinfo[i].ent->die != PlayerDie_Internal) {
				proxyinfo[i].die = *proxyinfo[i].ent->die;
				proxyinfo[i].ent->die = &PlayerDie_Internal;
			}
			*/
		}
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


void PlayerDie_Internal(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point) {
	uint8_t id = getEntOffset(self) - 1;
	uint8_t aid = getEntOffset(attacker) - 1;
	
	if (self->deadflag != DEAD_DEAD) {	
		gi.dprintf("self: %s\t inflictor: %s\t attacker %s\n", self->classname, inflictor->classname, attacker->classname);
		
		// crater, drown (water, acid, lava)
		if (g_strcmp0(attacker->classname, "worldspawn") == 0) {
			//RA_Send(CMD_FRAG,"%d\\%d\\worldspawn", id, aid);
		} else if (g_strcmp0(attacker->classname, "player") == 0 && attacker->client) {
			//gi.dprintf("Attacker: %s\n", attacker->client->pers.netname);
			/*RA_Send(CMD_FRAG, "%d\\%d\\%s", id, aid,
				attacker->client->pers.weapon->classname
			);*/
		}
	}
	
	proxyinfo[id].die(self, inflictor, attacker, damage, point);
}

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
	RA_WriteShort(remote.port);
	RA_WriteByte(remote.maxclients);
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
