/**
 * Q2Admin
 * Whois functions
 */

#pragma once

#define WHOISFILE   "whois.dat"

typedef struct {
    char name[16];
} user_dyn;

typedef struct {
    int id;
    char ip[22];
    char seen[32];
    user_dyn dyn[10];
} user_details;

extern int WHOIS_COUNT;
extern int whois_active;
extern user_details *whois_details;

void reloadWhoisFileRun(int startarg, edict_t *ent, int client);
void whois_getid(int client, edict_t *ent);
void whois(int client, edict_t *ent);
void whois_adduser(int client, edict_t *ent);
void whois_newname(int client, edict_t *ent);
void whois_update_seen(int client, edict_t *ent);
void whois_dumpdetails(int client, edict_t *ent, int userid);
void whois_write_file(void);
void whois_read_file(void);
