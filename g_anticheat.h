/**
 * Q2Admin
 * Anticheat functions
 */

#pragma once

#define ANTICHEATEXCEPTIONREMOTEFILE    "http://q2.packetflinger.com/dl/q2admin/ac.cfg"
#define ANTICHEATEXCEPTIONLOCALFILE     "ac.cfg"
#define HASHLISTREMOTEDIR               "https://q2admin.net/server"

bool acGetRemoteFile(char *bfname);
void acLoadExceptions(void);
void acReloadExceptions(int startarg, edict_t *ent, int client);
void acUpdateList(void);
void getR1chHashList(char *hashname);
void acLoadHashList(void);
bool acReadRemoteHashListFile(char *bfname, char *blname);
void reloadhashlistRun(int startarg, edict_t *ent, int client);
