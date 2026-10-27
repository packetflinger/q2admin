/**
 * Copyright (C) The Q2Admin contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

/**
 * VPN/proxy detection using the IPLogs API (https://iplogs.com/docs). It's a
 * public, unauthenticated POST endpoint, so there's no API key to configure.
 *
 * It's a best-effort service and responses can be slow, so a player may be
 * able to join and interact for a few seconds before the result arrives.
 */
#define VPN_API_HOST "iplogs.com"
#define VPN_API_PATH "/v1/check"
#define VPN_IGNORELIST_MAXRANGES 64     // max parsed entries from vpn_ignorelist

// states of a VPN check
typedef enum {
    VPN_UNKNOWN,    // unchecked, not known
    VPN_CHECKING,   // mid-lookup
    VPN_POSITIVE,   // verdict is vpn_likely or vpn_detected
    VPN_NEGATIVE,   // any other verdict
} vpn_state_t;

// Properties are only meaningful once state is VPN_POSITIVE or VPN_NEGATIVE
typedef struct {
    vpn_state_t state;
    char        verdict[16];    // "clean", "suspicious", "vpn_likely", "vpn_detected"
    double      score;          // 0.0-1.0 confidence value
    double      confidence;     // 0.0-1.0 certainty measure
    char        asn[16];        // just the number, e.g. "15169", from ip_info.asn
} vpn_t;

extern bool vpn_enable;
extern char vpn_ignorelist[1024];   // space/comma separated CIDR ranges exempt from the check

void vpnFinishLookup(download_t *download, int code, byte *buff, int len);
bool isVPN(int clientnum);
void vpnLookupStatus(edict_t *ent);
void vpnBuildIgnorelist(void);
void vpnCancelLookup(int client);
void vpnUsersRun(int startarg, edict_t *ent, int client);
