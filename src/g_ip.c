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
 * Empties a cache slot. Lookups stop at the first empty slot, so simply
 * clearing one would hide any entry stored further along the same run of
 * used slots. Instead, each later entry in the run that would still be
 * found from the gap (the gap lies between its home slot and where it is
 * now) is moved back into it, and the slot it left becomes the new gap.
 * Entries only ever move closer to their home slot, so none end up past
 * vpn_cache_probe.
 */
static void vpnCacheRemove(unsigned int gap) {
    unsigned int j = gap;

    vpn_cache[gap].used = false;
    for (;;) {
        unsigned int home, gapdist, jdist;

        j = (j + 1) % vpn_cache_slots;
        if (!vpn_cache[j].used) {
            break;
        }
        home = vpnCacheHash(&vpn_cache[j].addr) % vpn_cache_slots;
        gapdist = (gap + vpn_cache_slots - home) % vpn_cache_slots;
        jdist = (j + vpn_cache_slots - home) % vpn_cache_slots;
        if (gapdist < jdist) {
            vpn_cache[gap] = vpn_cache[j];
            vpn_cache[j].used = false;
            gap = j;
        }
    }
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
                vpnCacheRemove((idx + probe) % vpn_cache_slots);
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

/**
 * Formats a number of seconds as e.g. "3h12m", "4m05s" or "9s" for the
 * vpncache listing.
 */
static void vpnCacheFormatTime(char *out, size_t size, long secs) {
    if (secs >= 3600) {
        snprintf(out, size, "%ldh%02ldm", secs / 3600, (secs % 3600) / 60);
    } else if (secs >= 60) {
        snprintf(out, size, "%ldm%02lds", secs / 60, secs % 60);
    } else {
        snprintf(out, size, "%lds", secs);
    }
}

#define VPN_CACHE_ROWS_PER_FRAME 30   // entries vpncache prints per server frame

/**
 * Console command "vpncache": prints a summary of the cache's settings and
 * how full it is, then queues QCMD_DISPVPNCACHE to list the live entries a
 * few at a time. Expired entries still hold their slot until it's reused,
 * so they're counted but not listed.
 */
void vpnCacheRun(int startarg, edict_t *ent, int client) {
    unsigned int live = 0, expired = 0;
    time_t now = time(NULL);

    (void)startarg;

    if (!vpn_cache) {
        gi.cprintf(ent, PRINT_HIGH, "VPN cache is disabled (vpn_cache_size was %d at server start)\n",
            vpn_cache_size);
        return;
    }

    for (unsigned int i = 0; i < vpn_cache_slots; i++) {
        if (!vpn_cache[i].used) {
            continue;
        }
        if (now >= vpn_cache[i].expires) {
            expired++;
        } else {
            live++;
        }
    }

    gi.cprintf(ent, PRINT_HIGH, "VPN cache: %u of %u slots live, %u expired; probe %u, ttl %ds\n",
        live, vpn_cache_slots, expired, vpnCacheProbeLimit(), vpn_cache_ttl);
    if (live) {
        addCmdQueue(client, QCMD_DISPVPNCACHE, 0, 0, 0);
    }
}

/**
 * Prints up to VPN_CACHE_ROWS_PER_FRAME live cache entries, starting the
 * scan at slot, then re-queues itself (QCMD_DISPVPNCACHE) with the slot to
 * carry on from on the next frame. Printing a large cache in one go would
 * stall the server frame, the same reason displayNextBan() pages its list.
 *
 * ent:    who to print to, NULL for the server console.
 * client: their client index, used to requeue the next batch for them.
 * slot:   cache slot to resume scanning at.
 *
 * Called from G_RunFrame()'s QCMD_DISPVPNCACHE handling (g_main.c), first
 * queued by vpnCacheRun() at slot 0.
 */
void displayNextVPNCache(edict_t *ent, int client, unsigned long slot) {
    unsigned int printed = 0;
    time_t now = time(NULL);
    char left[32];

    // the cache can be freed (server shutdown) between frames
    if (!vpn_cache) {
        return;
    }

    for (; slot < vpn_cache_slots; slot++) {
        vpn_cache_entry_t *e = &vpn_cache[slot];

        if (!e->used || now >= e->expires) {
            continue;
        }
        if (printed == VPN_CACHE_ROWS_PER_FRAME) {
            addCmdQueue(client, QCMD_DISPVPNCACHE, 0, slot, 0);
            return;
        }
        printed++;
        vpnCacheFormatTime(left, sizeof(left), (long)(e->expires - now));
        gi.cprintf(ent, PRINT_HIGH, "  %-39s AS%-10s %-12s score %.2f conf %.2f expires in %s\n",
            net_addressToString(&e->addr, false, false, false),
            e->result.asn[0] ? e->result.asn : "-",
            e->result.verdict[0] ? e->result.verdict : "-",
            e->result.score, e->result.confidence, left);
    }
    gi.cprintf(ent, PRINT_HIGH, "End of VPN cache.\n");
}

/**
 * Console command "vpncacheprune": removes every expired entry from the
 * cache now, rather than waiting for its slot to be reused, and reports
 * how many went and how many live entries remain.
 */
void vpnCachePruneRun(int startarg, edict_t *ent, int client) {
    unsigned int pruned = 0, live = 0;
    time_t now = time(NULL);

    (void)startarg; (void)client;

    if (!vpn_cache) {
        gi.cprintf(ent, PRINT_HIGH, "VPN cache is disabled (vpn_cache_size was %d at server start)\n",
            vpn_cache_size);
        return;
    }

    // Removing a slot can move a later entry back into it, so a slot is
    // only stepped past once what's in it now has been checked.
    for (unsigned int i = 0; i < vpn_cache_slots; ) {
        if (vpn_cache[i].used && now >= vpn_cache[i].expires) {
            vpnCacheRemove(i);
            pruned++;
        } else {
            i++;
        }
    }

    for (unsigned int i = 0; i < vpn_cache_slots; i++) {
        if (vpn_cache[i].used) {
            live++;
        }
    }

    gi.cprintf(ent, PRINT_HIGH, "Pruned %u expired VPN cache entr%s, %u live of %u slots\n",
        pruned, pruned == 1 ? "y" : "ies", live, vpn_cache_slots);
}
