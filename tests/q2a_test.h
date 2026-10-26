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

#ifndef Q2A_TEST_H
#define Q2A_TEST_H

#include <stdio.h>
#include <string.h>

static int test_fails;
static int test_checks;

#define SECTION(name) printf("\n%s\n", (name))

#define CHECK_AT(file, line, ok, fmt, ...)                                  \
    do {                                                                    \
        test_checks++;                                                      \
        if (ok) {                                                           \
            printf("  ok   " fmt "\n", __VA_ARGS__);                        \
        } else {                                                            \
            test_fails++;                                                   \
            printf("  FAIL " fmt "   (%s:%d)\n", __VA_ARGS__, file, line);  \
        }                                                                   \
    } while (0)

/* Truthy check. */
#define CHECK(expr) \
    CHECK_AT(__FILE__, __LINE__, (expr), "%s", #expr)

/* Integer compare; prints both values on failure. */
#define CHECK_INT(expr, want)                                               \
    do {                                                                    \
        long _g = (long)(expr), _w = (long)(want);                          \
        CHECK_AT(__FILE__, __LINE__, _g == _w, "%s = %ld (want %ld)",       \
                 #expr, _g, _w);                                            \
    } while (0)

/* String compare; NULL is never equal, so a NULL return fails rather than
 * crashing inside strcmp. */
#define CHECK_STR(expr, want)                                               \
    do {                                                                    \
        const char *_g = (expr), *_w = (want);                              \
        CHECK_AT(__FILE__, __LINE__, _g && _w && strcmp(_g, _w) == 0,       \
                 "%s = \"%s\" (want \"%s\")", #expr,                        \
                 _g ? _g : "(null)", _w ? _w : "(null)");                   \
    } while (0)

static int test_summary(void)
{
    printf("\n%s: %d checks, %d failed\n",
           test_fails ? "FAILED" : "passed", test_checks, test_fails);
    return test_fails != 0;
}

#endif /* Q2A_TEST_H */
