/**
 * Q2Admin
 * Client checking stuff (proxies, zbots, etc)
 */

#pragma once

int checkForOverflows(edict_t *ent, int client);
void serverLogZBot(edict_t *ent, int client);
void ClientThink(edict_t *ent, usercmd_t *ucmd);
void G_RunFrame(void);
void Pmove_internal(pmove_t *pmove);
qboolean zbc_ZbotCheck(int client, usercmd_t *ucmd);
