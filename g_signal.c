/*
Copyright (C) 2000 Shane Powell

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

 */

#include "g_local.h"

int signal_score_threshold = 50; // 0 disables score-based removal

typedef struct {
    unsigned int bit;
    int weight;
    const char *name;
} signal_def_t;

// Not const: weights are overridable at runtime/via cfg, see
// findSignalDef()/signalWeightRun()/signalWeightInit() below. The values
// here are just the defaults.
static signal_def_t signalDefs[] = {
    { SIGNAL_AIMBOT_JITTER,             25,  "aimbot-jitter" },
    { SIGNAL_CHATFLOOD,                 15,  "chatflood" },
    { SIGNAL_SNAP_FIRE,                 40,  "snap-fire" },
    { SIGNAL_AIM_TRACK,                 30,  "aim-track" },
    { SIGNAL_SKIN_OVERFLOW,             25,  "skin-overflow" },
    { SIGNAL_VERSION_DEADLINE,          20,  "version-probe" },
    { SIGNAL_TIMESCALE_DEADLINE,        20,  "timescale-probe" },
    { SIGNAL_ALIAS_DEADLINE,            20,  "alias-probe" },
    { SIGNAL_CHECKVAR_DEADLINE,         20,  "checkvar-probe" },
    { SIGNAL_MSEC_OVERRUN,              20,  "msec-overrun" },
    { SIGNAL_MSEC_UNDERRUN,             20,  "msec-underrun" },
    { SIGNAL_IMPULSE,                   10,  "impulse-sent" },
    { SIGNAL_MANUAL,                     0,  "admin-adjustment" },
    { SIGNAL_BAN_ADJUSTMENT,             0,  "ban-entry" },
    { SIGNAL_VPN_SUSPICIOUS,            20,  "vpn-suspicious" },
    { SIGNAL_VPN_LIKEY,                 30,  "vpn-likely" },
    { SIGNAL_VPN_DETECTED,              40,  "vpn-detected" },
    { SIGNAL_WONKY_USERINFO,            15,  "wonky-userinfo" },
    { SIGNAL_TIMESCALE_MODIFIED,   1000000,  "timescale-modified" },
    { SIGNAL_ZBOT_DETECTED,        1000000,  "zbot-detected" },
    { SIGNAL_RATBOT_DETECTED,      1000000,  "ratbot-detected" },
    { SIGNAL_PROXY_DETECTED,       1000000,  "proxy-detected" },
    { SIGNAL_ALIAS_UNSUPPORTED,    1000000,  "alias-unsupported" },
    { SIGNAL_BAD_CLIENT,           1000000,  "bad-client" },
};

/**
 * Marks a signal as currently matching for a client. Does not by itself remove
 * the client, callers that care about crossing the removal threshold should
 * follow up with evaluateSignalScore().
 */
void raiseSignal(int client, unsigned int signal) {
    if (!VALIDCLIENT(client)) {
        return;
    }
    proxyinfo[client].signalMask |= signal;
}

/**
 * Marks a signal as no longer matching for a client.
 */
void clearSignal(int client, unsigned int signal) {
    if (!VALIDCLIENT(client)) {
        return;
    }
    proxyinfo[client].signalMask &= ~signal;
}

/**
 * Sum of the weights of every signal currently matching this client.
 */
int signalScore(int client) {
    int score = 0;
    unsigned int mask;
    proxyinfo_t *cl;

    if (!VALIDCLIENT(client)) {
        return 0;
    }

    cl = &proxyinfo[client];
    mask = cl->signalMask;
    for (unsigned int i = 0; i < lengthof(signalDefs); i++) {
        if (mask & signalDefs[i].bit) {
            if (signalDefs[i].bit == SIGNAL_IMPULSE) {
                score += (signalDefs[i].weight * cl->impulsesgenerated);
            } else if (signalDefs[i].bit == SIGNAL_BAN_ADJUSTMENT) {
                score += cl->ban_signal_score;
            } else if (signalDefs[i].bit == SIGNAL_MANUAL) {
                score += cl->manual_signal_score;
            } else {
                score += signalDefs[i].weight;
            }
        }
    }
    return score;
}

/**
 * Comma separated list of the names of every signal currently matching
 * this client, for admin display and kick messages. "(none)" if nothing
 * currently matches.
 */
