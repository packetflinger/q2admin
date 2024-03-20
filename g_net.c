/**
 * IP address helper functions.
 */
#include "g_local.h"

/**
 * Get a string representation of a netadr_t.
 *
 * dest needs to be at least 46 bytes long
 */
void IPString(char *dest, netadr_t *address, qboolean incport)
{
    int i;
    char temp[INET6_ADDRSTRLEN];

    q2a_memset(temp, 0, sizeof(temp));
    if (address->type == NA_IP6) {
        inet_ntop(AF_INET6, &address->ip.u8, temp, INET6_ADDRSTRLEN);
        q2a_strcpy(dest, va("[%s]", temp));
    } else {
        inet_ntop(AF_INET, &address->ip.u8, temp, INET_ADDRSTRLEN);
        q2a_strcpy(dest, temp);
    }

    if (incport) {
        q2a_strcat(dest, ":");
        q2a_strcat(dest, address->port);
    }
}

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
    int addrlen;           // number of characters in IP string
    char addr[40];         // temporarily hold just the IP part
    struct in6_addr addr6; // use for both versions

    q2a_memset(addr, 0, 40);
    q2a_memset(address, 0, sizeof(netadr_t));
    q2a_memset(&addr6, 0, sizeof(struct in6_addr));

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
        q2a_memcpy(addr, ip, addrlen);
        inet_pton(AF_INET, addr, &addr6);
        q2a_memcpy(address->ip.u8, addr6.s6_addr, sizeof(in_addr_t));
    }
}
