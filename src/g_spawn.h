/**
 * Copyright (C) The Q2Admin contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <https://www.gnu.org/licenses/>.
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
bool checkForSpawnCmd(char *cp, int spawncmd);
void displayNextSpawn(edict_t *ent, int client, long floodcmd);
void freeOneLevelSpawnLists(void);
void freeSpawnLists(void);
void linkentity_internal(edict_t *ent);
void listspawnsRun(int startarg, edict_t *ent, int client);
bool readSpawnFile(char *spawnname, bool onelevelflag);
void readSpawnLists(void);
void reloadSpawnFileRun(int startarg, edict_t *ent, int client);
void spawncmdRun(int startarg, edict_t *ent, int client);
void spawnDelRun(int startarg, edict_t *ent, int client);
void unlinkentity_internal(edict_t *ent);
