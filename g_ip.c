// Cache of IPLogs VPN-check results, keyed by client IP address. See g_ip.h.

#include "g_local.h"

int iplogs_cache_ttl = 86400; // 1 day, overridden via q2admin.cfg

static iplogs_cache_entry_t iplogs_cache[IPLOGS_CACHE_SIZE]; // zero-initialized (all unused) by the loader

/**
 * Fast FNV-1a hash over an address' raw bytes. Only the base address
 * (not port) is hashed, since that's all IPLogsCacheGet/Set key on.
 */
static unsigned int IPLogsCacheHash(netadr_t *addr) {
    unsigned int h = 2166136261u;
    int len = (addr->type == NA_IP6) ? IP6_LEN : IP4_LEN;

    for (int i = 0; i < len; i++) {
        h ^= addr->ip.u8[i];
        h *= 16777619u;
    }
    return h;
}

/**
 * Looks up the cached IPLogs result for an address. Returns false if
 * there's no entry within the probe window, or the entry has expired
 * (freeing the slot for reuse in that case).
 */
bool IPLogsCacheGet(netadr_t *addr, iplogsvpn_t *out) {
    unsigned int idx = IPLogsCacheHash(addr) & (IPLOGS_CACHE_SIZE - 1);

    for (int probe = 0; probe < IPLOGS_CACHE_PROBE; probe++) {
        iplogs_cache_entry_t *e = &iplogs_cache[(idx + probe) & (IPLOGS_CACHE_SIZE - 1)];

        if (!e->used) {
            return false; // empty slot ends the chain, nothing further was ever inserted here
        }
        if (NET_IsEqualBaseAdr(&e->addr, addr)) {
            if (time(NULL) >= e->expires) {
                e->used = false;
                return false;
            }
            *out = e->result;
            return true;
        }
    }
    return false;
}

/**
 * Stores/updates the cached IPLogs result for an address, valid for
 * iplogs_cache_ttl seconds. Reuses a matching, empty, or expired slot
 * within the probe window if one is found; otherwise evicts whichever
 * slot in that window expires soonest.
 */
void IPLogsCacheSet(netadr_t *addr, const iplogsvpn_t *result) {
    unsigned int idx = IPLogsCacheHash(addr) & (IPLOGS_CACHE_SIZE - 1);
    iplogs_cache_entry_t *victim = NULL;

    for (int probe = 0; probe < IPLOGS_CACHE_PROBE; probe++) {
        iplogs_cache_entry_t *e = &iplogs_cache[(idx + probe) & (IPLOGS_CACHE_SIZE - 1)];

        if (!e->used || time(NULL) >= e->expires || NET_IsEqualBaseAdr(&e->addr, addr)) {
            victim = e;
            break;
        }
        if (!victim || e->expires < victim->expires) {
            victim = e;
        }
    }

    victim->used = true;
    victim->addr = *addr;
    victim->expires = time(NULL) + iplogs_cache_ttl;
    victim->result = *result;
}
