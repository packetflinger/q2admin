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
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#define test_isatty(fd) _isatty(fd)
#else
#include <unistd.h>
#define test_isatty(fd) isatty(fd)
#endif

static int test_fails;
static int test_checks;

/**
 * ANSI colors for results: dark green for passes, dark red for failures.
 * Only used when stdout is a terminal and NO_COLOR isn't set, so output
 * redirected to a file stays plain.
 */
#define TEST_GREEN "\033[32m"
#define TEST_RED   "\033[31m"
#define TEST_RESET "\033[0m"

static int test_color(void) {
    static int color = -1;
    if (color < 0) {
        color = test_isatty(1) && !getenv("NO_COLOR");
    }
    return color;
}

#define TEST_C(code) (test_color() ? (code) : "")

#define SECTION(name) printf("\n%s\n", (name))

#define CHECK_AT(file, line, ok, fmt, ...)                                  \
    do {                                                                    \
        test_checks++;                                                      \
        if (ok) {                                                           \
            printf("  %sok%s   " fmt "\n", TEST_C(TEST_GREEN),              \
                   TEST_C(TEST_RESET), __VA_ARGS__);                        \
        } else {                                                            \
            test_fails++;                                                   \
            printf("  %sFAIL " fmt "   (%s:%d)%s\n", TEST_C(TEST_RED),      \
                   __VA_ARGS__, file, line, TEST_C(TEST_RESET));            \
        }                                                                   \
    } while (0)

/**
 * Truthy check.
 */
#define CHECK(expr) \
    CHECK_AT(__FILE__, __LINE__, (expr), "%s", #expr)

/**
 * Integer compare; prints both values on failure.
 */
#define CHECK_INT(expr, want)                                               \
    do {                                                                    \
        long _g = (long)(expr), _w = (long)(want);                          \
        CHECK_AT(__FILE__, __LINE__, _g == _w, "%s = %ld (want %ld)",       \
                 #expr, _g, _w);                                            \
    } while (0)

/**
 * String compare; NULL is never equal, so a NULL return fails rather than
 * crashing inside strcmp.
 */
#define CHECK_STR(expr, want)                                               \
    do {                                                                    \
        const char *_g = (expr), *_w = (want);                              \
        CHECK_AT(__FILE__, __LINE__, _g && _w && strcmp(_g, _w) == 0,       \
                 "%s = \"%s\" (want \"%s\")", #expr,                        \
                 _g ? _g : "(null)", _w ? _w : "(null)");                   \
    } while (0)

static int test_summary(void) {
    printf("\n%s%s: %d checks, %d failed%s\n",
           TEST_C(test_fails ? TEST_RED : TEST_GREEN),
           test_fails ? "FAILED" : "passed", test_checks, test_fails,
           TEST_C(TEST_RESET));
    return test_fails != 0;
}

#endif /* Q2A_TEST_H */
