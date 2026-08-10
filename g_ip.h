/**
 * IP Address context stuff
 */
#pragma once

typedef struct {
    bool found;
    int asnumber;
    char prefix[45];
    bool vpn;
    bool datacenter;
    float score;
} ipcontext_t;

ipcontext_t IP_Lookup(sqlite3 *db, netadr_t addr);
sqlite3 *IP_OpenDatabase(const char *dbfile);
