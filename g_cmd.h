/**
 * Q2Admin
 */

#pragma once

// Where the command/setting allowed to be run/set from. For commands, they must
// be prefixed with "!" when executing, but not when defined in the q2aCommands
// structure.
#define CMDCTX_CFGFILE        BIT(0)  // any q2a*.cfg file
#define CMDCTX_CLIENTCONSOLE  BIT(1)  // cmd from admin-authed player in game
#define CMDCTX_SERVERCONSOLE  BIT(2)  // cmd typed into srv console (or rcon)

// Types of commands/values. All values are inputed as strings, these values
// determine how they're casted and stored internally.
//  "string1" -> "string1"
//  "Yes"     -> true
//  "4.5"     -> 4.5
#define CMDTYPE_COMMAND     0   // normal command
#define CMDTYPE_LOGICAL     1   // boolean value (yes/no/1/0) case insensitive
#define CMDTYPE_NUMBER      2   // integer or float value
#define CMDTYPE_STRING      3   // any string

#define MAX_BLOCK_MODELS    26

typedef void CMDRUNFUNC(int startarg, edict_t *ent, int client);
typedef void CMDINITFUNC(char *arg);

typedef struct {
    char *cmdname;
    byte cmdwhere;
    byte cmdtype;
    void *datapoint;
    CMDRUNFUNC *runfunc;    // called when cmd is issued from console
    CMDINITFUNC *initfunc;  // only for CFGFILE, called when file is read
} q2acmd_t;
extern q2acmd_t q2aCommands[];

typedef struct {
    char *model_name;
} block_model;
extern block_model block_models[MAX_BLOCK_MODELS];

void AddCommandString_internal(char *text);
void bprintf_internal(int printlevel, char *fmt, ...);
void cl_anglespeedkey_enableRun(int startarg, edict_t *ent, int client);
void cl_pitchspeed_enableRun(int startarg, edict_t *ent, int client);
void ClientCommand(edict_t *ent);
void clientsidetimeoutInit(char *arg);
void clientsidetimeoutRun(int startarg, edict_t *ent, int client);
void Cmd_Invite_f(edict_t *ent);
void Cmd_Teleport_f(edict_t *ent);
void cprintf_internal(edict_t *ent, int printlevel, char *fmt, ...);
void cvarsetRun(int startarg, edict_t *ent, int client);
bool doClientCommand(edict_t *ent, int client, bool *checkforfloodafter);
bool doServerCommand(void);
void dprintf_internal(char *fmt, ...);
void freezeRun(int startarg, edict_t *ent, int client);
char *getArgs(void);
edict_t *getClientFromArg(int client, edict_t *ent, int *cleintret, char *cp, char **text);
int getClientsFromArg(int client, edict_t *ent, char *cp, char **text);
void hackDetected(edict_t *ent, int client);
void impulsesToKickOnInit(char *arg);
void impulsesToKickOnRun(int startarg, edict_t *ent, int client);
void ipRun(int startarg, edict_t *ent, int client);
void kickRun(int startarg, edict_t *ent, int client);
void lockDownServerRun(int startarg, edict_t *ent, int client);
void maxfpsallowedInit(char *arg);
void maxfpsallowedRun(int startarg, edict_t *ent, int client);
void maxrateallowedRun(int startarg, edict_t *ent, int client);
void minfpsallowedInit(char *arg);
void minfpsallowedRun(int startarg, edict_t *ent, int client);
void minrateallowedRun(int startarg, edict_t *ent, int client);
void processCommand(int cmdidx, int startarg, edict_t *ent);
void proxyDetected(edict_t *ent, int client);
void ratbotDetected(edict_t *ent, int client);
bool readCfgFile(char *cfgfilename);
void readCfgFiles(void);
bool sayGroupCmd(edict_t *ent, int client, char *args);
void sayGroupRun(int startarg, edict_t *ent, int client);
bool sayPersonCmd(edict_t *ent, int client, char *args);
void sayPersonLowRun(int startarg, edict_t *ent, int client);
void sayPersonRun(int startarg, edict_t *ent, int client);
void ServerCommand(void);
void setadminRun(int startarg, edict_t *ent, int client);
void stuffClientRun(int startarg, edict_t *ent, int client);
void stuffNextLine(edict_t *ent, int client);
void timescaleDetected(edict_t *ent, int client);
void unfreezeRun(int startarg, edict_t *ent, int client);
void motdRun(int startarg, edict_t *ent, int client);
void versionRun(int startarg, edict_t *ent, int client);
