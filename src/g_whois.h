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

#define WHOISFILE   "whois.dat"
#define WHOISIPLEN  50
#define WHOISNAMELEN 16
// Aliases remembered per record, oldest dropped first. Changing this also
// means reworking the fixed 13-field fscanf() in whoisReadFile(), which is
// the one place the slot count can't be driven from here.
#define WHOISNAMES  10

typedef struct {
    char name[WHOISNAMELEN];
} user_dyn_t;

typedef struct {
    int id;
    char ip[WHOISIPLEN]; // allow for ipv6
    char seen[32];
    user_dyn_t dyn[WHOISNAMES];
} user_details_t;

extern int WHOIS_COUNT;
extern int whois_active;
extern user_details_t *whois_details;

void whoisReloadFileRun(int startarg, edict_t *ent, int client);
void whoisGetID(int client, edict_t *ent);
void whois(int client, edict_t *ent);
void whoisAddUser(int client, edict_t *ent);
void whoisNewName(int client, edict_t *ent);
void whoisUpdateSeen(int client, edict_t *ent);
void whoisDumpDetails(int client, edict_t *ent, int userid);
void whoisWriteFile(void);
void whoisReadFile(void);
