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

extern bool vpn_kick;
extern bool vpn_enable;
extern char vpn_api_key[33];
extern char vpn_host[50];

void FinishVPNLookup(download_t *download, int code, byte *buff, int len);
bool isVPN(int clientnum);
void LookupVPNStatus(edict_t *ent);
void vpnUsersRun(int startarg, edict_t *ent, int client);

/**
 * Independent, second VPN check against the IPLogs API (https://iplogs.com/docs).
 * Unlike the vpnapi.io check above, this is a public, unauthenticated POST
 * endpoint, so there's no API key to configure.
 */
#define IPLOGS_HOST "iplogs.com"
#define IPLOGS_PATH "/v1/check"

// states of an IPLogs check
typedef enum {
    IPLOGS_UNKNOWN,     // unchecked, not known
    IPLOGS_CHECKING,    // mid-lookup
    IPLOGS_VPN,         // confirmed, vpn/proxy address
    IPLOGS_CLEAN,       // confirmed, clean address
} iplogs_state_t;

// Properties are only meaningful once state is IPLOGS_VPN or IPLOGS_CLEAN
typedef struct {
    iplogs_state_t  state;
    bool            is_vpn;
    char            verdict[16];    // "clean", "suspicious", "vpn_likely", "vpn_detected"
    double          score;          // 0.0-1.0 confidence value
    double          confidence;     // 0.0-1.0 certainty measure
} iplogsvpn_t;

extern bool iplogs_enable;

void IPLogsCheckVPN(edict_t *ent);
void IPLogsFinishCheck(download_t *download, int code, byte *buff, int len);
bool isIPLogsVPN(int clientnum);
