/**
 * Q2Admin
 * Client identification by version string. See g_protocol.h for what this is
 * for and why its results are advisory.
 */

#include "g_local.h"

/**
 * Every engine we know how to recognise, and the best protocol each can
 * speak.
 *
 * Order is match priority: the first pattern that matches wins, so anything
 * that could be mistaken for something else goes above it. The bare version
 * number of stock Quake II is last, because a fork that never changed its
 * "version" cvar still reports "3.2x" and should be given every chance to be
 * recognised by name first.
 *
 * On the protocol column: 34 is the safe answer whenever there is any doubt.
 * A client is only flagged when it connects BELOW the number in this table,
 * so listing an engine too low costs at most a missed detection, while
 * listing one too high raises a signal against a player who did nothing.
 * Entries marked "inferred" below are reasoned from the engine's lineage
 * rather than read out of its source, and are deliberately conservative.
 */
static const client_def_t clientDefs[] = {
    // Verified: these three define the protocol numbers everyone else
    // implements, and their version strings are unmistakable.
    { CLIENT_Q2PRO,      "Q2PRO",       PROTOCOL_Q2PRO,    "q2pro" },
    { CLIENT_R1Q2,       "R1Q2",        PROTOCOL_R1Q2,     "r1q2" },

    // Inferred: both are built on the Q2PRO codebase and keep its netcode.
    { CLIENT_Q2RTX,      "Q2RTX",       PROTOCOL_Q2PRO,    "q2rtx" },
    { CLIENT_AQTION,     "AQtion",      PROTOCOL_Q2PRO,    "aqtion" },

    // Yamagi deliberately stays wire-compatible with stock Quake II. It has
    // answered to both spellings over the years.
    { CLIENT_YAMAGI,     "Yamagi",      PROTOCOL_VANILLA,  "yamagi" },
    { CLIENT_YAMAGI,     "Yamagi",      PROTOCOL_VANILLA,  "yquake2" },

    // Inferred: forks of the original id release, listed at 34 because that
    // is the floor, not because each was confirmed to stop there. Raising one
    // of these is what turns the check on for that engine.
    { CLIENT_VKQUAKE2,   "vkQuake2",    PROTOCOL_VANILLA,  "vkquake2" },
    { CLIENT_KMQUAKE2,   "KMQuake2",    PROTOCOL_VANILLA,  "kmquake2" },
    { CLIENT_APRQ2,      "AprQ2",       PROTOCOL_VANILLA,  "aprq2" },
    { CLIENT_EGL,        "EGL",         PROTOCOL_VANILLA,  "egl v" },
    { CLIENT_BERSERKER,  "Berserker",   PROTOCOL_VANILLA,  "berserker" },
    { CLIENT_QUAKE2XP,   "Quake2xp",    PROTOCOL_VANILLA,  "quake2xp" },
    { CLIENT_QUAKE2MAXX, "Quake2Maxx",  PROTOCOL_VANILLA,  "quake2maxx" },
    { CLIENT_QUAKE2MAXX, "Quake2Maxx",  PROTOCOL_VANILLA,  "q2max" },
    { CLIENT_JAKE2,      "Jake2",       PROTOCOL_VANILLA,  "jake2" },

    // Stock Quake II reports "3.20"/"3.21" followed by arch and build date.
    // Anchored so it can't match a version number buried in a longer string.
    { CLIENT_VANILLA,    "Quake II",    PROTOCOL_VANILLA,  "^3\\.2" },
};

/**
 * Work out which engine a version string came from.
 *
 * Matching is done on a lowercased copy so the table can hold one spelling
 * per engine, and is unanchored unless the pattern says otherwise, so decoration
 * around the engine name doesn't matter.
 *
 * version: the string the client sent back, ie proxyinfo[].client_version.
 *          Safe to pass NULL or "".
 *
 * Returns the matching row of clientDefs[], which is static and outlives the
 * caller, or NULL if nothing matched.
 */
const client_def_t *identifyClient(const char *version) {
    char lower[MAX_VERSION_CHARS];
    int len;

    if (!version || !version[0]) {
        return NULL;
    }

    q2a_strncpy(lower, version, sizeof(lower) - 1);
    lower[sizeof(lower) - 1] = 0;
    lowerCase(lower);

    for (unsigned int i = 0; i < lengthof(clientDefs); i++) {
        // re_match() gives back the offset the match was found at, or -1 for
        // no match. Anything but -1 means the pattern is in there somewhere.
        if (re_match(clientDefs[i].pattern, lower, &len) != -1) {
            return &clientDefs[i];
        }
    }

    return NULL;
}

/**
 * Raise SIGNAL_PROTOCOL_DOWNGRADE if this client negotiated a lower protocol
 * than the engine it claims to be is capable of.
 *
 * Silently does nothing unless all three facts are in hand: the protocol the
 * client actually connected with, a recognised engine, and a known ceiling
 * for that engine. Any of those missing means there is nothing to compare,
 * not that something is wrong.
 *
 * client: the proxyinfo index of the player to check.
 *
 * Called from doClientCommand() once the client answers the version probe,
 * which is the first moment both halves of the comparison exist.
 */
void checkProtocolDowngrade(int client) {
    const client_def_t *def;
    proxyinfo_t *cl;

    if (!VALIDCLIENT(client)) {
        return;
    }

    cl = &proxyinfo[client];

    // protocol_major only reaches us through the extra userinfo block, which
    // needs GMF_EXTRA_USERINFO on both sides. 0 means we were never told what
    // the client connected with, so there is nothing to compare against.
    if (cl->protocol_major <= 0) {
        return;
    }

    def = identifyClient(cl->client_version);
    if (!def || def->max_protocol == PROTOCOL_UNKNOWN) {
        return;
    }

    if (cl->protocol_major < def->max_protocol) {
        raiseSignal(client, SIGNAL_PROTOCOL_DOWNGRADE);
    }
}
