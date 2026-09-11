/**
 * Basic VPN detection. Sadly, as VPN services have become
 * mainstream, some abusive players have started using them
 * to hide their identities to enable abuse.
 *
 * The feature will query an API for the player's IP address
 * to find out if it's from a VPN provider. If so, q2admin
 * can be configured to kick this player or just identity them.
 *
 * Current provider is https://vpnapi.io. There is a free
 * account option that allows for up to 1000 queries per day.
 * You'll need to register on that site and add the API key
 * to your config.
 */

#include "g_local.h"

char vpn_host[50] = VPNAPIHOST;

/**
 * Initiates a lookup for the VPN status of a player edict using CURL. This is
 * a non-blocking call that will finish on a later framerun.
 */
void LookupVPNStatus(edict_t *ent) {
    char *request;
    proxyinfo_t *pi;
    char *addr;

    int i = getEntOffset(ent) - 1;
    if (!vpn_enable) {
        return;
    }
    pi = &proxyinfo[i];

    // already checking or already checked
    if (pi->vpn.state >= VPN_CHECKING) {
        return;
    }

    addr = net_addressToString(&pi->address, false, false, false);
    request = va("/api/%s?key=%s", addr, vpn_api_key);
    proxyinfo[i].vpn.state = VPN_CHECKING;
    proxyinfo[i].dl.initiator = ent;
    proxyinfo[i].dl.onFinish = FinishVPNLookup;
    Q_strncpy(pi->dl.path, request, sizeof(pi->dl.path)-1);

    HTTP_QueueDownload(&proxyinfo[i].dl);
}

/**
 * Callback when CURL finishes download. Parse resulting JSON
 */
void FinishVPNLookup(download_t *download, int code, byte *buff, int len) {
    vpn_t *v;
    json_t mem[32];
    const json_t *root, *security, *net;
    int i = getEntOffset(download->initiator) - 1;

    if (buff) {
        v = &proxyinfo[i].vpn;
        root = json_create(buff, mem, sizeof(mem)/sizeof(*mem));
        if (!root) {
            gi.dprintf("json parsing error\n");
            return;
        }

        security = json_getProperty(root, "security");
        if (security) {
            const char *vpn_val = json_getPropertyValue(security, "vpn");
            const char *proxy_val = json_getPropertyValue(security, "proxy");
            const char *tor_val = json_getPropertyValue(security, "tor");
            const char *relay_val = json_getPropertyValue(security, "relay");
            v->is_vpn = vpn_val && Q_stricmp((char *)vpn_val, "true") == 0;
            v->is_proxy = proxy_val && Q_stricmp((char *)proxy_val, "true") == 0;
            v->is_tor = tor_val && Q_stricmp((char *)tor_val, "true") == 0;
            v->is_relay = relay_val && Q_stricmp((char *)relay_val, "true") == 0;
            if (v->is_vpn || v->is_proxy || v->is_tor || v->is_relay) {
                v->state = VPN_POSITIVE;
            }
        }
        net = json_getProperty(root, "network");
        if (net) {
            const char *network_val = json_getPropertyValue(net, "network");
            const char *asn_val = json_getPropertyValue(net, "autonomous_system_number");
            if (network_val) {
                proxyinfo[i].network = net_parseIPAddressMask(network_val);
            }
            if (asn_val) {
                q2a_strncpy(proxyinfo[i].auton_sys_num, asn_val, sizeof(proxyinfo[i].auton_sys_num)-1);
            }
        }

        if (v->state == VPN_POSITIVE && vpn_kick) {
            Q_snprintf(buffer, sizeof(buffer), "VPN connections not allowed, please reconnect without it\n");
            gi.cprintf(download->initiator, PRINT_HIGH, buffer);
            addCmdQueue(i, QCMD_DISCONNECT, 1, 0, buffer);
        }
        if (ip_limit_vpn > 0 && proxyinfo[i].vpn.state == VPN_POSITIVE) {
            int sameasn = 1;
            for (int j = 0; j < (int)maxclients->value; j++) {
               if (!proxyinfo[j].inuse || i == j) {
                   continue;
               }
               if (Q_stricmp(proxyinfo[j].auton_sys_num, proxyinfo[i].auton_sys_num) == 0) {
                   sameasn++;
               }
           }
           if (sameasn > ip_limit_vpn) {
               Q_snprintf(buffer, sizeof(buffer), "Too many connections from the same VPN provider\n");
               gi.cprintf(proxyinfo[i].ent, PRINT_HIGH, buffer);
               addCmdQueue(i, QCMD_DISCONNECT, 1, 0, buffer);
           }
        }
        gi.cprintf(NULL, PRINT_HIGH, "%s %s %s%s\n", NAME(i), net_addressToString(&proxyinfo[i].network, false, false, true), proxyinfo[i].auton_sys_num, (isVPN(i) ? " (VPN)" : ""));
    }
}

/**
 * Whether the client is coming from a VPN connection or not.
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

        if (proxyinfo[i].vpn.state == VPN_POSITIVE) {
            gi.cprintf(NULL, PRINT_HIGH, "  %s [%s - %s]\n", proxyinfo[i].name, net_addressToString(&proxyinfo[i].network, false, false, true), proxyinfo[i].auton_sys_num);
        }
    }
}

/**
 * A second, independent VPN/proxy check against the IPLogs API
 * (https://iplogs.com/docs). This is a public endpoint that requires no
 * API key, so unlike LookupVPNStatus() above there's nothing to configure
 * beyond enabling it.
 */

