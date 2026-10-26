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
