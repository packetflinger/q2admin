/**
 * Q2Admin
 * Client checking stuff (proxies, zbots, etc)
 */

#pragma once

#define MAXIMPULSESTOTEST   256

#define DEFAULTRECONNECTMSG     "Please wait to be reconnected to the server - this is normal for this level of bot protection.\nThe fastest way to do this is not to change any client info e.g. your name or skin."
#define DEFAULTUSERDISPLAY      "%s is using a client side proxy."
#define DEFAULTTSDISPLAY        "%s is using a speed cheat."
#define DEFAULTHACKDISPLAY      "%s is using a modified client."
#define DEFAULTSKINCRASHMSG     "%s tried to crash the server."
#define DEFAULTCL_PITCHSPEED_KICKMSG    "cl_pitchspeed changes not allowed on this server."
#define DEFAULTCL_ANGLESPEEDKEY_KICKMSG "cl_anglespeedkey changes not allowed on this server."

#define ZBOT_TESTSTRING1            "q2startxx\n"
#define ZBOT_TESTSTRING_TEST1       "q2startxx"
#define ZBOT_TESTSTRING_TEST2       "q2exx"
#define ZBOT_TESTSTRING_TEST3       ".please.disconnect.all.bots"
#define ZBOT_TESTSTRING_TEST1_OLD   "q2start"
#define ZBOT_TESTSTRING_TEST2_OLD   "q2e"
#define RATBOT_CHANGENAMETEST       "pwsnskle"
#define BOTDETECT_CHAR1             'F'
#define BOTDETECT_CHAR2             'U'

// In a perfect world the msec available vs used would be 1:1, but small delays
// are introduced here and there. This value is a multiplier for an acceptable
// amount of msec usage over. Q2Pro servers will allocate each client 1800ms of
// time for their moves over a period of 16 server frames (at the default HZ).
#define MSEC_SLOP                   1.125

// What we need to figure out if a player is using some kind of aim assist
typedef struct {
    short angles[2][2]; // pitch and yaw for current and last frame
    int toggle;         // for alternating angles between frames
    int jitter;         // jitter violation count. bot confirmed over threshold
    float jitter_time;  // ltime of first violation
    float jitter_last;  // ltime of most recent violation
} aimbot_t;

// Needed for tracking player freezing. When a player is frozen their msec
// value is set to 0.
typedef struct {
    bool frozen;
    float started;    // ltime
    float thaw;       // future ltime to automatically unfreeze
} freeze_t;

// What actions can be taken if msec violations are detected?
typedef enum {
    MVA_LEGACY,             // backwards compat, kick if over max_violations
    MVA_NOTHING,            // Don't do anything
    MVA_KICK,               // Remove the offender
} msec_action_t;

// Limits and such for ensuring msec values sent from clients are not abused.
// Instead of checking each second (1000ms) for a violation, extended over a
// larger period of time (a few seconds)
typedef struct {
    int max_allowed;        // Total consumption allowed per timespan
    int min_required;       // To prevent banking time to use in a burst later
    int timespan;           // Seconds
    int max_violations;     // Times max_used excseeded before taking action
    msec_action_t action;   // What do we do when violation happen?
} msec_limits_t;

// A collection of msec-related properties associated with each player.
typedef struct {
    int total;              // The msec sum from frames over a period of time
    int previous;           // The value in the previous usercmd_t
    int violations;         // How many times player exceeded global msec limits
    float end_frame;        // lframenum marking the end of the timespan
} player_msec_t;

// All the userinfo-related variables we need to track
typedef struct {
    int changed_count;
    int changed_start;
    float cl_anglespeedkey;
    int cl_pitchspeed;
    char skin[40];
    int rate;
    int maxfps;
    int msg;
    int namechangecount;
    long namechangetimeout;
    char raw[MAX_INFO_STRING + 45]; // the raw userinfo string
    int skinchangecount;
    long skinchangetimeout;
} userinfo_t;

extern char zbot_teststring1[];
extern char zbot_teststring_test1[];
extern char zbot_teststring_test2[];
extern char zbot_teststring_test3[];
extern char zbot_testchar1;
extern char zbot_testchar2;
extern char testchars[];
extern int maxrateallowed;
extern int minrateallowed;
extern int maxfpsallowed;
extern int minfpsallowed;
extern int zbc_jittermax;
extern int zbc_jittertime;
extern int zbc_jittermove;
extern char client_msg[256];
extern bool private_command_kick;
extern int speedbot_check_type;
extern int max_pmod_noreply;
extern msec_limits_t msec;

void serverLogZBot(edict_t *ent, int client);
void ClientThink(edict_t *ent, usercmd_t *ucmd);
void G_RunFrame(void);
void Pmove_internal(pmove_t *pmove);
bool AimbotCheck(int client, usercmd_t *ucmd);
void checkClientDeadlines(int c);
