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

#define LRCONFILE       "q2a_lrcon.cfg"
#define LRCONCMD        "[sv] !lrcon [SW/EX/RE] \"password\" \"command\"\n"
#define LRCONDELCMD     "[sv] !lrcondel lrconnum\n"
#define LRCON_MAXCMDS   1024

#define LRC_SW  0
#define LRC_EX  1
#define LRC_RE  2

typedef struct {
    char *lrconcmd;
    char *password;
    byte type;
    re_t r;
} lrconcmd_t;

void check_lrcon_password(void);
bool checklrcon(char *cp, int lrcon);
void displayNextLRCon(edict_t *ent, int client, long lrconnum);
void freeLRconLists(void);
void listlrconsRun(int startarg, edict_t *ent, int client);
void lrcon_reset_rcon_password(int, edict_t *, int);
void lrconDelRun(int startarg, edict_t *ent, int client);
void lrconRun(int startarg, edict_t *ent, int client);
bool ReadLRconFile(char *lrcname);
void readLRconLists(void);
void reloadlrconfileRun(int startarg, edict_t *ent, int client);
void run_lrcon(edict_t *ent, int client);