static netadr_t iplogs_ignorelist_ranges[IPLOGS_IGNORELIST_MAXRANGES];
static int      iplogs_ignorelist_count = 0;

/**
 * Parses the space/comma separated CIDR ranges in iplogs_ignorelist into
 * iplogs_ignorelist_ranges, so IPLogsIsIgnorelisted() can compare against
 * pre-parsed netadr_t's instead of re-parsing the cvar on every connect.
 * Called from SpawnEntities() so the list is rebuilt on every map change,
 * picking up any changes to the cvar.
 */
void IPLogsBuildIgnorelist(void) {
    char list[sizeof(iplogs_ignorelist)];
    char *saveptr, *token;

    iplogs_ignorelist_count = 0;

    if (!iplogs_ignorelist[0]) {
        return;
    }

    Q_strncpy(list, iplogs_ignorelist, sizeof(list)-1);
    list[sizeof(list)-1] = '\0';

    for (token = strtok_r(list, " ,", &saveptr);
         token && iplogs_ignorelist_count < IPLOGS_IGNORELIST_MAXRANGES;
         token = strtok_r(NULL, " ,", &saveptr)) {
        iplogs_ignorelist_ranges[iplogs_ignorelist_count++] = net_parseIPAddressMask(token);
    }

    if (q2a_developer) {
        q2a_printf("compiled %d CIDR ranges in VPN ignorelist\n", iplogs_ignorelist_count);
    }
}

/**
 * Whether a network address falls within one of the pre-parsed
 * iplogs_ignorelist_ranges. Ignorelisted addresses skip the IPLogs check
 * entirely; these are for IPs we know aren't VPNs or don't care to waste
 * resources checking (wallfly and friends).
 */
static bool IPLogsIsIgnorelisted(netadr_t *addr) {
    for (int i = 0; i < iplogs_ignorelist_count; i++) {
        if (net_contains(&iplogs_ignorelist_ranges[i], addr)) {
            return true;
        }
    }
    return false;
}

/**
 * Initiates a lookup for the VPN status of a player edict by POSTing their
 * IP address to IPLogs using CURL. This is a non-blocking call that will
 * finish on a later framerun.
 */
void IPLogsCheckVPN(edict_t *ent) {
    download_t *dl;
    proxyinfo_t *pi;
    char *addr;

    int i = getEntOffset(ent) - 1;
    if (!iplogs_enable) {
        return;
    }
    pi = &proxyinfo[i];
    addr = net_addressToString(&pi->address, false, false, false);

    if (IPLogsIsIgnorelisted(&pi->address)) {
        if (q2a_developer) {
            q2a_printf("skipping VPN check for %s, ignorelisted\n", addr);
        }
        return;
    }

    // already checking or already checked
    if (pi->iplogs.state >= IPLOGS_CHECKING) {
        return;
    }

    dl = &pi->iplogs_dl;
    dl->initiator = ent;
    dl->type = DL_IPLOGS;
    dl->onFinish = IPLogsFinishCheck;
    dl->post = true;
    Q_strncpy(dl->host, IPLOGS_HOST, sizeof(dl->host)-1);
    Q_strncpy(dl->path, IPLOGS_PATH, sizeof(dl->path)-1);
    Q_snprintf(dl->body, sizeof(dl->body), "{\"ip\":\"%s\"}", addr);

    pi->iplogs.state = IPLOGS_CHECKING;

    q2a_printf("checking %s for proxy/vpn\n", addr);
    HTTP_QueueDownload(dl);
}

/**
 * Callback when CURL finishes the IPLogs download. Parse resulting JSON.
 */
void IPLogsFinishCheck(download_t *download, int code, byte *buff, int len) {
    iplogsvpn_t *v;
    json_t mem[128];
    const json_t *root, *prop;
    const char *verdict;
    int i = getEntOffset(download->initiator) - 1;

    if (!buff) {
        proxyinfo[i].iplogs.state = IPLOGS_UNKNOWN;
        return;
    }

    v = &proxyinfo[i].iplogs;
    root = json_create((char *)buff, mem, sizeof(mem)/sizeof(*mem));
    if (!root) {
        q2a_printf("iplogs: json parsing error\n");
        return;
    }

    prop = json_getProperty(root, "is_vpn");
    if (prop) {
        v->is_vpn = json_getBoolean(prop);
    }

    verdict = json_getPropertyValue(root, "verdict");
    if (verdict) {
        q2a_strncpy(v->verdict, verdict, sizeof(v->verdict)-1);
    }

    prop = json_getProperty(root, "score");
    if (prop) {
        v->score = json_getReal(prop);
    }

    prop = json_getProperty(root, "confidence");
    if (prop) {
        v->confidence = json_getReal(prop);
    }

    v->state = v->is_vpn ? IPLOGS_VPN : IPLOGS_CLEAN;

    q2a_printf("%s[%s] vpn check: score=%.2f verdict=%s%s\n", NAME(i), IP(i), v->score, v->verdict, (v->is_vpn ? " (VPN)" : ""));
}

/**
 * Whether the client is coming from a VPN connection or not, per IPLogs.
 */
bool isIPLogsVPN(int clientnum) {
    if (!VALIDCLIENT(clientnum)) {
        return false;
    }
    if (!iplogs_enable) {
        return false;
    }
    return proxyinfo[clientnum].iplogs.state == IPLOGS_VPN;
}
