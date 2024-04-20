/**
 * Q2Admin
 * ref/admin related stuff
 */

#pragma once

#define LOGINFILE           "q2a_login.cfg"
#define BYPASSFILE          "q2a_bypass.cfg"
#define MAX_ADMINS          128
#define ADMIN_AUTH_LEVEL    1

#define ADMIN_LEVEL1    BIT(0)
#define ADMIN_LEVEL2    BIT(1)
#define ADMIN_LEVEL3    BIT(2)
#define ADMIN_LEVEL4    BIT(3)
#define ADMIN_LEVEL5    BIT(4)
#define ADMIN_LEVEL6    BIT(5)
#define ADMIN_LEVEL7    BIT(6)
#define ADMIN_LEVEL8    BIT(7)
#define ADMIN_LEVEL9    BIT(8)

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
void ADMIN_dumpmsec(edict_t *ent, int client);
void ADMIN_dumpuser(edict_t *ent, int client, int user, qboolean check);
void ADMIN_gfx(edict_t *ent);
void ADMIN_players(edict_t *ent, int client);
int ADMIN_process_command(edict_t *ent, int client);
int get_admin_level(char *givenpass, char *givenname);
int get_bypass_level(char *givenpass, char *givenname);
void List_Admin_Commands(edict_t *ent, int client);
void Read_Admin_cfg(void);
void reloadLoginFileRun(int startarg, edict_t *ent, int client);
