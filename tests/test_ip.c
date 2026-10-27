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
#include "q2a_test.h"
#include "stub_engine.h"

#define SLOTS   8
#define ROUNDS  5000

static netadr_t addrs[SLOTS];
static bool     live[SLOTS];

/**
 * Fills a small cache with a random mix of live and already-expired
 * entries. The probe window covers the whole cache and there are never
 * more entries than slots, so no live entry gets evicted: anything live
 * that can't be found afterwards was lost by a removal.
 */
static void fill(unsigned int seed) {
    vpn_t v = { VPN_NEGATIVE, "clean", 0.1, 0.9, "15169" };
    char ip[32];

    srand(seed);
    vpnCacheInit();
    for (int i = 0; i < SLOTS; i++) {
        snprintf(ip, sizeof(ip), "10.0.%d.%d:27910", rand() % 4, rand() % 256);
        net_parseIP(&addrs[i], ip);
        live[i] = rand() % 2;
        vpn_cache_ttl = live[i] ? 3600 : -1;
        vpnCacheSet(&addrs[i], &v);
    }
    // a repeated address keeps only the last result stored for it
    for (int i = 0; i < SLOTS; i++) {
        for (int j = i + 1; j < SLOTS; j++) {
            if (NET_IsEqualBaseAdr(&addrs[i], &addrs[j])) {
                live[i] = live[j];
            }
        }
    }
}

/**
 * Whether every address is found exactly when it's live.
 */
static bool consistent(void) {
    vpn_t out;

    for (int i = 0; i < SLOTS; i++) {
        if (vpnCacheGet(&addrs[i], &out) != live[i]) {
            return false;
        }
    }
    return true;
}

/**
 * vpncacheprune prints a line per call; hundreds of rounds of that would
 * bury the results.
 */
static void quiet_cprintf(edict_t *ent, int level, char *fmt, ...) {
    (void)ent; (void)level; (void)fmt;
}

int main(void) {
    int bad;

    stub_engine_init();
    gi.cprintf = quiet_cprintf;
    vpn_cache_size = SLOTS;
    vpn_cache_probe = SLOTS;

    SECTION("pruning keeps every live entry findable");
    bad = 0;
    for (int r = 0; r < ROUNDS; r++) {
        fill(r);
        vpnCachePruneRun(0, NULL, -1);
        bad += !consistent();
    }
    CHECK_INT(bad, 0);

    SECTION("an expired entry removed by a lookup doesn't hide later ones");
    bad = 0;
    for (int r = 0; r < ROUNDS; r++) {
        vpn_t out;

        fill(r);
        // look up the expired ones first, so they're removed before the
        // live ones are looked for
        for (int i = 0; i < SLOTS; i++) {
            if (!live[i]) {
                vpnCacheGet(&addrs[i], &out);
            }
        }
        bad += !consistent();
    }
    CHECK_INT(bad, 0);

    vpnCacheFree();
    return test_summary();
}
