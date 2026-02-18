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
    vpn_state_t     state;
    bool        is_vpn;
    bool        is_proxy;
    bool        is_tor;
    bool        is_relay;
    char            network[50];
    char            asn[10];
} vpn_t;

extern bool vpn_kick;
extern bool vpn_enable;
extern char vpn_api_key[33];
extern char vpn_host[50];

void FinishVPNLookup(download_t *download, int code, byte *buff, int len);
bool isVPN(int clientnum);
void LookupVPNStatus(edict_t *ent);
void vpnUsersRun(int startarg, edict_t *ent, int client);
