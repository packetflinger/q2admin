
#include "g_local.h"


remote_t remote;

// remote admin specific cvars
cvar_t		*remote_enabled;
cvar_t		*remote_server;
cvar_t		*remote_port;
cvar_t		*remote_key;
cvar_t		*remote_flags;
cvar_t		*net_port;


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
	
	gi.dprintf("Sending: %s\n", final);
	
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
	
	remote_enabled = gi.cvar("remote_enabled", "1", CVAR_LATCH | CVAR_SERVERINFO);
	remote_server = gi.cvar("remote_server", "packetflinger.com", CVAR_LATCH);
	remote_port = gi.cvar("remote_port", "9999", CVAR_LATCH);
	remote_key = gi.cvar("remote_key", "beefwellingon", CVAR_LATCH);
	remote_flags = gi.cvar("remote_flags", "7", CVAR_LATCH | CVAR_SERVERINFO);
	net_port = gi.cvar("net_port", "27910", CVAR_LATCH);
	maxclients = gi.cvar("maxclients", "64", CVAR_LATCH);
	
	if (g_strcmp0(remote_enabled->string, "0") == 0)
		return;

	remote.enabled = 1;
	
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
}


void Remote_RunFrame() {
	
	uint8_t i;
	static uint8_t mc = 0;
	if (mc == 0) 
		mc = maxclients->value;
	
	// hijack each player entity's die() pointer, it gets reset on spawn.
	for (i=0; i<=mc; i++) {
		if (proxyinfo[i].inuse) {
			if (*proxyinfo[i].ent->die != PlayerDie_Internal) {
				proxyinfo[i].die = *proxyinfo[i].ent->die;
				proxyinfo[i].ent->die = &PlayerDie_Internal;
			}
		}
	}
}

// hijack the real player_die function to get frag info to send
void PlayerDie_Internal(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point) {
	uint8_t id = getEntOffset(self) - 1;
	uint8_t aid = getEntOffset(attacker) - 1;
	
	if (self->deadflag != DEAD_DEAD) {	
		gi.dprintf("self: %s\t inflictor: %s\t attacker %s\n", self->classname, inflictor->classname, attacker->classname);
		
		// another player killed us
		if (g_strcmp0(attacker->classname, "player") == 0 && attacker->client) {
			RA_Send(CMD_FRAG, "%d\\%d\\%s", id, aid, 
				attacker->client->pers.weapon->classname
			);
		}
	}
	
	// call the real die()
	proxyinfo[id].die(self, inflictor, attacker, damage, point);
}
