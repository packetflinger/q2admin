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
bool checkForDisableCmd(char *cp, int disablecmd);
void disablecmdRun(int startarg, edict_t *ent, int client);
void disableDelRun(int startarg, edict_t *ent, int client);
void displayNextDisable(edict_t *ent, int client, long floodcmd);
void freeDisableLists(void);
void listdisablesRun(int startarg, edict_t *ent, int client);
bool readDisableFile(char *disablename);
void readDisableLists(void);
void reloadDisableFileRun(int startarg, edict_t *ent, int client);
