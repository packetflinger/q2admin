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

/**
 * A rule with nothing set but its number and name mode.
 */
static baninfo_t *rule(int type) {
    static baninfo_t b;

    memset(&b, 0, sizeof(b));
    b.bannum = 4;
    b.type = type;
    return &b;
}

int main(void) {
    baninfo_t *b;

    stub_engine_init();

    SECTION("a NULL rule is not dereferenced");
    CHECK_STR(banMatchReason(NULL), "no criteria");

    SECTION("each name mode reads differently");
    b = rule(NICKEQ);
    snprintf(b->nick, sizeof(b->nick), "bob");
    CHECK_STR(banMatchReason(b),
              "name is \"bob\"");
    b = rule(NICKLIKE);
    snprintf(b->nick, sizeof(b->nick), "idiot");
    CHECK_STR(banMatchReason(b),
              "name contains \"idiot\"");
    b = rule(NICKRE);
    snprintf(b->nick, sizeof(b->nick), "^bot[0-9]+");
    CHECK_STR(banMatchReason(b),
              "name matches /^bot[0-9]+/");
    CHECK_STR(banMatchReason(rule(NICKBLANK)),
              "blank name");

    SECTION("address only counts when the rule sets a mask");
    b = rule(NICKALL);
    b->addr = net_parseIPAddressMask("192.0.2.0/24");
    CHECK_STR(banMatchReason(b),
              "any name, ip in 192.0.2.0/24");
    // checkBanList() skips the address test on a zero mask, so neither
    // should this.
    b = rule(NICKALL);
    memset(&b->addr, 0, sizeof(b->addr));
    CHECK_STR(banMatchReason(b),
              "any name");

    SECTION("vpn and asn");
    b = rule(NICKALL);
    b->vpn = true;
    CHECK_STR(banMatchReason(b),
              "any name, on a VPN");
    b = rule(NICKALL);
    snprintf(b->asn, sizeof(b->asn), "AS13335");
    CHECK_STR(banMatchReason(b),
              "any name, ASN AS13335");

    SECTION("version, by match type");
    b = rule(NICKALL);
    b->vtype = VERSION_EQUALS;
    snprintf(b->version, sizeof(b->version), "q2pro r1892");
    CHECK_STR(banMatchReason(b),
              "any name, version is \"q2pro r1892\"");
    b->vtype = VERSION_LIKE;
    CHECK_STR(banMatchReason(b),
              "any name, version contains \"q2pro r1892\"");
    b->vtype = VERSION_REGEX;
    CHECK_STR(banMatchReason(b),
              "any name, version matches /q2pro r1892/");
    // An empty version string means the rule does not test the version,
    // whatever vtype says.
    b = rule(NICKALL);
    b->vtype = VERSION_EQUALS;
    CHECK_STR(banMatchReason(b),
              "any name");

    SECTION("password and connection cap");
    b = rule(NICKALL);
    snprintf(b->password, sizeof(b->password), "letmein");
    // The password is printed: it's the only stable way to tell from the log
    // which allow rule let a player in, since rule numbers shift as rules
    // are added and removed. It's already in plain text in q2a_ban.cfg.
    CHECK_STR(banMatchReason(b),
              "any name, password \"letmein\"");
    b = rule(NICKALL);
    b->maxnumberofconnects = 3;
    CHECK_STR(banMatchReason(b),
              "any name, at most 3 connections");

    SECTION("everything at once, in a fixed order");
    b = rule(NICKLIKE);
    snprintf(b->nick, sizeof(b->nick), "bot");
    b->addr = net_parseIPAddressMask("10.0.0.0/8");
    b->vpn = true;
    snprintf(b->asn, sizeof(b->asn), "AS1");
    b->vtype = VERSION_LIKE;
    snprintf(b->version, sizeof(b->version), "r1q2");
    snprintf(b->password, sizeof(b->password), "pw");
    b->maxnumberofconnects = 2;
    CHECK_STR(banMatchReason(b),
              "name contains \"bot\", ip in 10.0.0.0/8, "
              "on a VPN, ASN AS1, version contains \"r1q2\", password \"pw\", "
              "at most 2 connections");

    SECTION("a rule that tests nothing says so rather than returning empty");
    b = rule(NOTUSED);
    CHECK_STR(banMatchReason(b),
              "no criteria");

    SECTION("an overlong rule truncates instead of overflowing");
    b = rule(NICKEQ);
    memset(b->nick, 'n', sizeof(b->nick) - 1);
    b->vtype = VERSION_EQUALS;
    memset(b->version, 'v', sizeof(b->version) - 1);
    memset(b->password, 'p', sizeof(b->password) - 1);
    b->maxnumberofconnects = 999999;
    {
        const char *r = banMatchReason(b);
        CHECK(strlen(r) < 512);
        CHECK(strncmp(r, "name is \"nnn", 12) == 0);
    }

    return test_summary();
}
