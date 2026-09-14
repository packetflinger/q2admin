/**
 * Q2Admin
 * Cache of IPLogs VPN-check results, keyed by client IP address.
 *
 * Repeat connections from the same address (rejoins, alt characters, short
 * disconnects) would otherwise re-query the IPLogs API every time. This
 * keeps the most recent result per address in a small fixed-size
 * open-addressing hash table, so a repeat connection is a handful of array
 * reads instead of a network round trip. Lookup/insert are both O(1)
 * (bounded by IPLOGS_CACHE_PROBE), no allocation, no locking needed since
 * everything runs on the single main server-frame thread.
 */
#pragma once

#define IPLOGS_CACHE_SIZE    1024   // slots; must be a power of 2
#define IPLOGS_CACHE_PROBE   8      // max linear-probe distance before an insert evicts

typedef struct {
    bool        used;
    netadr_t    addr;      // base address this entry is keyed on (port ignored)
    time_t      expires;   // absolute time this entry stops being valid
    iplogsvpn_t result;
} iplogs_cache_entry_t;

extern int iplogs_cache_ttl;   // seconds a cached IPLogs result stays valid

bool IPLogsCacheGet(netadr_t *addr, iplogsvpn_t *out);
void IPLogsCacheSet(netadr_t *addr, const iplogsvpn_t *result);
