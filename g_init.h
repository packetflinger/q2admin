/**
 * Q2Admin
 * Initialization stuff
 */

qboolean checkForNameChange(int client, edict_t *ent, char *userinfo);
qboolean checkForSkinChange(int client, edict_t *ent, char *userinfo);
qboolean checkReconnectList(char *username);
qboolean checkReconnectUserInfoSame(char *userinfo1, char *userinfo2);
void ClientBegin(edict_t *ent);
qboolean ClientConnect(edict_t *ent, char *userinfo);
void ClientDisconnect(edict_t *ent);
void ClientUserinfoChanged(edict_t *ent, char *userinfo);
char *COM_Parse(char **data_p, char **command_p);
char *FindIpAddressInUserInfo(char *userinfo, qboolean *userInfoOverflow);
void InitGame(void);
void ReadGame(char *filename);
void ReadLevel(char *filename);
void SpawnEntities(char *mapname, char *entities, char *spawnpoint);
void SubstituteEntities(char *newents, char *oldents);
void SubstituteEntity(char *newents, cvar_t *sub, char *needle, char *token, qboolean *found);
qboolean UpdateInternalClientInfo(int client, edict_t *ent, char *userinfo, qboolean* userInfoOverflow);
void WriteGame(char *filename, qboolean autosave);
void WriteLevel(char *filename);
