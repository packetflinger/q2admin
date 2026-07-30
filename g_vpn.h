/**
 * Q2Admin
 * VPN detection
 */

#pragma once

#define VPNAPIHOST  "vpnapi.io"

// states of VPN check
typedef enum {
    VPN_UNKNOWN,    // unchecked, not known
    VPN_CHECKING,   // mid-lookup
    VPN_POSITIVE,   // confirmed, vpn address
    VPN_NEGATIVE,   // confirmed, non-vpn address
} vpn_state_t;

// Properties will be non-null if state == VPN_POSITIVE
typedef struct {
    vpn_state_t state;
    bool        is_vpn;
    bool        is_proxy;
    bool        is_tor;
    bool        is_relay;
} vpn_t;

// We don't want to query the VPN API every time someone joins the server. Keep
// a cache of addresses with answers and check every new player against it.
// Each entry should be cached for ~1 day.
typedef struct vpn_cache {
    netadr_t address;           // the parent cidr range
    vpn_t vpn_status;           // is it a vpn?
    float expires;              // level time + ??
    struct vpn_cache *next;
    struct vpn_cache *prev;
} vpn_cache_t;

extern bool vpn_kick;
extern bool vpn_enable;
extern char vpn_api_key[33];
extern char vpn_host[50];

void dumpCache();
void addToAddressCache(netadr_t addr, vpn_t results);
vpn_t *findAddressInCache(netadr_t addr);
void purgeExpiredCache();
void deleteAddressCache();
void FinishVPNLookup(download_t *download, int code, byte *buff, int len);
bool isVPN(int clientnum);
void LookupVPNStatus(edict_t *ent);
void vpnUsersRun(int startarg, edict_t *ent, int client);
void cacheDumpRun(int startarg, edict_t *ent, int client);
void displayNextCacheEntry(edict_t *ent, int client, long cachenum);
void cacheDeleteRun(int startarg, edict_t *ent, int client);
