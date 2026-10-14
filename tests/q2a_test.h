/**
 * Q2Admin test harness.
 *
 * Deliberately not a framework: a few macros that count failures and print
 * what went wrong. Each test file is its own program, so the counters are
 * static here and live in whichever file includes this.
 *
 * Write a test like:
 *
 *     #include "q2a_test.h"
 *     int main(void) {
 *         SECTION("net_parseIP");
 *         CHECK(net_parseIP(&a, "192.0.2.4:1234"));
 *         CHECK_INT(a.port, 1234);
 *         return test_summary();
 *     }
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
