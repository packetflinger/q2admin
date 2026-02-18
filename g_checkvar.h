/**
 * Q2Admin
 * Client variable checking stuff
 */
#pragma once

#define CHECKVARFILE    "q2a_cvar.cfg"
#define CHECKVARCMD     "[sv] !checkvarcmd [CT/RG] \"variable\" [\"value\" | \"lower\" \"upper\"]\n"
#define CHECKVARDELCMD  "[sv] !checkvardel checkvarnum\n"

#define CHECKVAR_MAX    50
#define CV_CONSTANT     0
#define CV_RANGE        1

/**
 * Variable properties
 */
typedef struct {
    char variablename[50];
    char value[50];
    double upper, lower;
    int type;
} checkvar_t;

void checkvarcmdRun(int startarg, edict_t *ent, int client);
void checkvarDelRun(int startarg, edict_t *ent, int client);
void checkVariableTest(edict_t *ent, int client, int idx);
void checkVariableValid(edict_t *ent, int client, char *value);
void displayNextCheckvar(edict_t *ent, int client, long checkvarcmd);
void listcheckvarsRun(int startarg, edict_t *ent, int client);
bool ReadCheckVarFile(char *checkvarname);
void readCheckVarLists(void);
void reloadCheckVarFileRun(int startarg, edict_t *ent, int client);
