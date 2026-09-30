/**
 * Q2Admin
 * Identifying which engine a client is running, from the version string it
 * reports, and noticing when it connected with less protocol than it can
 * speak.
 *
 * Shortly after a player joins, q2admin stuffs a probe that makes the client
 * send back its "version" cvar (QCMD_CLIENTVERSION, landing in
 * proxyinfo[].client_version). That string names the engine, and the engine
 * tells us the highest network protocol that client is capable of.
 *
 * Comparing that against the protocol actually negotiated - the "major" key
 * from the extra userinfo, in proxyinfo[].protocol_major - catches a client
 * that connected with less than it is capable of. That alone proves nothing;
 * pinning cl_protocol is something people legitimately do when a server
 * misbehaves. But cheat clients frequently pose as an older, simpler protocol
 * to sidestep newer server-side checks, so it earns a signal rather than a
 * kick.
 *
 * Everything here rests on a self-reported, trivially forged string. Nothing
 * it produces is authenticated, and a client that lies about its version
 * simply isn't identified.
 */

#pragma once

// Network protocol versions, as negotiated at connect time and handed to us
// in the extra userinfo "major" key. See ClientConnect().
#define PROTOCOL_VANILLA    34   // id's original, every client speaks this
#define PROTOCOL_R1Q2       35
#define PROTOCOL_Q2PRO      36

// max_protocol for an engine nobody has researched yet. Such a client is
// still identified by name, but is never treated as downgraded, so an
// unverified entry can't invent a signal. Prefer this over guessing high:
// guessing low only costs a missed detection, guessing high accuses a player.
#define PROTOCOL_UNKNOWN    0

/**
 * Engines a client might be running. Several engines answer to more than one
 * version string, so this is not one-to-one with the rows of clientDefs[].
 */
typedef enum {
    CLIENT_UNKNOWN,
    CLIENT_VANILLA,
    CLIENT_Q2PRO,
    CLIENT_R1Q2,
    CLIENT_Q2RTX,
    CLIENT_AQTION,
    CLIENT_YAMAGI,
    CLIENT_VKQUAKE2,
    CLIENT_KMQUAKE2,
    CLIENT_APRQ2,
    CLIENT_EGL,
    CLIENT_BERSERKER,
    CLIENT_QUAKE2MAXX,
    CLIENT_QUAKE2XP,
    CLIENT_JAKE2,
} client_id_t;

/**
 * One way of recognising one engine.
 *
 * pattern is matched against a lowercased copy of the client's version
 * string, so it must itself be lowercase. It is a tiny-regex pattern, which
 * means no alternation and no grouping - an engine that needs two spellings
 * gets two rows sharing an id.
 */
typedef struct {
    client_id_t id;
    const char *name;         // for display and logging
    int max_protocol;         // best this engine can speak, or PROTOCOL_UNKNOWN
    const char *pattern;
} client_def_t;

const client_def_t *identifyClient(const char *version);
void checkProtocolDowngrade(int client);
