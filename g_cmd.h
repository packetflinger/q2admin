/**
 * Q2Admin
 */

#pragma once

// where the command can't be run?
#define CMDWHERE_CFGFILE        BIT(0)
#define CMDWHERE_CLIENTCONSOLE  BIT(1)
#define CMDWHERE_SERVERCONSOLE  BIT(2)

// type of command
#define CMDTYPE_NONE        0
#define CMDTYPE_LOGICAL     1   // boolean
#define CMDTYPE_NUMBER      2
#define CMDTYPE_STRING      3

typedef void CMDRUNFUNC(int startarg, edict_t *ent, int client);
typedef void CMDINITFUNC(char *arg);

typedef struct {
    char *cmdname;
    byte cmdwhere;
    byte cmdtype;
    void *datapoint;
    CMDRUNFUNC *runfunc;
    CMDINITFUNC *initfunc;
} q2acmd_t;

extern q2acmd_t q2aCommands[];

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
qboolean doClientCommand(edict_t *ent, int client, qboolean *checkforfloodafter);
qboolean doServerCommand(void);
void dprintf_internal(char *fmt, ...);
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
qboolean readCfgFile(char *cfgfilename);
void readCfgFiles(void);
qboolean sayGroupCmd(edict_t *ent, int client, char *args);
void sayGroupRun(int startarg, edict_t *ent, int client);
qboolean sayPersonCmd(edict_t *ent, int client, char *args);
void sayPersonLowRun(int startarg, edict_t *ent, int client);
void sayPersonRun(int startarg, edict_t *ent, int client);
void ServerCommand(void);
void setadminRun(int startarg, edict_t *ent, int client);
void stuffClientRun(int startarg, edict_t *ent, int client);
void stuffNextLine(edict_t *ent, int client);
void timescaleDetected(edict_t *ent, int client);
void zbotmotdRun(int startarg, edict_t *ent, int client);
void zbotversionRun(int startarg, edict_t *ent, int client);