char *signalListString(int client) {
    static char list[256];
    unsigned int mask;
    bool first = true;

    list[0] = 0;
    if (!VALIDCLIENT(client)) {
        return list;
    }

    mask = proxyinfo[client].signalMask;
    for (unsigned int i = 0; i < lengthof(signalDefs); i++) {
        if (mask & signalDefs[i].bit) {
            if (!first) {
                q2a_strcat(list, ", ");
            }
            if (signalDefs[i].bit == SIGNAL_BAN_ADJUSTMENT) {
                q2a_strcat(list, va("%s(%d)", signalDefs[i].name, proxyinfo[client].ban_signal_score));
            } else if (signalDefs[i].bit == SIGNAL_MANUAL) {
                q2a_strcat(list, va("%s(%d)", signalDefs[i].name, proxyinfo[client].manual_signal_score));
            } else if (signalDefs[i].bit == SIGNAL_IMPULSE) {
                q2a_strcat(list, va("%s(%d*%d)", signalDefs[i].name, signalDefs[i], proxyinfo[client].impulsesgenerated));
            } else {
                q2a_strcat(list, va("%s(%d)", signalDefs[i].name, signalDefs[i].weight));
            }
            first = false;
        }
    }

    if (first) {
        q2a_strncpy(list, "(none)", sizeof(list)-1);
    }
    return list;
}

/**
 * Checks whether a client's current signal score has reached the removal
 * threshold and, if so, disconnects them. Callers for soft/ambiguous
 * signals (aimbot jitter, vpn, chatflood) should call this after raising
 * their signal; hackDetected()'s hacktype_t signals are weighted to always
 * cross the threshold on their own but are removed via their own existing
 * disconnectuser-gated path instead, so they don't call this.
 */
void evaluateSignalScore(int client) {
    int score;

    if (!VALIDCLIENT(client) || signal_score_threshold <= 0) {
        return;
    }

    if (proxyinfo[client].clientcommand & CCMD_KICKED) {
        return;
    }

    score = signalScore(client);
    if (score >= signal_score_threshold) {
        proxyinfo[client].clientcommand |= CCMD_KICKED;
        gi.cprintf(proxyinfo[client].ent, PRINT_HIGH, "You exceeded the server's signal threshold (%d/%d)\n", score, signal_score_threshold);
        addCmdQueue(client, QCMD_DISCONNECT, 1, 0,
            va("%s tripped the signal threshold (score %d/%d): %s", NAME(client), score, signal_score_threshold, signalListString(client))
        );
    }
}

/**
 * Looks up a signal_def_t by its display name (case insensitive, as
 * shown in !signals/signalListString()), for signalWeightRun()/
 * signalWeightInit() below. NULL if nothing matches.
 */
static signal_def_t *findSignalDef(const char *name) {
    for (unsigned int i = 0; i < lengthof(signalDefs); i++) {
        if (Q_stricmp((char *) signalDefs[i].name, (char *) name) == 0) {
            return &signalDefs[i];
        }
    }
    return NULL;
}

/**
 * !signal_weight <name> [weight] - with both args, overrides how much
 * one signal contributes to signalScore(), so an admin can retune
 * detection sensitivity (including softening/hardening a hack-type
 * signal's instant-kick weight) without a rebuild. With just <name>,
 * reports that signal's current weight instead of changing anything -
 * handy for checking what's currently in effect after a q2admin.cfg
 * override. <name> is whatever's shown in !signals for that signal (e.g.
 * "aimbot-jitter", "snap-fire"); the values baked into signalDefs[]
 * above are just the defaults, overridden here or, more usually, once at
 * startup from q2admin.cfg (see signalWeightInit() below, which shares
 * this same lookup).
 *
 * SIGNAL_MANUAL and SIGNAL_BAN_ADJUSTMENT are rejected either way: their
 * contribution comes from a per-client score (manual_signal_score /
 * ban_signal_score, see signalScore()) rather than this table, so their
 * table weight is meaningless to set or view.
 */
