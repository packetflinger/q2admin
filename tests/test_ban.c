/**
 * banMatchReason(): turning a matched ban rule back into readable terms for
 * the log.
 *
 * A rule's criteria are ANDed, so these mostly check that every field the
 * rule sets shows up, and that fields it leaves alone stay out.
 */

#include "g_local.h"
#include "q2a_test.h"
#include "stub_engine.h"

/* A rule with nothing set but its number and name mode. */
static baninfo_t *rule(int type)
{
    static baninfo_t b;

    memset(&b, 0, sizeof(b));
    b.bannum = 4;
    b.type = type;
    return &b;
}

int main(void)
{
    baninfo_t *b;

    stub_engine_init();

    SECTION("no match at all");
    CHECK_STR(banMatchReason(BT_NOTFOUND, NULL), "no rule matched");
    /* An entry passed with BT_NOTFOUND is ignored, not described. */
    CHECK_STR(banMatchReason(BT_NOTFOUND, rule(NICKALL)), "no rule matched");
    /* A NULL entry must not be dereferenced whatever the result says. */
    CHECK_STR(banMatchReason(BT_DENYLISTED, NULL), "no rule matched");

    SECTION("the verb comes from the result");
    CHECK_STR(banMatchReason(BT_DENYLISTED, rule(NICKALL)),
              "denylisted by ban 4 (any name)");
    CHECK_STR(banMatchReason(BT_ALLOWLISTED, rule(NICKALL)),
              "allowlisted by ban 4 (any name)");

    SECTION("each name mode reads differently");
    b = rule(NICKEQ);
    snprintf(b->nick, sizeof(b->nick), "bob");
    CHECK_STR(banMatchReason(BT_DENYLISTED, b),
              "denylisted by ban 4 (name is \"bob\")");
    b = rule(NICKLIKE);
    snprintf(b->nick, sizeof(b->nick), "idiot");
    CHECK_STR(banMatchReason(BT_DENYLISTED, b),
              "denylisted by ban 4 (name contains \"idiot\")");
    b = rule(NICKRE);
    snprintf(b->nick, sizeof(b->nick), "^bot[0-9]+");
    CHECK_STR(banMatchReason(BT_DENYLISTED, b),
              "denylisted by ban 4 (name matches /^bot[0-9]+/)");
    CHECK_STR(banMatchReason(BT_DENYLISTED, rule(NICKBLANK)),
              "denylisted by ban 4 (blank name)");

    SECTION("address only counts when the rule sets a mask");
    b = rule(NICKALL);
    b->addr = net_parseIPAddressMask("192.0.2.0/24");
    CHECK_STR(banMatchReason(BT_DENYLISTED, b),
              "denylisted by ban 4 (any name, ip in 192.0.2.0/24)");
    /* checkBanList() skips the address test on a zero mask, so neither
     * should this. */
    b = rule(NICKALL);
    memset(&b->addr, 0, sizeof(b->addr));
    CHECK_STR(banMatchReason(BT_DENYLISTED, b),
              "denylisted by ban 4 (any name)");

    SECTION("vpn and asn");
    b = rule(NICKALL);
    b->vpn = true;
    CHECK_STR(banMatchReason(BT_DENYLISTED, b),
              "denylisted by ban 4 (any name, on a VPN)");
    b = rule(NICKALL);
    snprintf(b->asn, sizeof(b->asn), "AS13335");
    CHECK_STR(banMatchReason(BT_DENYLISTED, b),
              "denylisted by ban 4 (any name, ASN AS13335)");

    SECTION("version, by match type");
    b = rule(NICKALL);
    b->vtype = VERSION_EQUALS;
    snprintf(b->version, sizeof(b->version), "q2pro r1892");
    CHECK_STR(banMatchReason(BT_DENYLISTED, b),
              "denylisted by ban 4 (any name, version is \"q2pro r1892\")");
    b->vtype = VERSION_LIKE;
    CHECK_STR(banMatchReason(BT_DENYLISTED, b),
              "denylisted by ban 4 (any name, version contains \"q2pro r1892\")");
    b->vtype = VERSION_REGEX;
    CHECK_STR(banMatchReason(BT_DENYLISTED, b),
              "denylisted by ban 4 (any name, version matches /q2pro r1892/)");
    /* An empty version string means the rule does not test the version,
     * whatever vtype says. */
    b = rule(NICKALL);
    b->vtype = VERSION_EQUALS;
    CHECK_STR(banMatchReason(BT_DENYLISTED, b),
              "denylisted by ban 4 (any name)");

    SECTION("password and connection cap");
    b = rule(NICKALL);
    snprintf(b->password, sizeof(b->password), "letmein");
    /* The password itself is never printed - this goes into a log. */
    CHECK_STR(banMatchReason(BT_ALLOWLISTED, b),
              "allowlisted by ban 4 (any name, password)");
    CHECK(strstr(banMatchReason(BT_ALLOWLISTED, b), "letmein") == NULL);
    b = rule(NICKALL);
    b->maxnumberofconnects = 3;
    CHECK_STR(banMatchReason(BT_ALLOWLISTED, b),
              "allowlisted by ban 4 (any name, at most 3 connections)");

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
    CHECK_STR(banMatchReason(BT_DENYLISTED, b),
              "denylisted by ban 4 (name contains \"bot\", ip in 10.0.0.0/8, "
              "on a VPN, ASN AS1, version contains \"r1q2\", password, "
              "at most 2 connections)");

    SECTION("a rule with no criteria says so rather than printing ()");
    b = rule(NOTUSED);
    CHECK_STR(banMatchReason(BT_DENYLISTED, b),
              "denylisted by ban 4 (no criteria)");

    SECTION("an overlong rule truncates instead of overflowing");
    b = rule(NICKEQ);
    memset(b->nick, 'n', sizeof(b->nick) - 1);
    b->vtype = VERSION_EQUALS;
    memset(b->version, 'v', sizeof(b->version) - 1);
    memset(b->password, 'p', sizeof(b->password) - 1);
    b->maxnumberofconnects = 999999;
    {
        const char *r = banMatchReason(BT_DENYLISTED, b);
        CHECK(strlen(r) < 512);
        CHECK(strncmp(r, "denylisted by ban 4 (name is ", 29) == 0);
    }

    return test_summary();
}
