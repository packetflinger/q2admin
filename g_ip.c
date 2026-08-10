// This file provides functionality around the context of an IP address that a
// player connects from. Is it a residential address directly assigned by an
// ISP? Is it used in a datacenter? Is it a VPN? What autonomous system number
// is it associated with? What's the parent prefix?
//
// With the rise of VPN accessibility and decreasing costs, players with
// malicious intent often mask their identity and true point of origin using
// VPNs. This provides them with an effectively limitless amount of unique IP
// addresses to use that can be swapped and changed at will. This makes keeping
// these players off your servers difficult. The easiest way to filter these
// players out is to not allow VPN connections at all. The problem becomes
// identifying which players are using VPNs.
//
// There are legitimate use-cases for VPN access. Some players report vastly
// different (and superior) routing when using a VPN resulting in a more stable
// connection with a lower ping. If all VPNs are disallowed, exceptions need to
// be possible for these situations like this.

#include "g_local.h"

ipcontext_t noipcontext = {
        .found = false,
        .asnumber = 0,
        .prefix = 0,
        .vpn = false,
        .datacenter = false,
        .score = 0.0,
};

/**
 * Query the database for the given IP.
 */
ipcontext_t IP_Lookup(sqlite3 *db, netadr_t addr) {
    int ret;
    sqlite3_stmt *st;
    ipcontext_t out;
    int len;

    if (!db) {
        return noipcontext;
    }

    const char *sql = "SELECT asn, cidr FROM vpn_prefix WHERE first <= ? AND last > ?;";

    ret = sqlite3_prepare_v2(db, sql, -1, &st, NULL);
    if (ret != SQLITE_OK) {
        gi.cprintf(NULL, PRINT_HIGH, "Error looking up IP %s: %s\n", IPSTRMASK(&addr), sqlite3_errmsg(db));
        return out;
    }

    len = (addr.type == NA_IP6) ? IP6_LEN : IP4_LEN;
    sqlite3_bind_blob(st, 1, &addr.ip.u8, len, SQLITE_TRANSIENT);
    sqlite3_bind_blob(st, 2, &addr.ip.u8, len, SQLITE_TRANSIENT);

    while ((ret = sqlite3_step(st)) == SQLITE_ROW) {
       out.asnumber = sqlite3_column_int(st, 0);
       q2a_memset(&out.prefix, 0, sizeof(out.prefix));
       strncpy(out.prefix, sqlite3_column_text(st, 1), sizeof(out.prefix));
       out.found = true;
    }
    sqlite3_finalize(st);
    return out;
}

/**
 * Open the database file and return the handle
 */
sqlite3 *IP_OpenDatabase(const char *dbfile) {
    sqlite3 *db;
    int ret;

    if (dbfile == NULL) {
        return NULL;
    }

    ret = sqlite3_open(dbfile, &db);
    if (ret != SQLITE_OK) {
        gi.cprintf(NULL, PRINT_HIGH, "Unable to open IP database \"%s\": %s\n", dbfile, sqlite3_errmsg(db));
        return NULL;
    }
    return db;
}