void signalWeightRun(int startarg, edict_t *ent, int client) {
    char *name;
    signal_def_t *def;
    int weight;

    if (gi.argc() < startarg + 1) {
        gi.cprintf(ent, PRINT_HIGH, "[sv] !signal_weight <name> [weight]\n");
        return;
    }

    name = gi.argv(startarg);
    def = findSignalDef(name);
    if (!def) {
        gi.cprintf(ent, PRINT_HIGH, "unknown signal \"%s\", see !signals for valid names\n", name);
        return;
    }
    if (def->bit == SIGNAL_MANUAL || def->bit == SIGNAL_BAN_ADJUSTMENT) {
        gi.cprintf(ent, PRINT_HIGH, "%s's score doesn't come from a fixed weight, can't set/view it here\n", def->name);
        return;
    }

    if (gi.argc() < startarg + 2) {
        gi.cprintf(ent, PRINT_HIGH, "%s weight = %d\n", def->name, def->weight);
        return;
    }

    weight = q2a_atoi(gi.argv(startarg + 1));
    def->weight = weight;
    gi.cprintf(ent, PRINT_HIGH, "%s weight = %d\n", def->name, weight);
}

/**
 * CFGFILE counterpart to signalWeightRun() above: one
 * "signal_weight <name> <weight>" line per signal to override, read at startup
 * (readAdminConfig() -> readCfgFile()) so admins can retune signalDefs[]'s
 * default weights from q2admin.cfg instead of only at runtime via the
 * console/rcon.
 */
void signalWeightInit(char *arg) {
    char name[32];
    char *cp = arg;
    unsigned int i;

    SKIPBLANK(cp);
    for (i = 0; i < sizeof(name) - 1 && *cp && *cp != ' '; i++, cp++) {
        name[i] = *cp;
    }
    name[i] = 0;
    SKIPBLANK(cp);

    if (!name[0] || !(*cp == '-' || *cp == '+' || isdigit((unsigned char) *cp))) {
        return;
    }

    signal_def_t *def = findSignalDef(name);
    if (!def || def->bit == SIGNAL_MANUAL || def->bit == SIGNAL_BAN_ADJUSTMENT) {
        return;
    }
    def->weight = q2a_atoi(cp);
}

/**
 * !signaladd <player> <+/-integer> - manually adjust a player's signal
 * score by an admin-chosen amount, for evidence the automated detectors
 * don't catch (e.g. an admin spotting cheating in a demo) or to correct
 * a false positive with a negative adjustment. Accumulates into
 * manual_signal_score (repeated uses add up rather than replace) and
 * raises SIGNAL_MANUAL so the adjustment shows up in !signals and counts
 * in signalScore(); like any other signal it can push a client straight
 * over signal_score_threshold.
 */
void signaladdRun(int startarg, edict_t *ent, int client) {
    char *text;
    edict_t *enti;
    int clienti;
    int amount;

    text = getArgs();
    if (!ent) {
        while (*text != ' ') {
            text++;
        }
    }
    SKIPBLANK(text);
    enti = getClientFromArg(client, ent, &clienti, text, &text);

    if (!enti || !(*text == '-' || *text == '+' || isdigit((unsigned char) *text))) {
        gi.cprintf(ent, PRINT_HIGH, "[sv] !signaladd %s <+/-integer>\n", PLAYERSPEC);
        return;
    }

    amount = q2a_atoi(text);

    proxyinfo[clienti].manual_signal_score += amount;
    raiseSignal(clienti, SIGNAL_MANUAL);

    q2a_printf(
        "%s manual signal adjusted by %d (now %d, score %d/%d)\n",
        proxyinfo[clienti].name,
        amount,
        proxyinfo[clienti].manual_signal_score,
        signalScore(clienti),
        signal_score_threshold
    );
}

/**
 * !signals <player> - show which signals currently match a player and
 * their resulting score, even if that score is below the removal
 * threshold.
 */
void signalsRun(int startarg, edict_t *ent, int client) {
    char *text;
    edict_t *enti;
    int clienti;
    char tmptext[320];

    text = getArgs();
    if (!ent) {
        while (*text != ' ') {
            text++;
        }
    }
    SKIPBLANK(text);
    enti = getClientFromArg(client, ent, &clienti, text, &text);

    if (enti) {
        Q_snprintf(
            tmptext,
            sizeof(tmptext),
            "%s signals (score %d/%d): %s\n",
            proxyinfo[clienti].name,
            signalScore(clienti),
            signal_score_threshold,
            signalListString(clienti)
        );
        cprintf_internal(ent, PRINT_HIGH, "%s", tmptext);
    } else {
        gi.cprintf(ent, PRINT_HIGH, "[sv] !signals %s\n", PLAYERSPEC);
    }
}
