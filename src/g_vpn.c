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

#include "g_local.h"

static netadr_t vpn_ignorelist_ranges[VPN_IGNORELIST_MAXRANGES];
static int      vpn_ignorelist_count = 0;

/**
 * Cancels the VPN lookup still running for a player slot, if any. The
 * lookup keeps its request state inside the slot, so this must run before
 * the slot is cleared for someone else - otherwise the lookup finishes into
 * zeroed memory and crashes the server, or reports its result against
 * whoever has the slot by then.
 *
 * Called from ClientConnect() before it wipes the slot, and from
 * ClientDisconnect().
 */
void vpnCancelLookup(int client) {
    if (!VALIDCLIENT(client)) {
        return;
    }
    httpCancelDownloads(&proxyinfo[client].vpn_dl);
}

/**
 * Parses the space/comma separated CIDR ranges in vpn_ignorelist into
 * vpn_ignorelist_ranges, so vpnIsIgnorelisted() can compare against
 * pre-parsed netadr_t's instead of re-parsing the setting on every connect.
 * Called from SpawnEntities() so the list is rebuilt on every map change,
 * picking up any changes to the setting.
 */
void vpnBuildIgnorelist(void) {
    char list[sizeof(vpn_ignorelist)];
    char *saveptr, *token;

    vpn_ignorelist_count = 0;

    if (!vpn_ignorelist[0]) {
        return;
    }

    Q_strlcpy(list, vpn_ignorelist, sizeof(list));

    for (token = strtok_r(list, " ,", &saveptr);
         token && vpn_ignorelist_count < VPN_IGNORELIST_MAXRANGES;
         token = strtok_r(NULL, " ,", &saveptr)) {
        vpn_ignorelist_ranges[vpn_ignorelist_count++] = net_parseIPAddressMask(token);
    }

    if (q2a_developer) {
        Q_printf("compiled %d CIDR ranges in VPN ignorelist\n", vpn_ignorelist_count);
    }
}

/**
 * Whether a network address falls within one of the pre-parsed
 * vpn_ignorelist_ranges. Ignorelisted addresses skip the VPN check
 * entirely; these are for IPs we know aren't VPNs or don't care to waste
 * resources checking (wallfly and friends).
 */
static bool vpnIsIgnorelisted(netadr_t *addr) {
    for (int i = 0; i < vpn_ignorelist_count; i++) {
        if (net_contains(&vpn_ignorelist_ranges[i], addr)) {
            return true;
        }
    }
    return false;
}

/**
 * Acts on a finished VPN check, whether it just came back from IPLogs or
 * was found in the cache: raises the signal matching the verdict, enforces
 * ip_limit_vpn and logs the result.
 *
 * A VPN alone doesn't remove anyone: each verdict adds to the player's
 * signal score like any other suspicion, and only combined with other
 * signals (or a raised weight) crosses the threshold. "IP VPN" ban rules
 * still refuse VPN users outright.
 *
 * client: the player the result belongs to.
 * cached: true if the result came from the cache, only used for the log.
 *
 * Called from vpnLookupStatus() for a cached result and from
 * vpnFinishLookup() for a fresh one.
 */
static void vpnApplyResult(int client, bool cached) {
    vpn_t *v = &proxyinfo[client].vpn;

    if (Q_stricmp(v->verdict, "suspicious") == 0) {
        raiseSignal(client, SIGNAL_VPN_SUSPICIOUS);
    } else if (Q_stricmp(v->verdict, "vpn_likely") == 0) {
        raiseSignal(client, SIGNAL_VPN_LIKEY);
    } else if (Q_stricmp(v->verdict, "vpn_detected") == 0) {
        raiseSignal(client, SIGNAL_VPN_DETECTED);
    }

    // Count the other VPN players on the same network (ASN), this one
    // included, and remove this one if that's over the limit.
    if (ip_limit_vpn > 0 && v->state == VPN_POSITIVE && v->asn[0]) {
        int sameasn = 1;
        for (int j = 0; j < (int)maxclients->value; j++) {
            if (!proxyinfo[j].inuse || j == client || !isVPN(j)) {
                continue;
            }
            if (Q_stricmp(proxyinfo[j].vpn.asn, v->asn) == 0) {
                sameasn++;
            }
        }
        if (sameasn > ip_limit_vpn) {
            Q_snprintf(buffer, sizeof(buffer), "Too many connections from the same VPN provider\n");
            gi.cprintf(proxyinfo[client].ent, PRINT_HIGH, "%s", buffer);
            addCmdQueue(client, QCMD_DISCONNECT, 1, 0, buffer);
        }
    }

    Q_printf("%s[%s] vpn check%s: score=%.2f verdict=%s asn=%s%s\n",
        NAME(client), IP(client), (cached ? " (cached)" : ""), v->score, v->verdict, v->asn,
        (v->state == VPN_POSITIVE ? " (VPN)" : ""));
}

