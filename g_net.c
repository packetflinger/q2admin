/**
 * IP address helper functions.
 */
#include "g_local.h"

/**
 * Check whether 2 IPs are the same
 */
qboolean AddressesMatch(netadr_t *a1, netadr_t *a2)
{
    int len;
    if (a1->type != a2->type) {
        return qfalse;
    }
    len = (a1->type == NA_IP6) ? IP6_LEN : IP4_LEN;
    return q2a_memcmp(a1->ip.u8, a2->ip.u8, len);
}

/**
 * Get a string representation of a netadr_t.
 *
 * dest needs to be at least 46 bytes long.
 * wrapv6 arg controls whether IPv6 addresses are sandwiched in between brackets or not.
 * incport arg controls whether ":portnum" will be appended.
 */
void AddressToString(char *dest, netadr_t *address, qboolean wrapv6, qboolean incport)
{
    int i;
    char temp[INET6_ADDRSTRLEN];

    q2a_memset(temp, 0, sizeof(temp));
    if (address->type == NA_IP6) {
        inet_ntop(AF_INET6, &address->ip.u8, temp, INET6_ADDRSTRLEN);
        if (wrapv6) {
            q2a_strcpy(dest, va("[%s]", temp));
        } else {
            q2a_strcpy(dest, temp);
        }
    } else {
        inet_ntop(AF_INET, &address->ip.u8, temp, INET_ADDRSTRLEN);
        q2a_strcpy(dest, temp);
    }

    if (incport) {
        q2a_strcat(dest, ":");
        q2a_strcat(dest, va("%d", address->port));
    }
}

/**
 * Get a netadr_t representing the subnet mask based on
 * CIDR notation (number of bits in the mask).
 *
 * Calculating this for IPv6 was for serious...
 */
netadr_t net_cidrToMask(int cidr, qboolean v6)
{
    int i;
    uint32_t mask;
    netadr_t addr;
    int tempcidr;

    q2a_memset(&addr, 0, sizeof(netadr_t));
    if (!v6) {
        addr.type = NA_IP;
        for (i=1; i<=cidr; i++) {
            mask += 1 << (32-i);
        }
        addr.ip.u8[3] = mask & 0xff;
        addr.ip.u8[2] = (mask >> 8) & 0xff;
        addr.ip.u8[1] = (mask >> 16) & 0xff;
        addr.ip.u8[0] = (mask >> 24) & 0xff;
        printf("v4mask: %u\n", mask);
    } else {
        addr.type = NA_IP6;
        if (cidr == 128) {
            addr.ip.u32[0] = 0xffffffff;
            addr.ip.u32[1] = 0xffffffff;
            addr.ip.u32[2] = 0xffffffff;
            addr.ip.u32[3] = 0xffffffff;
        }
        if (cidr >= 96 && cidr < 128) {
            addr.ip.u32[0] = 0xffffffff;
            addr.ip.u32[1] = 0xffffffff;
            addr.ip.u32[2] = 0xffffffff;
            tempcidr = cidr - 96;
            mask = 0;
            for (i=1; i<=tempcidr; i++) {
                mask += 1 << (32-i);
            }
            addr.ip.u8[15] = mask & 0xff;
            addr.ip.u8[14] = (mask >> 8) & 0xff;
            addr.ip.u8[13] = (mask >> 16) & 0xff;
            addr.ip.u8[12] = (mask >> 24) & 0xff;
        }
        if (cidr >= 64 && cidr < 96) {
            addr.ip.u32[0] = 0xffffffff;
            addr.ip.u32[1] = 0xffffffff;
            tempcidr = cidr - 64;
            mask = 0;
            for (i=1; i<=tempcidr; i++) {
                mask += 1 << (32-i);
            }
            addr.ip.u8[11] = mask & 0xff;
            addr.ip.u8[10] = (mask >> 8) & 0xff;
            addr.ip.u8[9] = (mask >> 16) & 0xff;
            addr.ip.u8[8] = (mask >> 24) & 0xff;
        }
        if (cidr >= 32 && cidr < 64) {
            addr.ip.u32[0] = 0xffffffff;
            tempcidr = cidr - 32;
            mask = 0;
            for (i=1; i<=tempcidr; i++) {
                mask += 1 << (32-i);
            }
            addr.ip.u8[7] = mask & 0xff;
            addr.ip.u8[6] = (mask >> 8) & 0xff;
            addr.ip.u8[5] = (mask >> 16) & 0xff;
            addr.ip.u8[4] = (mask >> 24) & 0xff;
        }
        if (cidr < 32) {
            tempcidr = cidr - 0;
            mask = 0;
            for (i=1; i<=tempcidr; i++) {
                mask += 1 << (32-i);
            }
            addr.ip.u8[3] = mask & 0xff;
            addr.ip.u8[2] = (mask >> 8) & 0xff;
            addr.ip.u8[1] = (mask >> 16) & 0xff;
            addr.ip.u8[0] = (mask >> 24) & 0xff;
        }
    }
    return addr;
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
        address->port = (uint16_t) q2a_atoi(delim + 2);
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
        address->port = (uint16_t) q2a_atoi(delim + 1);
        addrlen = (int) (delim - ip);
        q2a_memcpy(addr, ip, addrlen);
        inet_pton(AF_INET, addr, &addr6);
        q2a_memcpy(address->ip.u8, addr6.s6_addr, sizeof(in_addr_t));
    }
}

/**
 * Parse a string representation of an IP address into a netadr_t.
 * This support both IPv4 and IPv6 addresses. Address should not have
 * a port appended and IPv6 addresses shouldn't be surrounded by
 * square brackets.
 *
 * This does not do input format checking, so be careful.
 *
 * Input format: "192.2.0.4" or "2001:db8::face"
 */
void ParseIPAddressBase(netadr_t *address, const char *ip)
{
    char *delim;
    int addrlen;           // number of characters in IP string
    char addr[40];         // temporarily hold just the IP part
    struct in6_addr addr6; // use for both versions

    q2a_memset(addr, 0, 40);
    q2a_memset(address, 0, sizeof(netadr_t));
    q2a_memset(&addr6, 0, sizeof(struct in6_addr));

    // Look for IPv6
    delim = strstr(ip, ":");
    if (delim) {
        address->type = NA_IP6;
        inet_pton(AF_INET6, ip, &addr6);
        q2a_memcpy(address->ip.u8, addr6.s6_addr, 16);
        return;
    }

    // assume it's an IPv4 address
    delim = strstr(ip, ".");
    if (delim) {
        address->type = NA_IP;
        inet_pton(AF_INET, ip, &addr6);
        q2a_memcpy(address->ip.u8, addr6.s6_addr, sizeof(in_addr_t));
    }
}

/**
 * Very simple check for whether a client's IP starts with a valid number
 */
qboolean ValidIPAddress(netadr_t *addr)
{
    int first = q2a_atoi(addr->ip.u8[0]);
    return first > 0;
}
