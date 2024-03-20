/**
 * IP address helper functions.
 */
#include "g_local.h"

/**
 * Parse a string representation of the player's IP address
 * into a netadr_t struct. This support both IPv4 and IPv6
 * addresses.
 *
 * This does not do input format checking, so be careful.
 *
 * Input format: "192.2.0.4:1234" or "[2001:db8::face]:23456"
 */
void ParseIP(netadr_t *address, const char *ip)
{
    char *delim;
    int addrlen;  // number of characters in IP string
    char addr[40]; // temporarily hold just the IP part
    struct in6_addr addr6;
    struct in_addr addr4;

    memset(addr, 0, 40);

    // Look for IPv6
    delim = strstr(ip, "]:");
    if (delim) {
        address->type = NA_IP6;
        address->port = (uint16_t) atoi(delim + 2);
        addrlen = (int) (delim - (ip + 1));
        q2a_memcpy(addr, ip + 1, addrlen);
        inet_pton(AF_INET6, addr, &addr6);
        q2a_memcpy(address->ip.u8, addr6.s6_addr, 16);
        return;
    }

    // assume it's an IPv4 address
    delim = strstr(ip, ":");
    if (delim) {
        address->type = NA_IP;
        address->port = (uint16_t) atoi(delim + 1);
        addrlen = (int) (delim - ip);
        q2a_memcpy(addr, ip + 1, addrlen);
        inet_pton(AF_INET, addr, &addr4);
        q2a_memcpy(address->ip.u8, addr6.s6_addr, 16);
        return;
    }
    return;
}
