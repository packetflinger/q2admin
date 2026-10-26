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

#define CHECKVARFILE    "q2a_cvar.cfg"
#define CHECKVARCMD     "[sv] !checkvarcmd [CT/RG] \"variable\" [\"value\" | \"lower\" \"upper\"]\n"
#define CHECKVARDELCMD  "[sv] !checkvardel checkvarnum\n"

#define CHECKVAR_MAX    50
#define CV_CONSTANT     0
#define CV_RANGE        1

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
