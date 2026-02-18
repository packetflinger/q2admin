/**
 * Q2Admin
 * Spawn related stuff
 */

#pragma once

#define SPAWNFILE       "q2a_spawn.cfg"
#define SPAWNCMD        "[sv] !spawncmd [SW/EX/RE] \"command\"\n"
#define SPAWNDELCMD     "[sv] !spawndel spawnnum\n"
#define SPAWN_MAXCMDS   50

#define SPAWN_SW  0
#define SPAWN_EX  1
#define SPAWN_RE  2

typedef struct {
    char *spawncmd;
    byte type;
    bool onelevelflag;
    re_t r;
} spawncmd_t;

bool checkDisabledEntities(char *classname);
bool checkforspawncmd(char *cp, int spawncmd);
void displayNextSpawn(edict_t *ent, int client, long floodcmd);
void freeOneLevelSpawnLists(void);
void freeSpawnLists(void);
void linkentity_internal(edict_t *ent);
void listspawnsRun(int startarg, edict_t *ent, int client);
bool ReadSpawnFile(char *spawnname, bool onelevelflag);
void readSpawnLists(void);
void reloadSpawnFileRun(int startarg, edict_t *ent, int client);
void spawncmdRun(int startarg, edict_t *ent, int client);
void spawnDelRun(int startarg, edict_t *ent, int client);
void unlinkentity_internal(edict_t *ent);
