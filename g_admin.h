/**
 * Q2Admin
 * ref/admin related stuff
 */

#pragma once

#define LOGINFILE           "q2a_login.cfg"
#define BYPASSFILE          "q2a_bypass.cfg"    // anticheat requirement bypass
#define MAX_ADMINS          128
#define ADMIN_AUTH_LEVEL    1

// Sets what admin commands are available for an admin-auth'd player. They can
// use these commands plus all commands from lower levels:
#define ADMIN_LEVEL1    BIT(0)  // !boot        - to kick other players
#define ADMIN_LEVEL2    BIT(1)  // !dumpmsec    - see what msec values are used
#define ADMIN_LEVEL3    BIT(2)  // !changemap   - obvious
#define ADMIN_LEVEL4    BIT(3)  // !dumpuser    - extended player data
#define ADMIN_LEVEL5    BIT(4)  // !auth and !gfx
#define ADMIN_LEVEL6    BIT(5)  // !dostuff     - stuff cmds to players
#define ADMIN_LEVEL7    BIT(6)  // ???
#define ADMIN_LEVEL8    BIT(7)  // !writewhois  - write players to local whois
#define ADMIN_LEVEL9    BIT(8)  // ???

typedef struct {
    char name[256];
    char password[256];
    char ip[256];
    int level;
} admin_type;

extern admin_type admin_pass[MAX_ADMINS];
extern admin_type q2a_bypass_pass[MAX_ADMINS];
extern int num_admins;
extern int num_q2a_admins;

void ADMIN_auth(edict_t *ent);
void ADMIN_boot(edict_t *ent, int client, int user);
void ADMIN_changemap(edict_t *ent, int client, char *mname);
void adm_dumpmsec(edict_t *ent, int client);
void adm_dumpuser(edict_t *ent, int client, int user, bool check);
void ADMIN_gfx(edict_t *ent);
void adm_players(edict_t *ent, int client);
int ADMIN_process_command(edict_t *ent, int client);
int get_admin_level(char *givenpass, char *givenname);
int get_bypass_level(char *givenpass, char *givenname);
void List_Admin_Commands(edict_t *ent, int client);
void Read_Admin_cfg(void);
void reloadLoginFileRun(int startarg, edict_t *ent, int client);
