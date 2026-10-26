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

bool checkForNameChange(int client, edict_t *ent, char *userinfo);
bool checkForSkinChange(int client, edict_t *ent, char *userinfo);
extern unsigned int reconnectCapacity;

bool checkReconnectList(char *username);
int addRetryEntry(char *ip);
void expireReconnectEntries(void);
void releaseRetryEntry(int r);
void removeReconnectEntry(int i, bool keepRetry);
void reserveReconnectEntry(void);
void ClientBegin(edict_t *ent);
bool ClientConnect(edict_t *ent, char *userinfo);
void ClientDisconnect(edict_t *ent);
void ClientUserinfoChanged(edict_t *ent, char *userinfo);
char *COM_Parse(char **data_p, char **command_p);
bool entSwapAllowed(char *cn);
char *featuresToString(int f);
char *FindIpAddressInUserInfo(char *userinfo, bool *userInfoOverflow);
void InitGame(void);
void ReadGame(char *filename);
void ReadLevel(char *filename);
void SpawnEntities(char *mapname, char *entities, char *spawnpoint);
void SubstituteEntities(char *newents, char *oldents);
void SubstituteEntity(char *newents, cvar_t *sub, char *needle, char *token, bool *found);
bool UpdateInternalClientInfo(int client, edict_t *ent, char *userinfo, bool* userInfoOverflow);
void WriteGame(char *filename, bool autosave);
void WriteLevel(char *filename);
