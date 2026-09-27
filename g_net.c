/**
 * IP address helper functions.
 */
#include "g_local.h"

/**
 * Check whether 2 IPs are the same. Ports are deliberately ignored, use
 * NET_IsEqualAdr() when the port matters too.
 */
bool net_addressesMatch(netadr_t *a1, netadr_t *a2) {
    int len;
    if (a1->type != a2->type) {
        return false;
    }

    // An address that failed to parse is left zeroed at NA_UNSPECIFIED. Two of
    // those must not compare equal or every client with an unparseable address
    // collides with every other one, which ip_limit would then read as a flood
    // of connections from a single host.
    if (a1->type != NA_IP && a1->type != NA_IP6) {
        return false;
    }

    len = (a1->type == NA_IP6) ? IP6_LEN : IP4_LEN;
    return q2a_memcmp(a1->ip.u8, a2->ip.u8, len) == 0;
}

/**
 * Get a string representation of a netadr_t.
 *
 * wrapv6 arg controls whether IPv6 addresses are sandwiched in between brackets or not.
 * incport arg controls whether ":portnum" will be appended.
 * incmask arg controls whether "/xx" cidr mask will be appended.
 */
char *net_addressToString(netadr_t *address, bool wrapv6, bool incport, bool incmask) {
    char temp[INET6_ADDRSTRLEN];
    static char dest[INET6_ADDRSTRLEN];

    q2a_memset(temp, 0, sizeof(temp));
    q2a_memset(dest, 0, sizeof(dest));

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

    if (incmask) {
        q2a_strcat(dest, va("/%d", address->mask_bits));
    }

    if (incport) {
        q2a_strcat(dest, va(":%d", address->port));
    }
    return dest;
}

/**
 * Get a netadr_t representing the subnet mask based on
 * CIDR notation (number of bits in the mask).
 *
 * Calculating this for IPv6 was for serious...
 */
netadr_t net_cidrToMask(int cidr, netadrtype_t t) {
    int i;
    uint32_t mask = 0;
    netadr_t addr;
    int quarterbits;
    int sixteenth;
    int quarter;
    int qstart;

    q2a_memset(&addr, 0, sizeof(netadr_t));
    if (t != NA_IP6) {
        addr.type = NA_IP;
        for (i=1; i<=cidr; i++) {
            mask += 1 << (32-i);
        }
        addr.ip.u8[3] = mask & 0xff;
        addr.ip.u8[2] = (mask >> 8) & 0xff;
        addr.ip.u8[1] = (mask >> 16) & 0xff;
        addr.ip.u8[0] = (mask >> 24) & 0xff;
    } else {
        addr.type = NA_IP6;
        if (cidr == 0) {
            return addr;
        }
        if (cidr > 128) {
            cidr = 128;
        }

        sixteenth = Q_ceil((float)cidr/8);
        sixteenth--;
        quarter = Q_ceil((float)cidr/32);
        quarter--;
        quarterbits = cidr - (quarter * 32);
        qstart = quarter * 4;

        q2a_memset(&addr.ip.u8, 0xff, sixteenth);

        for (i=1; i<=quarterbits; i++) {
            mask += 1 << (32-i);
        }

        addr.ip.u8[qstart+3] = mask & 0xff;
        addr.ip.u8[qstart+2] = (mask >> 8) & 0xff;
        addr.ip.u8[qstart+1] = (mask >> 16) & 0xff;
        addr.ip.u8[qstart+0] = (mask >> 24) & 0xff;
    }
    return addr;
}

/**
 * Tests if a network address is in a particular subnet
 */
bool net_contains(netadr_t *network, netadr_t *host) {
    if (network->type != host->type) {
        return false;
    }

    // A network that failed to parse is zeroed at NA_UNSPECIFIED, which gives
    // an all-zero mask that would otherwise match every equally-unparseable
    // host. Malformed entries match nothing instead.
    if (network->type != NA_IP && network->type != NA_IP6) {
        return false;
    }

    netadr_t mask = net_cidrToMask(network->mask_bits, network->type);
    for (int i=0; i<IP6_LEN; i++) {
        if ((network->ip.u8[i] & mask.ip.u8[i]) != (host->ip.u8[i] & mask.ip.u8[i])) {
            return false;
        }
    }
    return true;
}

/**
 * Parse a string representation of the player's IP address
 * into a netadr_t struct. A subnet mask of /32 or /128 is
 * assumed since it's a specific address. This support both
 * IPv4 and IPv6 addresses.
 *
 * The string comes from the client's userinfo, which is only as trustworthy
 * as the engine's own userinfo sanitizing, so the length is checked before
 * copying and inet_pton() is checked before anything is stored.
 *
 * On failure the address is left zeroed at NA_UNSPECIFIED rather than
 * half-filled, so callers can tell a bad address from a good one with
 * HASIP() and it can never be mistaken for 0.0.0.0.
 *
 * Input format: "192.2.0.4:1234" or "[2001:db8::face]:23456"
 * A port is required; a bare address matches neither form and fails.
 *
 * Returns true if the address parsed.
 */
