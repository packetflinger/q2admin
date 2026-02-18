/**
 * Q2Admin
 * Command disabling functionality
 */

#pragma once

#define DISABLEFILE         "q2a_disable.cfg"
#define DISABLECMD          "[sv] !disablecmd [SW/EX/RE] \"command\"\n"
#define DISABLEDELCMD       "[sv] !disabledel disablenum\n"

#define DISABLE_MAXCMDS     50
#define DISABLE_SW          0
#define DISABLE_EX          1
#define DISABLE_RE          2

typedef struct {
    char *disablecmd;
    byte type;
    re_t r;
} disablecmd_t;

bool checkDisabledCommand(char *cmd);
bool checkfordisablecmd(char *cp, int disablecmd);
void disablecmdRun(int startarg, edict_t *ent, int client);
void disableDelRun(int startarg, edict_t *ent, int client);
void displayNextDisable(edict_t *ent, int client, long floodcmd);
void freeDisableLists(void);
void listdisablesRun(int startarg, edict_t *ent, int client);
bool ReadDisableFile(char *disablename);
void readDisableLists(void);
void reloadDisableFileRun(int startarg, edict_t *ent, int client);
