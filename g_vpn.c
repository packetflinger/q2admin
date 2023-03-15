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

// figure out dns resolution later (vpnapi.io)
char vpn_host[50] = "vpnapi.io";

/**
 *
 */
void LookupVPNStatus(edict_t *ent)
{
    char *request;
    proxyinfo_t *pi;

    int i = getEntOffset(ent) - 1;
    if (!vpn_enable) {
        return;
    }
    pi = &proxyinfo[i];

    // already checking or already checked
    if (pi->vpn.state >= VPN_CHECKING) {
        return;
    }


    request = va("/api/%d.%d.%d.%d?key=%s", pi->ipaddressBinary[0], pi->ipaddressBinary[1], pi->ipaddressBinary[2], pi->ipaddressBinary[3], vpn_api_key);
    proxyinfo[i].vpn.state = VPN_CHECKING;
    proxyinfo[i].dl.initiator = ent;
    proxyinfo[i].dl.onFinish = FinishVPNLookup;
    Q_strncpy(pi->dl.path, request, sizeof(pi->dl.path)-1);

    HTTP_QueueDownload(&proxyinfo[i].dl);
}

/**
 * Callback when CURL finishes download. Parse resulting JSON
 */
void FinishVPNLookup(download_t *download, int code, byte *buff, int len)
{
    vpn_t *v;
    json_t mem[32];
    const json_t *root;
    const json_t *security;
    const json_t *net;
    const char *vpn, *proxy, *tor, *relay;
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
            v->is_vpn = Q_stricmp(json_getPropertyValue(security, "vpn"), "true") == 0;
            v->is_proxy = Q_stricmp(json_getPropertyValue(security, "proxy"), "true") == 0;
            v->is_tor = Q_stricmp(json_getPropertyValue(security, "tor"), "true") == 0;
            v->is_relay = Q_stricmp(json_getPropertyValue(security, "relay"), "true") == 0;

            if (v->is_vpn || v->is_proxy || v->is_tor || v->is_relay) {
                v->state = VPN_POSITIVE;
                net = json_getProperty(root, "network");
                if (net) {
                    q2a_strncpy(v->network, json_getPropertyValue(net, "network"), sizeof(v->network));
                    q2a_strncpy(v->asn, json_getPropertyValue(net, "autonomous_system_number"), sizeof(v->asn));
                }

            }
        }

        if (v->state == VPN_POSITIVE && vpn_kick) {
            Q_snprintf(buffer, sizeof(buffer), "VPN connections not allowed, please reconnect without it\n", maxMsgLevel + 1);
            gi.cprintf(download->initiator, PRINT_HIGH, buffer);
            addCmdQueue(i, QCMD_DISCONNECT, 1, 0, buffer);
            gi.cprintf(NULL, PRINT_HIGH, "%s disconnected for using a VPN [%s - %s]\n",
                    download->initiator->client->pers.netname, proxyinfo[i].ipaddress, v->asn
            );
        }
    }
}

/**
 * Returns yes or no if the client is coming from a VPN connection
 */
qboolean isVPN(int clientnum)
{
    return qfalse;
}

/**
 * Display any players currently connected via a VPN
 */
void vpnUsersRun(int startarg, edict_t *ent, int client)
{
    int i;

    for (i=0; i<(int)maxclients->value; i++) {
        if (!proxyinfo[i].inuse) {
            continue;
        }

        if (proxyinfo[i].vpn.state == VPN_POSITIVE) {
            gi.cprintf(NULL, PRINT_HIGH, "  %s [%s - %s]\n", proxyinfo[i].name, proxyinfo[i].vpn.network, proxyinfo[i].vpn.asn);
        }
    }
}
