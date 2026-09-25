/**
 * Q2Admin
 * Signal/score based cheat detection.
 *
 * Various detection heuristics ("signals") each mark a bit for a client when
 * they currently match, and contribute a weight to that client's score while
 * the bit stays set. When the summed score reaches signal_score_threshold the
 * client is removed, but the mask itself is kept (and viewable via `!signals`)
 * even for clients who never cross it, so an admin can see what's currently
 * suspicious about a player.
 *
 * Signals derived from hackDetected()'s hacktype_t are weighted at
 * SIGNAL_SCORE_KICK so they still remove a client immediately on their own,
 * matching their historical behavior; they're raised purely for visibility,
 * hackDetected() still does its own disconnectuser-gated kick.
 */

#pragma once

#define SIGNAL_AIMBOT_JITTER       BIT(0)  // suspicious angle snapping, below confirmed-aimbot threshold
#define SIGNAL_CHATFLOOD           BIT(1)  // sending chat messages faster than allowed
#define SIGNAL_PROXY_DETECTED      BIT(2)  // Related to reconnection 
#define SIGNAL_TIMESCALE_MODIFIED  BIT(4)  // Client's timescale is more than 1.0
#define SIGNAL_ALIAS_UNSUPPORTED   BIT(5)  // Client doesn't handle "alias" command properly
#define SIGNAL_WONKY_USERINFO      BIT(6)  // A standard key is missing from userinfo
#define SIGNAL_BAD_CLIENT          BIT(7)  // Client doesn't behave the way we know it should
#define SIGNAL_SNAP_FIRE           BIT(8)  // view snapped onto a player that wasn't near the crosshair, firing immediately
#define SIGNAL_AIM_TRACK           BIT(9)  // crosshair stayed implausibly tight on a moving target for a sustained streak
#define SIGNAL_SKIN_OVERFLOW       BIT(10) // Attempted to use an oversized skin
#define SIGNAL_VERSION_DEADLINE    BIT(11) // Client version probe unanswered
#define SIGNAL_ALIAS_DEADLINE      BIT(12) // Alias probe unanswered
#define SIGNAL_TIMESCALE_DEADLINE  BIT(13) // Timescale probe unanwered
#define SIGNAL_CHECKVAR_DEADLINE   BIT(14) // Checkvar probe unanswered
#define SIGNAL_MSEC_OVERRUN        BIT(15) // Client command used too much msec
#define SIGNAL_MSEC_UNDERRUN       BIT(16) // Client banking msec for later burst
#define SIGNAL_IMPULSE             BIT(17) // Player issued a bad impulse
#define SIGNAL_ZBOT_DETECTED       BIT(18) // Pretty sure player is using a zbot
#define SIGNAL_RATBOT_DETECTED     BIT(19) // Pretty sure player is using a ratbot
#define SIGNAL_BAN_ADJUSTMENT      BIT(20) // Ban entry has a "SCORE" property
#define SIGNAL_MANUAL              BIT(21) // Human admin manually added score
#define SIGNAL_VPN_SUSPICIOUS      BIT(22) // 25-50% sure on VPN usage
#define SIGNAL_VPN_LIKEY           BIT(23) // 50-75% sure on VPN usage
#define SIGNAL_VPN_DETECTED        BIT(24) // 75-100% sure on VPN usage

extern int signal_score_threshold; // total score needed to remove a client, 0 disables

void clearSignal(int client, unsigned int signal);
void evaluateSignalScore(int client);
void raiseSignal(int client, unsigned int signal);
void signaladdRun(int startarg, edict_t *ent, int client);
char *signalListString(int client);
int signalScore(int client);
void signalsRun(int startarg, edict_t *ent, int client);
void signalWeightInit(char *arg);
void signalWeightRun(int startarg, edict_t *ent, int client);
