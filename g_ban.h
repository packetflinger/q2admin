/**
 * Q2Admin
 * Ban related stuff
 */
#pragma once

#define BANCMD_LAYOUT       "[sv] !BAN [+/-(-)] [ALL/[NAME [LIKE/RE] name/%%p x/BLANK/ALL(ALL)] [IP VPN/ipv4addr/ipv6addr/%%p x][/yyy(32|128)]] [VERSION [LIKE/RE] xxx] [PASSWORD xxx] [MAX 0-xxx(0)] [FLOOD xxx(num) xxx(sec) xxx(silence] [MSG xxx] [TIME 1-xxx(mins)] [SAVE [MOD]] [NOCHECK]\n"
#define CHATBANCMD_LAYOUT   "[sv] !CHATBAN [LIKE/RE(LIKE)] xxx [MSG xxx] [SAVE [MOD]]\n"
#define CHATBANFILE_LAYOUT  "CHATBAN: [LIKE/RE(LIKE)] \"xxx\" [MSG  \"xxx\"]"

#define BANLISTFILE         "q2a_ban.cfg"
#define BANLISTREMOTEFILE   "http://q2.packetflinger.com/dl/q2admin/ban.cfg"
#define DEFAULTBANMSG       "You are banned from this server!"
#define DEFAULTCHABANMSG    "Message banned."

#define BANCHECK     (CCMD_BANNED | CCMD_RECONNECT)

#define LT_PERM     1
#define LT_TEMP     2

/**
 * Player name matching type
 */
#define NOTUSED     0
#define NICKALL     1
#define NICKEQ      2
#define NICKLIKE    3
#define NICKRE      4
#define NICKBLANK   5

typedef enum {
    VERSION_NONE,
    VERSION_ALL,
    VERSION_EQUALS,
    VERSION_REGEX,
    VERSION_LIKE,
} versiontype_t;

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
    re_t r;
    qboolean exclude;
    byte type;
    byte loadType;
    netadr_t addr;
    versiontype_t vtype;
    re_t  vr;
    char version[100];
    char nick[80];
    char password[80];
    char *msg;
    long maxnumberofconnects;
    long numberofconnects;
    long bannum;
    float timeout;
    qboolean vpn;
    struct chatflood_s floodinfo;
    struct banstruct *next;
} baninfo_t;

/**
 * Chat ban stuff
 */
typedef struct chatbanstruct {
    re_t r;
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

extern baninfo_t *banhead;
extern chatbaninfo_t *cbanhead;
extern qboolean ChatBanning_Enable;
extern qboolean IPBanning_Enable;
extern qboolean NickBanning_Enable;
extern qboolean VersionBanning_Enable;
extern char defaultBanMsg[256];
extern char defaultChatBanMsg[256];

char *ban_parseBan(unsigned char *cp);
char *ban_parseChatban(unsigned char *cp);
char *ban_parseInclude(unsigned char *in);
void banRun(int startarg, edict_t *ent, int client);
void chatbanRun(int startarg, edict_t *ent, int client);
int checkBanList(edict_t *ent, int client);
int checkCheckIfBanned(edict_t *ent, int client);
int checkCheckIfChatBanned(char *txt);
void delbanRun(int startarg, edict_t *ent, int client);
void delchatbanRun(int startarg, edict_t *ent, int client);
void displayNextBan(edict_t *ent, int client, long bannum);
void displayNextChatBan(edict_t *ent, int client, long chatbannum);
void freeBanLists(void);
void listbansRun(int startarg, edict_t *ent, int client);
void listchatbansRun(int startarg, edict_t *ent, int client);
qboolean parseBanFileContents(unsigned char *data);
qboolean ReadBanFile(char *bfname);
void readBanLists(void);
qboolean ReadRemoteBanFile(char *bfname);
void reloadbanfileRun(int startarg, edict_t *ent, int client);
