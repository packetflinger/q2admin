/**
 * Copyright (C) The Q2Admin contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#include "g_local.h"

int vpn_cache_probe = 8;     // overridden via q2admin.cfg
int vpn_cache_size  = 1024;  // overridden via q2admin.cfg, only read at server start
int vpn_cache_ttl   = 86400; // 1 day, overridden via q2admin.cfg

static vpn_cache_entry_t *vpn_cache;    // allocated by vpnCacheInit(), NULL if disabled
static unsigned int vpn_cache_slots;    // size vpn_cache was allocated with

/**
 * Allocates the VPN result cache with vpn_cache_size slots. The size is
 * fixed from then on: changing vpn_cache_size later only takes effect at
 * the next server start, the same as whois_active. A size of 0 or less
 * leaves the cache disabled, so every connect does a live lookup.
 *
 * Called from InitGame() (g_init.c), after the config files are read.
 */
void vpnCacheInit(void) {
    vpnCacheFree();
    if (vpn_cache_size <= 0) {
        return;
    }
    vpn_cache_slots = (unsigned int)vpn_cache_size;
    vpn_cache = G_Malloc(vpn_cache_slots * sizeof(vpn_cache_entry_t));
    q2a_memset(vpn_cache, 0, vpn_cache_slots * sizeof(vpn_cache_entry_t));
}

/**
 * Frees the VPN result cache, if there is one.
 *
 * Called from ShutdownGame() (g_main.c), and from vpnCacheInit() in case
 * it's ever called twice.
 */
void vpnCacheFree(void) {
    if (vpn_cache) {
        G_Free(vpn_cache);
        vpn_cache = NULL;
    }
    vpn_cache_slots = 0;
}

/**
 * How many slots past an address' home slot to search, from
 * vpn_cache_probe. It can be changed at any time, so it's kept between 1
 * and the number of slots here rather than when it's set.
 */
static unsigned int vpnCacheProbeLimit(void) {
    if (vpn_cache_probe < 1) {
        return 1;
    }
    if ((unsigned int)vpn_cache_probe > vpn_cache_slots) {
        return vpn_cache_slots;
    }
    return (unsigned int)vpn_cache_probe;
}

/**
 * Fast FNV-1a hash over an address' raw bytes. Only the base address
 * (not port) is hashed, since that's all vpnCacheGet/Set key on.
 */
static unsigned int vpnCacheHash(netadr_t *addr) {
    unsigned int h = 2166136261u;
    int len = (addr->type == NA_IP6) ? IP6_LEN : IP4_LEN;

    for (int i = 0; i < len; i++) {
        h ^= addr->ip.u8[i];
        h *= 16777619u;
    }
    return h;
}

/**
 * Looks up the cached VPN check result for an address. Returns false if
 * the cache is disabled, there's no entry within the probe window, or the
 * entry has expired (freeing the slot for reuse in that case).
 */
bool vpnCacheGet(netadr_t *addr, vpn_t *out) {
    unsigned int idx, limit;

    if (!vpn_cache) {
        return false;
    }
    idx = vpnCacheHash(addr) % vpn_cache_slots;
    limit = vpnCacheProbeLimit();

    for (unsigned int probe = 0; probe < limit; probe++) {
        vpn_cache_entry_t *e = &vpn_cache[(idx + probe) % vpn_cache_slots];

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
 * Stores/updates the cached VPN check result for an address, valid for
 * vpn_cache_ttl seconds. Reuses a matching, empty, or expired slot
 * within the probe window if one is found; otherwise evicts whichever
 * slot in that window expires soonest. Does nothing if the cache is
 * disabled.
 */
void vpnCacheSet(netadr_t *addr, const vpn_t *result) {
    unsigned int idx, limit;
    vpn_cache_entry_t *victim = NULL;

    if (!vpn_cache) {
        return;
    }
    idx = vpnCacheHash(addr) % vpn_cache_slots;
    limit = vpnCacheProbeLimit();

    for (unsigned int probe = 0; probe < limit; probe++) {
        vpn_cache_entry_t *e = &vpn_cache[(idx + probe) % vpn_cache_slots];

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
    victim->expires = time(NULL) + vpn_cache_ttl;
    victim->result = *result;
}
