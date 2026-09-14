/**
 * Q2Admin
 * Signal/score based cheat detection.
 *
 * Various detection heuristics ("signals") each mark a bit for a client
 * when they currently match, and contribute a weight to that client's
 * score while the bit stays set. When the summed score reaches
 * signal_score_threshold the client is removed, but the mask itself is
 * kept (and viewable via `!signals`) even for clients who never cross it,
 * so an admin can see what's currently suspicious about a player.
 *
 * Signals derived from hackDetected()'s hacktype_t are weighted at
 * SIGNAL_SCORE_KICK so they still remove a client immediately on their
 * own, matching their historical behavior; they're raised purely for
 * visibility, hackDetected() still does its own disconnectuser-gated kick.
 */

#pragma once

#define SIGNAL_AIMBOT_JITTER       BIT(0)  // suspicious angle snapping, below confirmed-aimbot threshold
#define SIGNAL_VPN                 BIT(1)  // connecting through a known VPN/proxy
#define SIGNAL_CHATFLOOD           BIT(2)  // sending chat messages faster than allowed
#define SIGNAL_HACK_PROXY          BIT(3)  // HT_GENERAL_PROXY
#define SIGNAL_HACK_AIMBOT         BIT(4)  // HT_GENERAL_AIMBOT
#define SIGNAL_HACK_ZBOT           BIT(5)  // HT_ZBOT
#define SIGNAL_HACK_RATBOT         BIT(6)  // HT_RATBOT
#define SIGNAL_HACK_CUSTOMCLIENT   BIT(7)  // HT_CUSTOM_CLIENT
#define SIGNAL_HACK_MSEC           BIT(8)  // HT_MSEC
#define SIGNAL_HACK_TIMESCALE      BIT(9)  // HT_TIMESCALE
#define SIGNAL_HACK_ALIAS          BIT(10) // HT_ALIAS
#define SIGNAL_HACK_STUFF          BIT(11) // HT_STUFF
#define SIGNAL_HACK_USERINFO       BIT(12) // HT_USERINFO
#define SIGNAL_HACK_UNKNOWN        BIT(13) // HT_UNKNOWN
#define SIGNAL_SNAP_FIRE           BIT(14) // view snapped onto a player that wasn't near the crosshair, firing immediately
#define SIGNAL_AIM_TRACK           BIT(15) // crosshair stayed implausibly tight on a moving target for a sustained streak

extern int signal_score_threshold; // total score needed to remove a client, 0 disables

void raiseSignal(int client, unsigned int signal);
void clearSignal(int client, unsigned int signal);
int signalScore(int client);
char *signalListString(int client);
unsigned int signalForHacktype(hacktype_t h);
void evaluateSignalScore(int client);
void signalsRun(int startarg, edict_t *ent, int client);
