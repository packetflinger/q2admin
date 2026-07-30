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
vpn_cache_t *cachehead = NULL;

/**
 * Console command to output the contents of the VPN cache.
 */
void cacheDumpRun(int startarg, edict_t *ent, int client) {
    addCmdQueue(client, QCMD_DISPCACHE, 0, 0, 0);
}

/**
 * Console command to clear the VPN cache
 */
void cacheDeleteRun(int startarg, edict_t *ent, int client) {
    deleteAddressCache();
    gi.cprintf(ent, PRINT_HIGH, "VPN cache cleared.\n");
}

/**
 * When displaying the contents of the VPN cache, show one address per server
 * frame to avoid lagging the server with a big list.
 */
void displayNextCacheEntry(edict_t *ent, int client, long cachenum) {
    long upto = cachenum;
    vpn_cache_t *findentry = cachehead;

    cachenum++;
    if (cachenum == 1) {
        gi.cprintf(ent, PRINT_HIGH, "Current VPN cache list:\n");
        gi.cprintf(ent, PRINT_HIGH, "  %-44s |  %15s | %-7s\n", "Network", "Expires", "Status");
    }
    while (findentry && upto) {
        findentry = findentry->next;
        upto--;
    }
    if (findentry) {
        gi.cprintf(ent, PRINT_HIGH, "  %-44s | %15.2fs | %-7s\n",
                net_addressToString(&findentry->address, false, false, true),
                findentry->expires - ltime,
                findentry->vpn_status.is_vpn ? "VPN" : ""
        );
        addCmdQueue(client, QCMD_DISPCACHE, 0, cachenum, 0);
    } else {
        gi.cprintf(ent, PRINT_HIGH, "End of VPN cache list.\n");
    }
}

/**
 * Locate the matching cache entry for the player's IP. If found, return a
 * pointer to the original lookup results, if not return NULL.
 *
 * If we find an entry that is expired, remove it and everything after. Since
 * entries are added chronologically when encountering an expired node, every
 * node following that one is older and will also be expired; no need to
 * actually check every one.
 */
vpn_t *findAddressInCache(netadr_t addr) {
    vpn_cache_t *previous = NULL, *to_delete = NULL, *e = cachehead;
    while (e != NULL) {
        if (ltime > e->expires) {
            deleteAddressCache(&e);
            return NULL;
        }
        if (net_contains(&e->address, &addr)) {
            gi.dprintf("DEBUG: found ip in VPN cache\n");
            return &e->vpn_status;
        }
        e = e->next;
    }
    return NULL;
}

/**
 * Add a VPN lookup result to the cache. We don't check for duplicates because
 * we're trying to be as fast as possible and in theory this will only be
 * called in the event the player's address isn't already found in the cache.
 */
void addToAddressCache(netadr_t addr, vpn_t results) {
    vpn_cache_t *c;
    c = G_Malloc(sizeof(vpn_cache_t));
    c->address = addr;
    c->vpn_status = results;
    c->expires = ltime + 60;  // 1 day
    c->next = cachehead;
    c->prev = NULL;
    if (cachehead != NULL) {
        cachehead->prev = c;
    }
    cachehead = c;
}

/**
 * Remove any cache entries that are expired
 */
void purgeExpiredCache(vpn_cache_t **start) {
    vpn_cache_t *c = *start;

    while (c != NULL && (ltime < c->expires)) {
        c = c->next;
    }
    deleteAddressCache(&c);
}

/**
 * Remove all cached entries from start and free their memory. Passing
 * cachehead as start will remove the entire cache. This will most likely be
 * used to remove expired sections of the list.
 */
void deleteAddressCache(vpn_cache_t **start) {
    vpn_cache_t *c = *start;
    vpn_cache_t *next = NULL;

    while (c != NULL) {
        next = c->next;
        G_Free(c);
        c = next;
    }
    *start = NULL;
}

void dumpCache() {
    vpn_cache_t *c = cachehead;
    gi.dprintf("Dumping VPN cache\n");
    while (c != NULL) {
        gi.dprintf("  %s\n", net_addressToString(&c->address, false, false, true));
        c = c->next;
    }
}

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
            v->is_vpn = Q_stricmp((char *)json_getPropertyValue(security, "vpn"), "true") == 0;
            v->is_proxy = Q_stricmp((char *)json_getPropertyValue(security, "proxy"), "true") == 0;
            v->is_tor = Q_stricmp((char *)json_getPropertyValue(security, "tor"), "true") == 0;
            v->is_relay = Q_stricmp((char *)json_getPropertyValue(security, "relay"), "true") == 0;
            if (v->is_vpn || v->is_proxy || v->is_tor || v->is_relay) {
                v->state = VPN_POSITIVE;
            }
        }
        net = json_getProperty(root, "network");
        if (net) {
            proxyinfo[i].network = net_parseIPAddressMask(json_getPropertyValue(net, "network"));
            q2a_strncpy(proxyinfo[i].auton_sys_num, json_getPropertyValue(net, "autonomous_system_number"), sizeof(proxyinfo[i].auton_sys_num));
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
        addToAddressCache(proxyinfo[i].network, proxyinfo[i].vpn);
    } else {
        gi.dprintf("No buffer returned from VPN lookup, code: %d\n", code);
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
