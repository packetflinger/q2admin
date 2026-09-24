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
} user_dyn_t;

typedef struct {
    int id;
    char ip[WHOISIPLEN]; // allow for ipv6
    char seen[32];
    user_dyn_t dyn[10];
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
