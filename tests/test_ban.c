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
 * Runs one ban-file line through parseBanLine() and returns the rule it
 * added, or NULL if the line was rejected.
 */
static baninfo_t *parse(const char *line) {
    char buf[512];
    baninfo_t *before = banhead;

    snprintf(buf, sizeof(buf), "%s", line);
    parseBanLine(buf);
    return banhead != before ? banhead : NULL;
}

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

    SECTION("asn rules match the bare number the VPN check stores");
    CHECK(asnMatches("as7843", "7843"));
    CHECK(asnMatches("AS7843", "7843"));
    CHECK(asnMatches("AS7843", "AS7843"));
    CHECK(!asnMatches("as7843", "78430"));
    CHECK(!asnMatches("as7843", "843"));
    CHECK(!asnMatches("as7843", ""));

    SECTION("asnParse accepts AS plus 1-10 digits, quoted or not");
    {
        char asn[16];

        CHECK(asnParse(asn, sizeof(asn), "as7843", 6));
        CHECK_STR(asn, "as7843");
        CHECK(asnParse(asn, sizeof(asn), "\"AS7843\"", 8));
        CHECK_STR(asn, "AS7843");
        CHECK(asnParse(asn, sizeof(asn), "AS4294967295", 12));
        CHECK_STR(asn, "AS4294967295");
        CHECK(!asnParse(asn, sizeof(asn), "AS12345678901", 13));
        CHECK_STR(asn, "");
        CHECK(!asnParse(asn, sizeof(asn), "7843", 4));
        CHECK(!asnParse(asn, sizeof(asn), "AS", 2));
        CHECK(!asnParse(asn, sizeof(asn), "AS78x3", 6));
        CHECK(!asnParse(asn, sizeof(asn), "\"AS7843", 7));
    }

    SECTION("ban file ASN parsing");
    b = parse("BAN: ASN as7843");
    CHECK(b != NULL);
    CHECK_STR(b ? b->asn : NULL, "as7843");
    b = parse("BAN: ASN \"as7843\" MSG \"bye\"");
    CHECK(b != NULL);
    CHECK_STR(b ? b->asn : NULL, "as7843");
    CHECK_STR(b ? b->msg : NULL, "bye");
    b = parse("BAN: ASN as7843\n");
    CHECK_STR(b ? b->asn : NULL, "as7843");
    // a bad ASN drops the whole rule instead of widening it to everyone
    CHECK(parse("BAN: ASN as1234567890123456789") == NULL);
    CHECK(parse("BAN: ASN 7843") == NULL);

    SECTION("ipTokenParse takes v4/v6 with a mask, rejects junk and overlong tokens");
    {
        netadr_t a;
        const char *v6 = "0000:0000:0000:0000:0000:ffff:192.168.100.228/128";

        CHECK(ipTokenParse(&a, "10.1.2.3", 8));
        CHECK(ipTokenParse(&a, "10.1.2.0/24 trailing", 11));
        CHECK_INT(a.mask_bits, 24);
        CHECK(ipTokenParse(&a, v6, strlen(v6)));
        CHECK_INT(a.mask_bits, 128);
        CHECK(ipTokenParse(&a, "::1", 3));
        CHECK(!ipTokenParse(&a, "10.1.2.999", 10));
        CHECK(!ipTokenParse(&a, "", 0));
        CHECK(!ipTokenParse(&a, "1111111111111111111111111111111111111111111111111111", 52));
    }

    SECTION("ban file IP parsing");
    b = parse("BAN: IP 10.1.2.0/24");
    CHECK(b != NULL);
    CHECK_INT(b ? b->addr.mask_bits : -1, 24);
    b = parse("BAN: IP 10.1.2.3 MSG \"bye\"");
    CHECK_STR(b ? b->msg : NULL, "bye");
    b = parse("BAN: IP ::1");
    CHECK(b != NULL);
    // a bad or overlong address drops the whole rule instead of widening it
    CHECK(parse("BAN: IP 10.1.2.999 VERSION \"q2pro\"") == NULL);
    CHECK(parse("BAN: IP 1111111111111111111111111111111111111111111111111111111111111111 VERSION \"q2pro\"") == NULL);
    CHECK(parse("BAN: IP") == NULL);

    SECTION("a bad VERSION RE drops the rule");
    CHECK(parse("BAN: VERSION RE \"[^\"") == NULL);
    b = parse("BAN: VERSION RE \"^q2pro\"");
    CHECK(b != NULL);

    SECTION("a VPN-only rule loads");
    b = parse("BAN: IP VPN");
    CHECK(b != NULL);
    CHECK(b && b->vpn);

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
