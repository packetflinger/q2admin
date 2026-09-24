/**
 * Q2Admin
 * Whois functions
 */

#pragma once

#define WHOISFILE   "whois.dat"
#define WHOISIPLEN  50
#define WHOISNAMELEN 16

typedef struct {
    char name[WHOISNAMELEN];
} user_dyn;

typedef struct {
    int id;
    char ip[WHOISIPLEN]; // allow for ipv6
    char seen[32];
    user_dyn dyn[10];
} user_details;

extern int WHOIS_COUNT;
extern int whois_active;
extern user_details *whois_details;

void whoisReloadFileRun(int startarg, edict_t *ent, int client);
void whoisGetID(int client, edict_t *ent);
void whois(int client, edict_t *ent);
void whoisAddUser(int client, edict_t *ent);
void whoisNewName(int client, edict_t *ent);
void whoisUpdateSeen(int client, edict_t *ent);
void whois_dumpdetails(int client, edict_t *ent, int userid);
void whois_write_file(void);
void whoisReadFile(void);
