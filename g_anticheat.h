/**
 * Q2Admin
 * Anticheat functions
 */

#pragma once

#define ANTICHEATEXCEPTIONREMOTEFILE    "http://q2.packetflinger.com/dl/q2admin/ac.cfg"
#define ANTICHEATEXCEPTIONLOCALFILE     "ac.cfg"
#define HASHLISTREMOTEDIR               "https://q2admin.net/server"

qboolean AC_GetRemoteFile(char *bfname);
void AC_LoadExceptions(void);
void AC_ReloadExceptions(int startarg, edict_t *ent, int client);
void AC_UpdateList(void);
void getR1chHashList(char *hashname);
void loadhashlist(void);
qboolean ReadRemoteHashListFile(char *bfname, char *blname);
void reloadhashlistRun(int startarg, edict_t *ent, int client);
