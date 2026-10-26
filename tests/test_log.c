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

static char line[4096];

/* Expand a format for client 1 and return the result. */
static const char *expand(const char *fmt, char *message, int number,
                          float ret_time)
{
    memset(line, 0, sizeof(line));
    convertToLogLine(line, (char *)fmt, 1, stub_ent(1), message, number,
                     ret_time);
    return line;
}

/* Same, but with no client - ent NULL, as server-wide events pass. */
static const char *expand_noent(const char *fmt, char *message, int number,
                                float ret_time)
{
    memset(line, 0, sizeof(line));
    convertToLogLine(line, (char *)fmt, 0, NULL, message, number, ret_time);
    return line;
}

int main(void)
{
    proxyinfo_t *cl;

    stub_engine_init();
    cl = stub_add_client(1, "testplayer", "q2pro r1892", PROTOCOL_Q2PRO);
    cl->userinfo.rate = 25000;
    snprintf(cl->userinfo.skin, sizeof(cl->userinfo.skin), "male/grunt");
    cl->ent->client->ping = 42;
    net_parseIP(&cl->address, "192.0.2.77:27901");

    SECTION("text with no macros is copied through");
    CHECK_STR(expand("plain text", NULL, 0, 0.0f), "plain text");
    CHECK_STR(expand("", NULL, 0, 0.0f), "");

    SECTION("client macros");
    CHECK_STR(expand("#n", NULL, 0, 0.0f), "testplayer");
    CHECK_STR(expand("#i", NULL, 0, 0.0f), "192.0.2.77");
    CHECK_STR(expand("#r", NULL, 0, 0.0f), "25000");
    CHECK_STR(expand("#s", NULL, 0, 0.0f), "male/grunt");
    CHECK_STR(expand("#p", NULL, 0, 0.0f), "42");
    CHECK_STR(expand("[#n] rate=#r", NULL, 0, 0.0f), "[testplayer] rate=25000");

    SECTION("client macros expand to nothing when there is no client");
    CHECK_STR(expand_noent("#n", NULL, 0, 0.0f), "");
    CHECK_STR(expand_noent("#i", NULL, 0, 0.0f), "");
    CHECK_STR(expand_noent("#r", NULL, 0, 0.0f), "");
    CHECK_STR(expand_noent("#s", NULL, 0, 0.0f), "");
    CHECK_STR(expand_noent("#p", NULL, 0, 0.0f), "");
    CHECK_STR(expand_noent("a#nb", NULL, 0, 0.0f), "ab");

    SECTION("#m is the message, with newlines dropped");
    CHECK_STR(expand("#m", "hello", 0, 0.0f), "hello");
    CHECK_STR(expand("#m", "two\nlines", 0, 0.0f), "twolines");
    CHECK_STR(expand("#m", "", 0, 0.0f), "");
    CHECK_STR(expand("#m", NULL, 0, 0.0f), "");
    /* The message is data, never a format - a player name reaches this. */
    CHECK_STR(expand("#m", "100%s of it", 0, 0.0f), "100%s of it");

    SECTION("#e is the number");
    CHECK_STR(expand("#e", NULL, 0, 0.0f), "0");
    CHECK_STR(expand("#e", NULL, 175, 0.0f), "175");
    CHECK_STR(expand("#e", NULL, -8, 0.0f), "-8");
    /* Detection reason codes are negative and must not be truncated. */
    CHECK_STR(expand("#e", NULL, -1234567, 0.0f), "-1234567");

    SECTION("#f keeps six decimal places (the truncation bug)");
    CHECK_STR(expand("#f", NULL, 0, 0.00041f), "0.000410");
    /* This is the case that used to come out as "0.00000": eight characters
     * would not fit when the bound was sizeof(dest). */
    CHECK_STR(expand("#f", NULL, 0, 0.0000041f), "0.000004");
    CHECK_STR(expand("#f", NULL, 0, 0.0f), "0.000000");
    CHECK_STR(expand("#f", NULL, 0, 12.5f), "12.500000");

    SECTION("#x and #X are the signal score");
    stub_signal_score = 35;
    CHECK_STR(expand("#x", NULL, 0, 0.0f), "35");
    CHECK_STR(expand("#X", NULL, 0, 0.0f), "35/50");
    /* Over 7 characters, which the old bound cut. */
    stub_signal_score = 1000000;
    CHECK_STR(expand("#X", NULL, 0, 0.0f), "1000000/50");
    /* Guarded on ent like the other client values. */
    CHECK_STR(expand_noent("#x", NULL, 0, 0.0f), "");
    CHECK_STR(expand_noent("#X", NULL, 0, 0.0f), "");
    stub_signal_score = 0;

    SECTION("#w is the signal's name, looked up from the number");
    CHECK_STR(expand("#w", NULL, SIGNAL_CHATFLOOD, 0.0f), "chatflood");
    /* A number that is not a signal expands to nothing. */
    CHECK_STR(expand("#w", NULL, SIGNAL_SNAP_FIRE, 0.0f), "");
    CHECK_STR(expand("#w", NULL, 0, 0.0f), "");

    SECTION("#t and #T are timestamps");
    CHECK((int)strlen(expand("#T", NULL, 0, 0.0f)) == 14);   /* YYYYMMDDhhmmss */
    CHECK((int)strlen(expand("#t", NULL, 0, 0.0f)) > 0);

    SECTION("an unknown macro is left visible rather than eaten");
    CHECK_STR(expand("#z", NULL, 0, 0.0f), "#z");
    CHECK_STR(expand("a#zb", NULL, 0, 0.0f), "a#zb");
    /* A trailing # with nothing after it must not read past the string. */
    CHECK_STR(expand("end#", NULL, 0, 0.0f), "end#");

    SECTION("a realistic format line");
    CHECK_STR(expand("#n[#i] impulse #e", NULL, 175, 0.0f),
              "testplayer[192.0.2.77] impulse 175");

    return test_summary();
}
