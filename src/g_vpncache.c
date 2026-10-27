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
char vpn_cache_file[256] = "vpncache.dat"; // overridden via q2admin.cfg, in moddir

static vpn_cache_entry_t *vpn_cache;    // allocated by vpnCacheInit(), NULL if disabled
static unsigned int vpn_cache_slots;    // size vpn_cache was allocated with

static int vpnCacheRead(int *skipped);
static void vpnCacheReport(edict_t *ent, int loaded, int skipped, bool missing);

/**
 * Allocates the VPN result cache with vpn_cache_size slots. The size is
 * fixed from then on: changing vpn_cache_size later only takes effect at
 * the next server start, the same as whois_active. A size of 0 or less
 * leaves the cache disabled, so every connect does a live lookup.
 * Entries saved by the last server are then loaded back in.
 *
 * Called from InitGame() (g_init.c), after the config files are read.
 */
void vpnCacheInit(void) {
    int skipped;

    vpnCacheFree();
    if (vpn_cache_size <= 0) {
        return;
    }
    vpn_cache_slots = (unsigned int)vpn_cache_size;
    vpn_cache = G_Malloc(vpn_cache_slots * sizeof(vpn_cache_entry_t));
    q2a_memset(vpn_cache, 0, vpn_cache_slots * sizeof(vpn_cache_entry_t));
    vpnCacheReport(NULL, vpnCacheRead(&skipped), skipped, false);
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
 * Stores/updates the cached VPN check result for an address, valid until
 * expires. Reuses a matching, empty, or expired slot within the probe
 * window if one is found; otherwise evicts whichever slot in that window
 * expires soonest. Does nothing if the cache is disabled.
 */
static void vpnCacheStore(netadr_t *addr, const vpn_t *result, time_t expires) {
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
    victim->expires = expires;
    victim->result = *result;
}

/**
 * Caches the VPN check result for an address for vpn_cache_ttl seconds.
 * See vpnCacheStore() for which slot it goes in.
 */
void vpnCacheSet(netadr_t *addr, const vpn_t *result) {
    vpnCacheStore(addr, result, time(NULL) + vpn_cache_ttl);
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

/**
 * The state a verdict puts a player in: on a VPN for vpn_likely or
 * vpn_detected, not for anything else.
 *
 * Called from vpnFinishLookup() (g_vpn.c) for a fresh result, and from
 * vpnCacheRead() for a saved one.
 */
vpn_state_t vpnVerdictState(const char *verdict) {
    if (Q_stricmp((char *)verdict, "vpn_likely") == 0 || Q_stricmp((char *)verdict, "vpn_detected") == 0) {
        return VPN_POSITIVE;
    }
    return VPN_NEGATIVE;
}

/**
 * Builds the path of the saved cache, moddir/vpn_cache_file. Returns false
 * if vpn_cache_file is empty, which turns saving and loading off.
 */
static bool vpnCacheFilePath(char *out, size_t size) {
    if (!vpn_cache_file[0]) {
        return false;
    }
    Q_snprintf(out, size, "%s/%s", moddir, vpn_cache_file);
    return true;
}

/**
 * Writes every live cache entry to moddir/vpn_cache_file, one per line:
 *
 *   <ip> <expires> <verdict> <score> <confidence> <asn>
 *
 * expires is a Unix time, and an empty verdict or ASN is written as "-" to
 * keep the columns positional. Expired entries are left out.
 *
 * Returns how many entries were written, or -1 if the cache is disabled,
 * vpn_cache_file is empty, or the file couldn't be opened. Leaves an
 * existing file alone in those cases.
 *
 * Called from ShutdownGame() (g_main.c), SpawnEntities() (g_init.c) so a
 * crash loses at most one map's worth of lookups, and vpnCacheSaveRun().
 */
int vpnCacheWrite(void) {
    char path[512];
    FILE *f;
    int count = 0;
    time_t now = time(NULL);

    if (!vpn_cache || !vpnCacheFilePath(path, sizeof(path))) {
        return -1;
    }
    f = fopen(path, "w");
    if (!f) {
        gi.dprintf("[q2a] couldn't write VPN cache file %s\n", path);
        return -1;
    }
    for (unsigned int i = 0; i < vpn_cache_slots; i++) {
        vpn_cache_entry_t *e = &vpn_cache[i];

        if (!e->used || now >= e->expires) {
            continue;
        }
        fprintf(f, "%s %lld %s %.4f %.4f %s\n",
            net_addressToString(&e->addr, false, false, false),
            (long long)e->expires,
            e->result.verdict[0] ? e->result.verdict : "-",
            e->result.score, e->result.confidence,
            e->result.asn[0] ? e->result.asn : "-");
        count++;
    }
    fclose(f);
    return count;
}

/**
 * Replaces the cache's contents with the entries saved by vpnCacheWrite(),
 * keeping each one's original expiry time. Expired and malformed lines are
 * skipped. If the file holds more than fits (vpn_cache_size was lowered),
 * the usual eviction decides what stays. The cache is only cleared once the
 * file has been opened, so a missing file leaves it as it was.
 *
 * skipped: set to how many malformed lines were skipped.
 *
 * Returns how many entries were loaded, or -1 if vpn_cache_file is empty or
 * the file couldn't be opened.
 *
 * Called from vpnCacheInit(), right after the cache is allocated, and from
 * vpnCacheLoadRun().
 */
static int vpnCacheRead(int *skipped) {
    char path[512], line[256], ip[64], verdict[16], asn[16];
    long long expires;
    netadr_t addr;
    vpn_t v;
    FILE *f;
    int loaded = 0;
    time_t now = time(NULL);

    *skipped = 0;
    if (!vpnCacheFilePath(path, sizeof(path))) {
        return -1;
    }
    f = fopen(path, "r");
    if (!f) {
        return -1;
    }
    q2a_memset(vpn_cache, 0, vpn_cache_slots * sizeof(vpn_cache_entry_t));
    while (fgets(line, sizeof(line), f)) {
        q2a_memset(&v, 0, sizeof(v));
        if (sscanf(line, "%63s %lld %15s %lf %lf %15s", ip, &expires, verdict,
                   &v.score, &v.confidence, asn) != 6) {
            (*skipped)++;
            continue;
        }
        addr = net_parseIPAddressBase(ip);
        if (addr.type == NA_UNSPECIFIED) {
            (*skipped)++;
            continue;
        }
        if ((time_t)expires <= now) {
            continue;
        }
        if (q2a_strcmp(verdict, "-") != 0) {
            Q_strlcpy(v.verdict, verdict, sizeof(v.verdict));
        }
        if (q2a_strcmp(asn, "-") != 0) {
            Q_strlcpy(v.asn, asn, sizeof(v.asn));
        }
        v.state = vpnVerdictState(v.verdict);
        vpnCacheStore(&addr, &v, (time_t)expires);
        loaded++;
    }
    fclose(f);
    return loaded;
}

/**
 * Console command "vpncachesave": writes the cache to moddir/vpn_cache_file
 * now, rather than waiting for the next map change or shutdown.
 */
void vpnCacheSaveRun(int startarg, edict_t *ent, int client) {
    char path[512];
    int count;

    (void)startarg; (void)client;

    if (!vpn_cache) {
        gi.cprintf(ent, PRINT_HIGH, "VPN cache is disabled (vpn_cache_size was %d at server start)\n",
            vpn_cache_size);
        return;
    }
    if (!vpnCacheFilePath(path, sizeof(path))) {
        gi.cprintf(ent, PRINT_HIGH, "vpn_cache_file is empty, so the VPN cache isn't saved\n");
        return;
    }
    count = vpnCacheWrite();
    if (count < 0) {
        gi.cprintf(ent, PRINT_HIGH, "Couldn't write the VPN cache to %s\n", path);
        return;
    }
    gi.cprintf(ent, PRINT_HIGH, "Saved %d VPN cache entr%s to %s\n", count, count == 1 ? "y" : "ies", path);
}

/**
 * Prints the result of loading the cache file.
 *
 * ent:     who to print to, NULL for the server console.
 * loaded:  what vpnCacheRead() returned.
 * skipped: malformed lines vpnCacheRead() skipped.
 * missing: whether to say so when the file couldn't be read. A missing file
 *          is normal on first start, so the automatic load stays quiet.
 */
static void vpnCacheReport(edict_t *ent, int loaded, int skipped, bool missing) {
    char path[512];

    if (!vpnCacheFilePath(path, sizeof(path))) {
        if (missing) {
            gi.cprintf(ent, PRINT_HIGH, "vpn_cache_file is empty, so there's no VPN cache file to load\n");
        }
        return;
    }
    if (loaded < 0) {
        if (missing) {
            gi.cprintf(ent, PRINT_HIGH, "Couldn't read %s, the VPN cache is unchanged\n", path);
        }
        return;
    }
    gi.cprintf(ent, PRINT_HIGH, "Loaded %d VPN cache entr%s from %s%s\n", loaded, loaded == 1 ? "y" : "ies",
        path, skipped ? va(" (%d malformed line%s skipped)", skipped, skipped == 1 ? "" : "s") : "");
}

/**
 * Console command "vpncacheload": replaces the cache with what's in
 * moddir/vpn_cache_file, for when the file has been edited by hand while
 * the server is running. Lookups since the last save are lost, so run
 * vpncachesave first to keep them.
 */
void vpnCacheLoadRun(int startarg, edict_t *ent, int client) {
    int loaded, skipped;

    (void)startarg; (void)client;

    if (!vpn_cache) {
        gi.cprintf(ent, PRINT_HIGH, "VPN cache is disabled (vpn_cache_size was %d at server start)\n",
            vpn_cache_size);
        return;
    }
    loaded = vpnCacheRead(&skipped);
    vpnCacheReport(ent, loaded, skipped, true);
}