bool net_parseIP(netadr_t *address, const char *ip) {
    char *delim;
    int addrlen;                  // number of characters in IP string
    char addr[INET6_ADDRSTRLEN];  // temporarily hold just the IP part
    struct in6_addr addr6;        // use for both versions

    q2a_memset(addr, 0, sizeof(addr));
    q2a_memset(address, 0, sizeof(netadr_t));
    q2a_memset(&addr6, 0, sizeof(struct in6_addr));

    // Look for IPv6
    delim = strstr(ip, "]:");
    if (delim) {
        addrlen = (int) (delim - (ip + 1));
        if (addrlen < 1 || addrlen >= (int) sizeof(addr)) {
            return false;
        }
        q2a_memcpy(addr, ip + 1, addrlen);
        if (inet_pton(AF_INET6, addr, &addr6) != 1) {
            return false;
        }
        address->type = NA_IP6;
        address->port = (uint16_t) q2a_atoi(delim + 2);
        address->mask_bits = 128;
        q2a_memcpy(address->ip.u8, addr6.s6_addr, IP6_LEN);
        return true;
    }

    // assume it's an IPv4 address
    delim = strstr(ip, ":");
    if (delim) {
        addrlen = (int) (delim - ip);
        if (addrlen < 1 || addrlen >= (int) sizeof(addr)) {
            return false;
        }
        q2a_memcpy(addr, ip, addrlen);
        if (inet_pton(AF_INET, addr, &addr6) != 1) {
            return false;
        }
        address->type = NA_IP;
        address->port = (uint16_t) q2a_atoi(delim + 1);
        address->mask_bits = 32;
        q2a_memcpy(address->ip.u8, addr6.s6_addr, IP4_LEN);
        return true;
    }

    return false;
}

/**
 * Parse a string representation of an IP address into a netadr_t.
 * This support both IPv4 and IPv6 addresses. Address should not have
 * a port appended and IPv6 addresses shouldn't be surrounded by
 * square brackets.
 *
 * A mask of /32 or /128 is set so the result can be handed straight to
 * net_contains() as a single-host network. On failure the returned address
 * is zeroed at NA_UNSPECIFIED, which net_contains() never matches.
 *
 * Input format: "192.2.0.4" or "2001:db8::face"
 */
netadr_t net_parseIPAddressBase(const char *ip) {
    netadr_t address;
    struct in6_addr addr6; // use for both versions

    q2a_memset(&address, 0, sizeof(netadr_t));
    q2a_memset(&addr6, 0, sizeof(struct in6_addr));

    // Look for IPv6
    if (strstr(ip, ":")) {
        if (inet_pton(AF_INET6, ip, &addr6) != 1) {
            return address;
        }
        address.type = NA_IP6;
        address.mask_bits = 128;
        q2a_memcpy(address.ip.u8, addr6.s6_addr, IP6_LEN);
        return address;
    }

    // assume it's an IPv4 address
    if (strstr(ip, ".")) {
        if (inet_pton(AF_INET, ip, &addr6) != 1) {
            return address;
        }
        address.type = NA_IP;
        address.mask_bits = 32;
        q2a_memcpy(address.ip.u8, addr6.s6_addr, IP4_LEN);
        return address;
    }
    return address;
}

/**
 * Parse a basic string IP address with or without a CIDR mask.
 *
 * Callers feed this ban file entries, config tokens and the "network" field
 * from the IPLogs API response, so the length is checked before copying and
 * both the address and the mask width are validated before anything is
 * stored. An out-of-range mask is rejected rather than clamped: it means a
 * malformed entry, and silently widening or narrowing a ban would be worse
 * than ignoring it.
 *
 * On failure the returned address is zeroed at NA_UNSPECIFIED, which
 * net_contains() never matches, so a bad entry is simply inert.
 *
 * Examples:
 * 192.0.2.5
 * 192.0.2.5/27
 * 2002:db8::4
 * 2002:db8::4/64
 */
netadr_t net_parseIPAddressMask(const char *ip) {
    netadr_t address;
    char *delim;
    int addrlen;                  // number of characters in the address part
    int mask_bits;
    char addr[INET6_ADDRSTRLEN];  // temporarily hold just the IP part
    struct in6_addr addr6;        // use for both versions

    q2a_memset(addr, 0, sizeof(addr));
    q2a_memset(&address, 0, sizeof(netadr_t));
    q2a_memset(&addr6, 0, sizeof(struct in6_addr));

    // Look for IPv6
    if (strstr(ip, ":")) {
        delim = strstr(ip, "/");
        if (delim) {
            addrlen = (int) (delim - ip);
            mask_bits = q2a_atoi(delim + 1);
        } else {
            addrlen = (int) q2a_strlen(ip);
            mask_bits = 128;
        }
        if (addrlen < 1 || addrlen >= (int) sizeof(addr) || mask_bits < 0 || mask_bits > 128) {
            return address;
        }
        q2a_memcpy(addr, ip, addrlen);
        if (inet_pton(AF_INET6, addr, &addr6) != 1) {
            return address;
        }
        address.type = NA_IP6;
        address.mask_bits = (uint8_t) mask_bits;
        q2a_memcpy(address.ip.u8, addr6.s6_addr, IP6_LEN);
        return address;
    }

    // assume it's an IPv4 address
    if (strstr(ip, ".")) {
        delim = strstr(ip, "/");
        if (delim) {
            addrlen = (int) (delim - ip);
            mask_bits = q2a_atoi(delim + 1);
        } else {
            addrlen = (int) q2a_strlen(ip);
            mask_bits = 32;
        }
        if (addrlen < 1 || addrlen >= (int) sizeof(addr) || mask_bits < 0 || mask_bits > 32) {
            return address;
        }
        q2a_memcpy(addr, ip, addrlen);
        if (inet_pton(AF_INET, addr, &addr6) != 1) {
            return address;
        }
        address.type = NA_IP;
        address.mask_bits = (uint8_t) mask_bits;
        q2a_memcpy(address.ip.u8, addr6.s6_addr, IP4_LEN);
        return address;
    }
    return address;
}
