/**
 * Q2Admin
 * Text flood controls
 */

#define FLOODFILE       "q2a_flood.cfg"
#define FLOODCMD        "[sv] !floodcmd [SW/EX/RE] \"command\"\n"
#define FLOODDELCMD     "[sv] !flooddel floodnum\n"
#define FLOOD_MAXCMDS   1024

#define DEFAULTFLOODMSG     "%s changed names too many times."
#define DEFAULTCHATFLOODMSG "%s is making too much noise."
#define DEFAULTSKINFLOODMSG "%s changed skin too many times."

#define FLOOD_SW  0
#define FLOOD_EX  1
#define FLOOD_RE  2

typedef struct {
    char *floodcmd;
    byte type;
    regex_t *r;
} floodcmd_t;

void chatFloodProtectInit(char *arg);
void chatFloodProtectRun(int startarg, edict_t *ent, int client);
qboolean checkForFlood(int client);
qboolean checkforfloodcmd(char *cp, int floodcmd);
qboolean checkforfloodcmds(char *cp);
qboolean checkForMute(int client, edict_t *ent, qboolean displayMsg);
void clientchatfloodprotectRun(int startarg, edict_t *ent, int client);
void displayNextFlood(edict_t *ent, int client, long floodcmd);
void floodcmdRun(int startarg, edict_t *ent, int client);
void floodDelRun(int startarg, edict_t *ent, int client);
void freeFloodLists(void);
void listfloodsRun(int startarg, edict_t *ent, int client);
void muteRun(int startarg, edict_t *ent, int client);
void nameChangeFloodProtectInit(char *arg);
void nameChangeFloodProtectRun(int startarg, edict_t *ent, int client);
qboolean ReadFloodFile(char *floodname);
void readFloodLists(void);
void reloadFloodFileRun(int startarg, edict_t *ent, int client);
void skinChangeFloodProtectInit(char *arg);
void skinChangeFloodProtectRun(int startarg, edict_t *ent, int client);
void stifleRun(int startarg, edict_t *ent, int client);
void unstifleRun(int startarg, edict_t *ent, int client);
