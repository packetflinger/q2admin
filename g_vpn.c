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
/**
 * Returns yes or no if the client is coming from a VPN connection
 */
qboolean isVPN(int clientnum)
{
    return qfalse;
}
