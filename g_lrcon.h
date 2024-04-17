/**
 * Q2Admin
 * Limited RCON stuff
 *
 * This functionality is now built-in to Q2Pro which will work out of band,
 * so it might be more useful to use it there.
 */

#pragma once

#define LRCONFILE       "q2a_lrcon.cfg"
#define LRCONCMD        "[sv] !lrcon [SW/EX/RE] \"password\" \"command\"\n"
#define LRCONDELCMD     "[sv] !lrcondel lrconnum\n"
#define LRCON_MAXCMDS   1024

#define LRC_SW  0
#define LRC_EX  1
#define LRC_RE  2

typedef struct {
    char *lrconcmd;
    char *password;
    byte type;
    regex_t *r;
} lrconcmd_t;

void check_lrcon_password(void);
qboolean checklrcon(char *cp, int lrcon);
void displayNextLRCon(edict_t *ent, int client, long lrconnum);
void freeLRconLists(void);
void listlrconsRun(int startarg, edict_t *ent, int client);
void lrcon_reset_rcon_password(int, edict_t *, int);
void lrconDelRun(int startarg, edict_t *ent, int client);
void lrconRun(int startarg, edict_t *ent, int client);
qboolean ReadLRconFile(char *lrcname);
void readLRconLists(void);
void reloadlrconfileRun(int startarg, edict_t *ent, int client);
void run_lrcon(edict_t *ent, int client);
