/**
 * Q2Admin
 * Initialization stuff
 */

bool checkForNameChange(int client, edict_t *ent, char *userinfo);
bool checkForSkinChange(int client, edict_t *ent, char *userinfo);
bool checkReconnectList(char *username);
bool checkReconnectUserInfoSame(char *userinfo1, char *userinfo2);
void ClientBegin(edict_t *ent);
bool ClientConnect(edict_t *ent, char *userinfo);
void ClientDisconnect(edict_t *ent);
void ClientUserinfoChanged(edict_t *ent, char *userinfo);
char *COM_Parse(char **data_p, char **command_p);
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
