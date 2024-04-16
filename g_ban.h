/**
 * Q2Admin
 * Ban related stuff
 */

#define BANCMD_LAYOUT       "[sv] !BAN [+/-(-)] [ALL/[NAME [LIKE/RE] name/%%p x/BLANK/ALL(ALL)] [IP ipv4addr/ipv6addr/%%p x][/yyy(32|128)]] [PASSWORD xxx] [MAX 0-xxx(0)] [FLOOD xxx(num) xxx(sec) xxx(silence] [MSG xxx] [TIME 1-xxx(mins)] [SAVE [MOD]] [NOCHECK]\n"
#define CHATBANCMD_LAYOUT   "[sv] !CHATBAN [LIKE/RE(LIKE)] xxx [MSG xxx] [SAVE [MOD]]\n"

#define BANLISTFILE         "q2a_ban.cfg"
#define BANLISTREMOTEFILE   "http://q2.packetflinger.com/dl/q2admin/ban.cfg"
#define DEFAULTCHABANMSG    "Message banned."

/**
 * Player name matching type
 */
#define NOTUSED     0
#define NICKALL     1
#define NICKEQ      2
#define NICKLIKE    3
#define NICKRE      4
#define NICKBLANK   5

/**
 * Flood tracking
 */
struct chatflood_s {
    qboolean chatFloodProtect;
    int chatFloodProtectNum;
    int chatFloodProtectSec;
    int chatFloodProtectSilence;
};

/**
 * Each ban is loaded into one of these structs.
 * They're combined into a linked list
 */
typedef struct banstruct {
    regex_t *r;
    qboolean exclude;
    byte type;
    byte loadType;
    netadr_t addr;
    char nick[80];
    char password[80];
    char *msg;
    long maxnumberofconnects;
    long numberofconnects;
    long bannum;
    float timeout;
    struct chatflood_s floodinfo;
    struct banstruct *next;
} baninfo_t;

/**
 * Chat ban stuff
 */
typedef struct chatbanstruct {
    regex_t *r;
    byte type;
    byte loadType;
    long bannum;
    char chat[256];
    char *msg;
    struct chatbanstruct *next;
} chatbaninfo_t;

#define CNOTUSED    0
#define CHATLIKE    1
#define CHATRE      2
