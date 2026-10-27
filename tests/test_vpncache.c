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

/**
 * Saves a few entries, loads them back into a fresh cache, and checks what
 * came back. moddir points at a temporary directory for this.
 */
static void test_save_load(void) {
    char dir[] = "/tmp/q2a_test_vpncache_XXXXXX";
    char path[600];
    netadr_t a4, a6, gone;
    vpn_t v4 = { VPN_POSITIVE, "vpn_detected", 0.92, 0.88, "15169" };
    vpn_t v6 = { VPN_NEGATIVE, "clean", 0.03, 0.95, "" };
    vpn_t out;
    FILE *f;

    if (!mkdtemp(dir)) {
        CHECK(!"mkdtemp failed");
        return;
    }
    snprintf(moddir, sizeof(moddir), "%s", dir);
    snprintf(vpn_cache_file, sizeof(vpn_cache_file), "vpncache.dat");
    snprintf(path, sizeof(path), "%s/%s", dir, vpn_cache_file);

    vpn_cache_size = SLOTS;
    vpn_cache_probe = SLOTS;
    vpnCacheInit();
    a4 = net_parseIPAddressBase("203.0.113.5");
    a6 = net_parseIPAddressBase("2001:db8::face");
    gone = net_parseIPAddressBase("198.51.100.7");
    vpn_cache_ttl = 3600;
    vpnCacheSet(&a4, &v4);
    vpnCacheSet(&a6, &v6);
    vpn_cache_ttl = -1;
    vpnCacheSet(&gone, &v4);

    SECTION("only live entries are saved");
    CHECK_INT(vpnCacheWrite(), 2);

    // a malformed line in the file is skipped, not fatal
    f = fopen(path, "a");
    fputs("not a cache line\n", f);
    fclose(f);

    SECTION("a fresh cache loads them back");
    vpnCacheInit();
    q2a_memset(&out, 0, sizeof(out));
    CHECK(vpnCacheGet(&a4, &out));
    CHECK_INT(out.state, VPN_POSITIVE);
    CHECK_STR(out.verdict, "vpn_detected");
    CHECK_STR(out.asn, "15169");
    CHECK(out.score > 0.919 && out.score < 0.921);
    CHECK(out.confidence > 0.879 && out.confidence < 0.881);

    q2a_memset(&out, 0, sizeof(out));
    CHECK(vpnCacheGet(&a6, &out));
    CHECK_INT(out.state, VPN_NEGATIVE);
    CHECK_STR(out.verdict, "clean");
    CHECK_STR(out.asn, "");

    CHECK(!vpnCacheGet(&gone, &out));

    SECTION("vpncacheload replaces the cache with the file");
    {
        netadr_t added = net_parseIPAddressBase("192.0.2.9");
        netadr_t extra = net_parseIPAddressBase("192.0.2.10");

        // the file gets a hand-added line; memory gets an entry the file lacks
        f = fopen(path, "a");
        fprintf(f, "192.0.2.9 %lld vpn_likely 0.7 0.6 7843\n", (long long)time(NULL) + 600);
        fclose(f);
        vpn_cache_ttl = 3600;
        vpnCacheSet(&extra, &v6);

        vpnCacheLoadRun(0, NULL, -1);
        CHECK(vpnCacheGet(&added, &out));
        CHECK_STR(out.asn, "7843");
        CHECK_INT(out.state, VPN_POSITIVE);
        CHECK(vpnCacheGet(&a4, &out));
        CHECK(!vpnCacheGet(&extra, &out));
    }

    SECTION("vpncacheload leaves the cache alone if the file is missing");
    remove(path);
    vpnCacheLoadRun(0, NULL, -1);
    CHECK(vpnCacheGet(&a4, &out));

    SECTION("an empty vpn_cache_file turns saving off");
    vpn_cache_file[0] = '\0';
    CHECK_INT(vpnCacheWrite(), -1);

    remove(path);
    rmdir(dir);
    snprintf(vpn_cache_file, sizeof(vpn_cache_file), "vpncache.dat");
    snprintf(moddir, sizeof(moddir), "baseq2");
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

    test_save_load();

    vpnCacheFree();
    return test_summary();
}
