/**
 * g_net.c: parsing and formatting IP addresses. No engine state involved,
 * so these are the cheap kind of test - link the object and call it.
 */

#include "g_local.h"
#include "q2a_test.h"

int main(void)
{
    netadr_t a, b, n;
    char big[600];

    SECTION("net_parseIP accepts address:port");
    CHECK(net_parseIP(&a, "192.0.2.4:1234"));
    CHECK_INT(a.type, NA_IP);
    CHECK_INT(a.port, 1234);
    CHECK_INT(a.mask_bits, 32);
    CHECK_STR(net_addressToString(&a, false, false, false), "192.0.2.4");

    CHECK(net_parseIP(&a, "[2001:db8::face]:23456"));
    CHECK_INT(a.type, NA_IP6);
    CHECK_INT(a.port, 23456);
    CHECK_STR(net_addressToString(&a, false, false, false), "2001:db8::face");

    SECTION("net_parseIP rejects what it cannot parse, leaving the address unset");
    /* A bare IPv6 has no "]:" so it used to fall into the IPv4 branch and
     * silently become 0.0.0.0, which made every such client look like the
     * same address to ip_limit. */
    CHECK(!net_parseIP(&a, "2001:db8::1"));
    CHECK_INT(a.type, NA_UNSPECIFIED);
    CHECK(!net_parseIP(&a, "192.0.2.4"));          /* no port */
    CHECK(!net_parseIP(&a, "not-an-ip:99"));
    CHECK(!net_parseIP(&a, ""));
    CHECK(!net_parseIP(&a, ":1234"));              /* empty address */

    SECTION("net_parseIP bounds an overlong input");
    memset(big, 'A', sizeof(big));
    big[sizeof(big) - 1] = 0;
    strcpy(big + 500, ":1234");
    CHECK(!net_parseIP(&a, big));

    SECTION("net_addressesMatch ignores the port, and never matches unparsed");
    net_parseIP(&a, "192.0.2.4:1");
    net_parseIP(&b, "192.0.2.4:2");
    CHECK(net_addressesMatch(&a, &b));
    net_parseIP(&b, "192.0.2.5:1");
    CHECK(!net_addressesMatch(&a, &b));
    net_parseIP(&a, "bogus1");
    net_parseIP(&b, "bogus2");
    CHECK(!net_addressesMatch(&a, &b));

    SECTION("net_parseIPAddressMask and net_contains");
    n = net_parseIPAddressMask("192.0.2.0/24");
    CHECK_INT(n.mask_bits, 24);
    net_parseIP(&a, "192.0.2.77:1");
    CHECK(net_contains(&n, &a));
    net_parseIP(&a, "192.0.3.77:1");
    CHECK(!net_contains(&n, &a));

    n = net_parseIPAddressMask("2001:db8::/32");
    net_parseIP(&a, "[2001:db8::dead:beef]:1");
    CHECK(net_contains(&n, &a));
    net_parseIP(&a, "[2001:db9::1]:1");
    CHECK(!net_contains(&n, &a));

    CHECK_INT(net_parseIPAddressMask("192.0.2.5").mask_bits, 32);
    CHECK_INT(net_parseIPAddressMask("2001:db8::4").mask_bits, 128);

    SECTION("a malformed mask is inert, not a wild match");
    /* An out-of-range width used to reach net_cidrToMask()'s IPv4 path and
     * shift by a negative amount, giving an unpredictable mask. */
    CHECK_INT(net_parseIPAddressMask("192.0.2.5/64").type, NA_UNSPECIFIED);
    CHECK_INT(net_parseIPAddressMask("2001:db8::4/200").type, NA_UNSPECIFIED);
    CHECK_INT(net_parseIPAddressMask("999.1.2.3/8").type, NA_UNSPECIFIED);
    n = net_parseIPAddressMask("garbage");
    net_parseIP(&a, "192.0.2.77:1");
    CHECK(!net_contains(&n, &a));

    SECTION("net_cidrToMask is right for every width in both families");
    for (int fam = 0; fam < 2; fam++) {
        netadrtype_t t = fam ? NA_IP6 : NA_IP;
        int width = fam ? 128 : 32;
        int bad = 0;

        /* Build the expected mask independently, and go past both ends to
         * cover the clamping. */
        for (int cidr = -5; cidr <= width + 40; cidr++) {
            uint8_t want[16] = {0};
            int eff = cidr < 0 ? 0 : (cidr > width ? width : cidr);
            netadr_t m;

            for (int bit = 0; bit < eff; bit++) {
                want[bit / 8] |= (uint8_t)(0x80 >> (bit % 8));
            }
            m = net_cidrToMask(cidr, t);
            if (memcmp(m.ip.u8, want, fam ? 16 : 4) != 0) {
                bad++;
            }
        }
        CHECK_INT(bad, 0);
    }

    SECTION("net_addressToString's widest output is not truncated");
    /* 8 groups of 4 hex digits is the longest inet_ntop emits; with
     * brackets, mask and port that needs 52 bytes. */
    net_parseIP(&a, "[1111:2222:3333:4444:5555:6666:7777:8888]:65535");
    CHECK_STR(net_addressToString(&a, false, false, false),
              "1111:2222:3333:4444:5555:6666:7777:8888");
    CHECK_STR(net_addressToString(&a, true, true, true),
              "[1111:2222:3333:4444:5555:6666:7777:8888]/128:65535");

    return test_summary();
}
