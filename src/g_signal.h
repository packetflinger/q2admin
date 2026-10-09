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
#define SIGNAL_TIMESCALE_MODIFIED  BIT(3)  // Client's timescale is more than 1.0
#define SIGNAL_ALIAS_UNSUPPORTED   BIT(4)  // Client doesn't handle "alias" command properly
#define SIGNAL_WONKY_USERINFO      BIT(5)  // A standard key is missing from userinfo
#define SIGNAL_BAD_CLIENT          BIT(6)  // Client doesn't behave the way we know it should
#define SIGNAL_SNAP_FIRE           BIT(7)  // view snapped onto a player that wasn't near the crosshair, firing immediately
#define SIGNAL_AIM_TRACK           BIT(8)  // crosshair stayed implausibly tight on a moving target for a sustained streak
#define SIGNAL_SKIN_OVERFLOW       BIT(9)  // Attempted to use an oversized skin
#define SIGNAL_VERSION_DEADLINE    BIT(10) // Client version probe unanswered
#define SIGNAL_ALIAS_DEADLINE      BIT(11) // Alias probe unanswered
#define SIGNAL_TIMESCALE_DEADLINE  BIT(12) // Timescale probe unanwered
#define SIGNAL_CHECKVAR_DEADLINE   BIT(13) // Checkvar probe unanswered
#define SIGNAL_MSEC_OVERRUN        BIT(14) // Client command used too much msec
#define SIGNAL_MSEC_UNDERRUN       BIT(15) // Client banking msec for later burst
#define SIGNAL_IMPULSE             BIT(16) // Player issued a bad impulse
#define SIGNAL_ZBOT_DETECTED       BIT(17) // Pretty sure player is using a zbot
#define SIGNAL_RATBOT_DETECTED     BIT(18) // Pretty sure player is using a ratbot
#define SIGNAL_BAN_ADJUSTMENT      BIT(19) // Ban entry has a "SCORE" property
#define SIGNAL_MANUAL              BIT(20) // Human admin manually added score
#define SIGNAL_VPN_SUSPICIOUS      BIT(21) // 25-50% sure on VPN usage
#define SIGNAL_VPN_LIKEY           BIT(22) // 50-75% sure on VPN usage
#define SIGNAL_VPN_DETECTED        BIT(23) // 75-100% sure on VPN usage
#define SIGNAL_PROTOCOL_DOWNGRADE  BIT(24) // connected with less protocol than their client supports
#define SIGNAL_MVD_IMPOSTER        BIT(25) // Player's userinfo claims to be q2pro's dummy MVD client

// Signals whose contribution can change while they're already raised: either
// a weight times a running count, or a per-player amount. Raising one of these
// again always re-scores; raising any other signal that's already set is a
// no-op (see raiseSignal()).
#define SIGNAL_SCORE_VARIES (SIGNAL_AIMBOT_JITTER | SIGNAL_SNAP_FIRE | SIGNAL_IMPULSE | \
                             SIGNAL_MANUAL | SIGNAL_BAN_ADJUSTMENT)

// One signal's definition: the bit it sets, what it contributes to a client's
// score while set, and the name it goes by in q2admin.cfg, !signals and
// !signal_weight. Weights are overridable at runtime and from the config file,
// so the table in g_signal.c is not const; the values there are the defaults.
typedef struct {
    unsigned int bit;
    int weight;
    const char *name;
} signal_def_t;

extern int signal_score_threshold; // total score needed to remove a client, 0 disables

void clearSignal(int client, unsigned int signal);
void evaluateSignalScore(int client);
signal_def_t *findSignalDef(const char *name);
signal_def_t *findSignalDefFromID(const int id);
void raiseSignal(int client, unsigned int signal);
void signaladdRun(int startarg, edict_t *ent, int client);
char *signalListString(int client);
int signalScore(int client);
void signalsRun(int startarg, edict_t *ent, int client);
void signalWeightInit(char *arg);
void signalWeightRun(int startarg, edict_t *ent, int client);
