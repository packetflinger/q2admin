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

// Weight large enough that a single one of these signals always reaches
// signal_score_threshold on its own, so hacktype_t-derived signals keep
// removing a client immediately, same as before this system existed.
#define SIGNAL_SCORE_KICK   1000000

int signal_score_threshold = 50; // 0 disables score-based removal

typedef struct {
    unsigned int bit;
    int weight;
    const char *name;
} signal_def_t;

static const signal_def_t signalDefs[] = {
    { SIGNAL_AIMBOT_JITTER,      25,                 "aimbot-jitter" },
    { SIGNAL_VPN,                25,                 "vpn" },
    { SIGNAL_CHATFLOOD,          15,                 "chatflood" },
    { SIGNAL_SNAP_FIRE,          40,                 "snap-fire" },
    { SIGNAL_AIM_TRACK,          30,                 "aim-track" },
    { SIGNAL_SKIN_OVERFLOW,      25,                 "skin-overflow" },
    { SIGNAL_VERSION_DEADLINE,   20,                 "version-probe" },
    { SIGNAL_TIMESCALE_DEADLINE, 20,                 "timescale-probe" },
    { SIGNAL_ALIAS_DEADLINE,     20,                 "alias-probe" },
    { SIGNAL_CHECKVAR_DEADLINE,  20,                 "checkvar-probe" },
    { SIGNAL_MSEC_OVERRUN,       20,                 "msec-overrun" },
    { SIGNAL_MSEC_UNDERRUN,      20,                 "msec-underrun" },
    { SIGNAL_IMPULSE,            10,                 "impulse-sent" },
    { SIGNAL_MANUAL,              0,                 "admin-adjustment" },
    { SIGNAL_BAN_ADJUSTMENT,      0,                 "ban-entry" },
    { SIGNAL_ZBOT_DETECTED,      SIGNAL_SCORE_KICK,  "zbot-detected" },
    { SIGNAL_RATBOT_DETECTED,    SIGNAL_SCORE_KICK,  "ratbot-detected" },
    { SIGNAL_HACK_PROXY,         SIGNAL_SCORE_KICK,  "hack-proxy" },
    { SIGNAL_HACK_AIMBOT,        SIGNAL_SCORE_KICK,  "hack-aimbot" },
    { SIGNAL_HACK_ZBOT,          SIGNAL_SCORE_KICK,  "hack-zbot" },
    { SIGNAL_HACK_RATBOT,        SIGNAL_SCORE_KICK,  "hack-ratbot" },
    { SIGNAL_HACK_CUSTOMCLIENT,  SIGNAL_SCORE_KICK,  "hack-customclient" },
    { SIGNAL_HACK_MSEC,          SIGNAL_SCORE_KICK,  "hack-msec" },
    { SIGNAL_HACK_TIMESCALE,     SIGNAL_SCORE_KICK,  "hack-timescale" },
    { SIGNAL_HACK_ALIAS,         SIGNAL_SCORE_KICK,  "hack-alias" },
    { SIGNAL_HACK_STUFF,         SIGNAL_SCORE_KICK,  "hack-stuff" },
    { SIGNAL_HACK_USERINFO,      SIGNAL_SCORE_KICK,  "hack-userinfo" },
    { SIGNAL_HACK_UNKNOWN,       SIGNAL_SCORE_KICK,  "hack-unknown" },
};

/**
 * Marks a signal as currently matching for a client. Does not by itself
 * remove the client, callers that care about crossing the removal
 * threshold should follow up with evaluateSignalScore().
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

    if (!VALIDCLIENT(client)) {
        return 0;
    }

    mask = proxyinfo[client].signalMask;
    for (unsigned int i = 0; i < lengthof(signalDefs); i++) {
        if (mask & signalDefs[i].bit) {
            if (signalDefs[i].bit == SIGNAL_IMPULSE) {
                score += (signalDefs[i].weight * proxyinfo[client].impulsesgenerated);
            } else if (signalDefs[i].bit == SIGNAL_BAN_ADJUSTMENT) {
                score += proxyinfo[client].baninfo->signalscore;
            } else if (signalDefs[i].bit == SIGNAL_MANUAL) {
                score += proxyinfo[client].manual_signal_score;
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
            q2a_strcat(list, signalDefs[i].name);
            first = false;
        }
    }

    if (first) {
        q2a_strncpy(list, "(none)", sizeof(list)-1);
    }
    return list;
}

/**
 * Maps a hacktype_t (the reason hackDetected() is disconnecting a client)
 * to its corresponding signal bit, purely so it's visible on the client's
 * signal mask alongside the other detections.
 */
unsigned int signalForHacktype(hacktype_t h) {
    switch (h) {
        case HT_GENERAL_PROXY:  return SIGNAL_HACK_PROXY;
        case HT_GENERAL_AIMBOT: return SIGNAL_HACK_AIMBOT;
        case HT_ZBOT:            return SIGNAL_HACK_ZBOT;
        case HT_RATBOT:          return SIGNAL_HACK_RATBOT;
        case HT_CUSTOM_CLIENT:   return SIGNAL_HACK_CUSTOMCLIENT;
        case HT_MSEC:            return SIGNAL_HACK_MSEC;
        case HT_TIMESCALE:       return SIGNAL_HACK_TIMESCALE;
        case HT_ALIAS:           return SIGNAL_HACK_ALIAS;
        case HT_STUFF:           return SIGNAL_HACK_STUFF;
        case HT_USERINFO:        return SIGNAL_HACK_USERINFO;
        default:                 return SIGNAL_HACK_UNKNOWN;
    }
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
        addCmdQueue(
            client,
            QCMD_DISCONNECT,
            1,
            0,
            va("%s tripped the signal threshold (score %d): %s", proxyinfo[client].name, score, signalListString(client))
        );
    }
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
