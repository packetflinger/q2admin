/**
 * g_protocol.c: deciding whether a client connected with less protocol than
 * its engine can speak.
 *
 * This is the pattern for testing anything that works on a player. A client
 * is three fields in a proxyinfo slot - no engine, no network, no map:
 *
 *     stub_add_client(1, "bob", "q2pro r1892", PROTOCOL_R1Q2);
 *     checkProtocolDowngrade(1);
 *     CHECK(stub_signals_for(1) & SIGNAL_PROTOCOL_DOWNGRADE);
 *
 * server_protocol stands for what the server itself offers, so a test can
 * put a modern client on an older server and check nothing is flagged.
 */

#include "g_local.h"
#include "q2a_test.h"
#include "stub_engine.h"

/* Set up one client, run the check, report whether the signal was raised. */
static int downgraded(int server_offers, const char *version, int negotiated)
{
    server_protocol = server_offers;
    stub_add_client(1, "tester", version, negotiated);
    checkProtocolDowngrade(1);
    return (stub_signals_for(1) & SIGNAL_PROTOCOL_DOWNGRADE) != 0;
}

int main(void)
{
    const client_def_t *d;

    stub_engine_init();

    SECTION("identifyClient recognises real version strings");
    d = identifyClient("q2pro r1892~e0f8a1c x86_64 Dec  1 2023 Linux");
    CHECK_STR(d ? d->name : NULL, "Q2PRO");
    CHECK_INT(d->max_protocol, PROTOCOL_Q2PRO);
    CHECK_STR(identifyClient("R1Q2 b7900 x86 Linux")->name, "R1Q2");
    CHECK_STR(identifyClient("Q2RTX v1.7.0")->name, "Q2RTX");
    CHECK_STR(identifyClient("Yamagi Quake II v8.20")->name, "Yamagi");
    CHECK_STR(identifyClient("yquake2 8.00 x86_64 Linux")->name, "Yamagi");
    CHECK_STR(identifyClient("KMQuake2 v0.20 update 8")->name, "KMQuake2");
    CHECK_STR(identifyClient("3.21 x86 Dec 31 1997 Win32")->name, "Quake II");

    SECTION("identifyClient does not guess");
    CHECK(identifyClient("") == NULL);
    CHECK(identifyClient(NULL) == NULL);
    CHECK(identifyClient("totally made up 1.0") == NULL);
    /* The stock-Quake-II pattern is anchored, so a version number in the
     * middle of some other client's string must not match it. */
    CHECK(identifyClient("SomeClient build 3.21") == NULL);

    SECTION("on a Q2PRO server (offers 36)");
    CHECK(!downgraded(PROTOCOL_Q2PRO, "q2pro r1892", 36));   /* at its ceiling */
    CHECK(downgraded(PROTOCOL_Q2PRO, "q2pro r1892", 35));
    CHECK(downgraded(PROTOCOL_Q2PRO, "q2pro r1892", 34));
    CHECK(!downgraded(PROTOCOL_Q2PRO, "R1Q2 b7900", 35));    /* r1q2's ceiling */
    CHECK(downgraded(PROTOCOL_Q2PRO, "R1Q2 b7900", 34));
    CHECK(!downgraded(PROTOCOL_Q2PRO, "Yamagi Quake II v8.20", 34));

    SECTION("on an R1Q2 server (offers 35) nothing better is available");
    CHECK(!downgraded(PROTOCOL_R1Q2, "q2pro r1892", 35));
    CHECK(downgraded(PROTOCOL_R1Q2, "q2pro r1892", 34));

    SECTION("on a server capped at 34, everyone is at 34 by definition");
    CHECK(!downgraded(PROTOCOL_VANILLA, "q2pro r1892", 34));
    CHECK(!downgraded(PROTOCOL_VANILLA, "R1Q2 b7900", 34));

    SECTION("quiet when there is nothing to compare");
    /* protocol_major only arrives with the extra userinfo block, so 0 means
     * the server never told us what the client connected with. */
    CHECK(!downgraded(PROTOCOL_Q2PRO, "q2pro r1892", 0));
    CHECK(!downgraded(PROTOCOL_Q2PRO, "q2pro r1892", -1));
    CHECK(!downgraded(PROTOCOL_Q2PRO, "unrecognised client 1.0", 34));
    CHECK(!downgraded(PROTOCOL_Q2PRO, "", 34));
    /* Above its ceiling is not a downgrade. */
    CHECK(!downgraded(PROTOCOL_Q2PRO, "q2pro r1892", 37));

    SECTION("an invalid client index is ignored, not indexed");
    server_protocol = PROTOCOL_Q2PRO;
    checkProtocolDowngrade(-1);
    checkProtocolDowngrade(9999);
    CHECK(1);   /* reaching here without crashing is the check */

    return test_summary();
}
