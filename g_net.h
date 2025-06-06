#ifndef G_NET_H
#define G_NET_H

#define IP4_LEN  4
#define IP6_LEN 16

#define IP(x)     (net_addressToString(&proxyinfo[x].address, qfalse, qfalse, qfalse))
#define IPMASK(x) (net_addressToString(&proxyinfo[x].address, qfalse, qfalse, qtrue))
#define IPSTR(a)  (net_addressToString(a->address, qfalse, qfalse))
#define IPSTRMASK(a) (net_addressToString(a, qfalse, qfalse, qtrue))
#define HASIP(x)  (proxyinfo[x].address.ip.u8[0] != 0)

typedef enum {
    NA_UNSPECIFIED,
    NA_LOOPBACK,
    NA_BROADCAST,
    NA_IP,
    NA_IP6
} netadrtype_t;

typedef union {
    uint8_t u8[16];
    uint16_t u16[8];
    uint32_t u32[4];
    uint64_t u64[2];
} netadrip_t;

typedef struct netadr_s {
    netadrtype_t type;
    netadrip_t ip;
    uint16_t port;
    uint32_t scope_id;
    uint8_t mask_bits;
} netadr_t;

qboolean net_addressesMatch(netadr_t *a1, netadr_t *a2);
char *net_addressToString(netadr_t *address, qboolean wrapv6, qboolean incport, qboolean incmask);
netadr_t net_cidrToMask(int cidr, netadrtype_t t);
qboolean net_contains(netadr_t *network, netadr_t *host);
void net_parseIP(netadr_t *addr, const char *ip);
netadr_t net_parseIPAddressBase(const char *ip);
netadr_t net_parseIPAddressMask(const char *ip);

static inline bool NET_IsEqualAdr(const netadr_t *a, const netadr_t *b)
{
    if (a->type != b->type) {
        return false;
    }

    switch (a->type) {
    case NA_LOOPBACK:
        return true;
    case NA_IP:
    case NA_BROADCAST:
        return a->ip.u32[0] == b->ip.u32[0] && a->port == b->port;
    case NA_IP6:
        return !memcmp(a->ip.u8, b->ip.u8, 16) && a->port == b->port;
    default:
        return false;
    }
}

static inline bool NET_IsEqualBaseAdr(const netadr_t *a, const netadr_t *b)
{
    if (a->type != b->type) {
        return false;
    }

    switch (a->type) {
    case NA_LOOPBACK:
        return true;
    case NA_IP:
    case NA_BROADCAST:
        return a->ip.u32[0] == b->ip.u32[0];
    case NA_IP6:
        return !memcmp(a->ip.u8, b->ip.u8, 16);
    default:
        return false;
    }
}

static inline bool NET_IsEqualBaseAdrMask(const netadr_t *a,
                                          const netadr_t *b,
                                          const netadr_t *m)
{
    if (a->type != b->type) {
        return false;
    }

    switch (a->type) {
    case NA_IP:
        return !((a->ip.u32[0] ^ b->ip.u32[0]) & m->ip.u32[0]);
    case NA_IP6:
#if (defined __amd64__) || (defined _M_AMD64)
        return !(((a->ip.u64[0] ^ b->ip.u64[0]) & m->ip.u64[0]) |
                 ((a->ip.u64[1] ^ b->ip.u64[1]) & m->ip.u64[1]));
#else
        return !(((a->ip.u32[0] ^ b->ip.u32[0]) & m->ip.u32[0]) |
                 ((a->ip.u32[1] ^ b->ip.u32[1]) & m->ip.u32[1]) |
                 ((a->ip.u32[2] ^ b->ip.u32[2]) & m->ip.u32[2]) |
                 ((a->ip.u32[3] ^ b->ip.u32[3]) & m->ip.u32[3]));
#endif
    default:
        return false;
    }
}
#endif // G_NET_H
