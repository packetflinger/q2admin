
#include "g_local.h"


remote_t remote;

// remote admin specific cvars
cvar_t	*remote_enabled;
cvar_t	*remote_server;
cvar_t	*remote_port;
cvar_t	*remote_key;
cvar_t	*remote_flags;
cvar_t	*remote_cmd_teleport;
cvar_t	*remote_cmd_invite;
cvar_t	*remote_cmd_seen;
cvar_t	*remote_cmd_whois;
cvar_t	*net_port;


void RA_Send(remote_cmd_t cmd, const char *fmt, ...) {

	va_list     argptr;
    char        string[MAX_STRING_CHARS];
	size_t      len;
	
	if (!remote.enabled) {
		return;
	}
	
	va_start(argptr, fmt);
    len = g_vsnprintf(string, sizeof(string), fmt, argptr);
    va_end(argptr);
	
	if (len >= sizeof(string)) {
        return;
    }
	
	gchar *final = g_strconcat(stringf("%s\\%d\\", remote_key->string, cmd), string, NULL);
	
	if (remote.flags & REMOTE_FL_DEBUG) {
		gi.dprintf("RA - Sending: %s\n", final);
	}

	int r = sendto(
		remote.socket, 
		final, 
		strlen(final)+1, 
		MSG_DONTWAIT, 
		remote.addr->ai_addr, 
		remote.addr->ai_addrlen
	);
	
	if (r == -1) {
		gi.dprintf("RA: error sending data: %s\n", strerror(errno));
	}
	
	g_free(final);
}


void RA_Init() {
	
	memset(&remote, 0, sizeof(remote));

	remote_enabled = gi.cvar("remote_enabled", "1", CVAR_LATCH | CVAR_SERVERINFO);
	remote_server = gi.cvar("remote_server", "packetflinger.com", CVAR_LATCH);
	remote_port = gi.cvar("remote_port", "9999", CVAR_LATCH);
	remote_key = gi.cvar("remote_key", "beefwellingon", CVAR_LATCH);
	remote_flags = gi.cvar("remote_flags", "28", CVAR_LATCH | CVAR_SERVERINFO);
	remote_cmd_teleport = gi.cvar("remote_cmd_teleport", "!teleport", CVAR_LATCH);
	remote_cmd_invite = gi.cvar("remote_cmd_invite", "!invite", CVAR_LATCH);
	remote_cmd_seen = gi.cvar("remote_cmd_seen", "!seen", CVAR_LATCH);
	remote_cmd_whois = gi.cvar("remote_cmd_teleport", "!whois", CVAR_LATCH);

	net_port = gi.cvar("net_port", "27910", CVAR_LATCH);
	maxclients = gi.cvar("maxclients", "64", CVAR_LATCH);
	
	if (g_strcmp0(remote_enabled->string, "0") == 0)
		return;
	
	gi.dprintf("\nRA: Remote Admin Init\n");
	
	struct addrinfo hints, *res = 0;
	memset(&hints, 0, sizeof(hints));
	memset(&res, 0, sizeof(res));
	
	hints.ai_family         = AF_INET;   	// ipv4 only
	hints.ai_socktype       = SOCK_DGRAM;	// UDP
	hints.ai_protocol       = 0;
	hints.ai_flags          = AI_ADDRCONFIG;
	
	gi.dprintf("RA: looking up %s... ", remote_server->string);
	int err = getaddrinfo(remote_server->string, remote_port->string, &hints, &res);
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
		gi.dprintf("Unable to open socket to %s:%s...disabling remote admin\n", remote_server->string, remote_port->string);
		remote.enabled = 0;
		return;
	}
	
	remote.socket = fd;
	remote.addr = res;
	remote.flags = atoi(remote_flags->string);

	// remove later - set debug while under development
	remote.flags |= REMOTE_FL_DEBUG;

	remote.enabled = 1;
}


void RA_RunFrame() {
	
	if (!remote.enabled) {
		return;
	}

	uint8_t i;

	// report if necessary
	if (remote.next_report <= remote.frame_number) {
		RA_Send(CMD_REGISTER, "%s\\%d\\%s\\%d\\%d", remote.mapname, remote.maxclients, remote.rcon_password, remote.port, remote.flags);
		remote.next_report = remote.frame_number + SECS_TO_FRAMES(60);
	}

	for (i=0; i<=remote.maxclients; i++) {
		if (proxyinfo[i].inuse) {

			if (!proxyinfo[i].remote_reported) {
				RA_Send(CMD_CONNECT, "%d\\%s", i, proxyinfo[i].userinfo);
				proxyinfo[i].remote_reported = 1;
			}

			// replace player edict's die() pointer
			if (*proxyinfo[i].ent->die != PlayerDie_Internal) {
				proxyinfo[i].die = *proxyinfo[i].ent->die;
				proxyinfo[i].ent->die = &PlayerDie_Internal;
			}
		}
	}

	remote.frame_number++;
}

void PlayerDie_Internal(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point) {
	uint8_t id = getEntOffset(self) - 1;
	uint8_t aid = getEntOffset(attacker) - 1;
	
	if (self->deadflag != DEAD_DEAD) {	
		gi.dprintf("self: %s\t inflictor: %s\t attacker %s\n", self->classname, inflictor->classname, attacker->classname);
		
		if (g_strcmp0(attacker->classname, "player") == 0 && attacker->client) {
			RA_Send(CMD_FRAG, "%d\\%d\\%s", id, aid, 
				attacker->client->pers.weapon->classname
			);
		}
	}
	
	proxyinfo[id].die(self, inflictor, attacker, damage, point);
}
