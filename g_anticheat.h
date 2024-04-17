/**
 * Q2Admin
 * Anticheat functions
 */

#pragma once

#define ANTICHEATEXCEPTIONREMOTEFILE    "http://q2.packetflinger.com/dl/q2admin/ac.cfg"
#define ANTICHEATEXCEPTIONLOCALFILE     "ac.cfg"

qboolean AC_GetRemoteFile(char *bfname);
void AC_LoadExceptions(void);
void AC_ReloadExceptions(int startarg, edict_t *ent, int client);
void AC_UpdateList(void);
