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

// What we need to figure out if a player is using some kind of aim assist
typedef struct {
    short angles[2][2]; // pitch and yaw for current and last frame
    int toggle;         // for alternating angles between frames
    int jitter;         // jitter violation count. bot confirmed over threshold
    float jitter_time;  // ltime of first violation
    float jitter_last;  // ltime of most recent violation
} aimbot_t;

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
extern qboolean private_command_kick;
extern int msec_kick_on_bad;
extern int msec_max;
extern int speedbot_check_type;
extern int max_pmod_noreply;
extern int msec_int;

void serverLogZBot(edict_t *ent, int client);
void ClientThink(edict_t *ent, usercmd_t *ucmd);
void G_RunFrame(void);
void Pmove_internal(pmove_t *pmove);
qboolean AimbotCheck(int client, usercmd_t *ucmd);