/**
 * Initiates a lookup for the VPN status of a player edict by POSTing their
 * IP address to IPLogs using CURL. This is a non-blocking call that will
 * finish on a later framerun. A result cached for the address within the
 * last vpn_cache_ttl seconds is used instead of a new lookup.
 */
void vpnLookupStatus(edict_t *ent) {
    download_t *dl;
    proxyinfo_t *pi;
    char *addr;

    int i = getEntOffset(ent) - 1;
    if (!vpn_enable) {
        return;
    }
    pi = &proxyinfo[i];
    addr = net_addressToString(&pi->address, false, false, false);

    if (vpnIsIgnorelisted(&pi->address)) {
        if (q2a_developer) {
            Q_printf("skipping VPN check for %s, ignorelisted\n", addr);
        }
        return;
    }

    // already checking or already checked
    if (pi->vpn.state >= VPN_CHECKING) {
        return;
    }

    // Network is cached, use that entry instead of doing a live lookup
    if (vpnCacheGet(&pi->address, &pi->vpn)) {
        vpnApplyResult(i, true);
        return;
    }

    dl = &pi->vpn_dl;
    dl->initiator = ent;
    dl->type = DL_VPN;
    dl->onFinish = vpnFinishLookup;
    dl->post = true;
    Q_strlcpy(dl->host, VPN_API_HOST, sizeof(dl->host));
    Q_strlcpy(dl->path, VPN_API_PATH, sizeof(dl->path));
    Q_snprintf(dl->body, sizeof(dl->body), "{\"ip\":\"%s\"}", addr);

    pi->vpn.state = VPN_CHECKING;

    Q_printf("checking %s for proxy/vpn\n", addr);

    // A refused request never calls vpnFinishLookup(), so undo the pending
    // state or the player is stuck as checking and never retried.
    if (!httpQueueDownload(dl)) {
        pi->vpn.state = VPN_UNKNOWN;
    }
}

/**
 * Callback when CURL finishes the IPLogs download. Parses the resulting
 * JSON, caches it and acts on it. A verdict of vpn_likely or vpn_detected
 * marks the player as on a VPN.
 */
void vpnFinishLookup(download_t *download, int code, byte *buff, int len) {
    vpn_t *v;
    json_t mem[128];
    const json_t *root, *prop, *ip_info;
    const char *verdict, *asn;
    int i = getEntOffset(download->initiator) - 1;

    v = &proxyinfo[i].vpn;
    if (!buff) {
        v->state = VPN_UNKNOWN;
        return;
    }

    root = json_create((char *)buff, mem, sizeof(mem)/sizeof(*mem));
    if (!root) {
        Q_printf("vpn check: json parsing error\n");
        v->state = VPN_UNKNOWN;
        return;
    }

    verdict = json_getPropertyValue(root, "verdict");
    if (verdict) {
        Q_strlcpy(v->verdict, verdict, sizeof(v->verdict));
    }

    prop = json_getProperty(root, "score");
    if (prop) {
        v->score = json_getReal(prop);
    }

    prop = json_getProperty(root, "confidence");
    if (prop) {
        v->confidence = json_getReal(prop);
    }

    ip_info = json_getProperty(root, "ip_info");
    if (ip_info) {
        asn = json_getPropertyValue(ip_info, "asn");
        if (asn) {
            Q_strlcpy(v->asn, asn, sizeof(v->asn));
        }
    }

    if (Q_stricmp(v->verdict, "vpn_likely") == 0 || Q_stricmp(v->verdict, "vpn_detected") == 0) {
        v->state = VPN_POSITIVE;
    } else {
        v->state = VPN_NEGATIVE;
    }
    vpnCacheSet(&proxyinfo[i].address, v);
    vpnApplyResult(i, false);
}

/**
 * Whether the client is coming from a VPN connection or not, meaning the
 * check's verdict was vpn_likely or vpn_detected.
 */
bool isVPN(int clientnum) {
    if (!VALIDCLIENT(clientnum)) {
        return false;
    }
    if (!vpn_enable) {
        return false;
    }
    return proxyinfo[clientnum].vpn.state == VPN_POSITIVE;
}

/**
 * Display any players currently connected via a VPN
 */
void vpnUsersRun(int startarg, edict_t *ent, int client) {
    if (!vpn_enable) {
        gi.cprintf(NULL, PRINT_HIGH, "VPN tracking is currently disabled\n");
        return;
    }
    for (int i = 0; i < (int)maxclients->value; i++) {
        if (!proxyinfo[i].inuse) {
            continue;
        }

        if (isVPN(i)) {
            gi.cprintf(NULL, PRINT_HIGH, "  %s [%s - %s - %s]\n", NAME(i), IP(i),
                proxyinfo[i].vpn.asn, proxyinfo[i].vpn.verdict);
        }
    }
}
