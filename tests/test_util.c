/**
 * g_util.c: the text and data handling the config parsers, ban matching and
 * userinfo checks are all built on. No engine state, so these are cheap.
 *
 * moddir is "baseq2" in the stub, which is what processString's "\m" escape
 * expands to here.
 */

#include "g_local.h"
#include "q2a_test.h"
#include "stub_engine.h"

int main(void)
{
    char out[256], a[256], b[256];
    char info[MAX_INFO_STRING];

    stub_engine_init();

    SECTION("getLogicalValue: Yes/Y/true/1 are true, in any casing");
    CHECK(getLogicalValue("Yes"));
    CHECK(getLogicalValue("yes"));
    CHECK(getLogicalValue("Y"));
    CHECK(getLogicalValue("1"));
    CHECK(getLogicalValue("true"));
    CHECK(getLogicalValue("True"));
    CHECK(getLogicalValue("TRUE"));
    CHECK(!getLogicalValue("No"));
    CHECK(!getLogicalValue(""));
    /* Still false, so worth pinning: "true" is accepted but these are not,
     * and anything unrecognised reads as off rather than erroring. */
    CHECK(!getLogicalValue("on"));
    CHECK(!getLogicalValue("t"));
    CHECK(!getLogicalValue("2"));
    CHECK(!getLogicalValue("truex"));

    SECTION("isBlank: spaces only, not tabs");
    CHECK(isBlank(""));
    CHECK(isBlank("   "));
    CHECK(!isBlank("  x"));
    CHECK(!isBlank("\t"));      /* only ' ' is skipped */

    SECTION("Q_trim strips trailing whitespace, leaves leading alone");
    strcpy(a, "  hello  \t\n");
    CHECK_STR(Q_trim(a), "  hello");
    strcpy(a, "nospace");
    CHECK_STR(Q_trim(a), "nospace");
    strcpy(a, "   ");
    CHECK_STR(Q_trim(a), "");
    strcpy(a, "");
    CHECK_STR(Q_trim(a), "");
    CHECK(Q_trim(NULL) == NULL);

    SECTION("wordCount");
    CHECK_INT(wordCount(""), 0);
    CHECK_INT(wordCount("   "), 0);
    CHECK_INT(wordCount("one"), 1);
    CHECK_INT(wordCount("one two three"), 3);
    CHECK_INT(wordCount("  padded   words  "), 2);
    CHECK_INT(wordCount("tabs\tand\nnewlines"), 3);

    SECTION("case helpers");
    strcpy(a, "MiXeD 123!");
    lowerCase(a);
    CHECK_STR(a, "mixed 123!");
    upperCase(a);
    CHECK_STR(a, "MIXED 123!");

    SECTION("Q_stricmp / Q_strncasecmp / Q_strcasestr");
    CHECK_INT(Q_stricmp("abc", "ABC"), 0);
    CHECK(Q_stricmp("abc", "abd") != 0);
    CHECK_INT(Q_strncasecmp("abcdef", "ABCxxx", 3), 0);
    CHECK(Q_strncasecmp("abcdef", "ABCxxx", 4) != 0);
    CHECK_STR(Q_strcasestr("Hello World", "WORLD"), "World");
    CHECK(Q_strcasestr("Hello", "zzz") == NULL);

    SECTION("prefix and substring matching");
    /* startContains(src, cmp): does src start with cmp, ignoring case. */
    CHECK(startContains("q2pro r1892", "q2pro"));
    CHECK(startContains("Q2PRO", "q2pro"));
    CHECK(!startContains("q2pro", "q2pro r1892"));   /* src is shorter */
    CHECK(startContains("anything", ""));            /* empty cmp matches */
    /* startswith(needle, haystack) - note the order is the other way round
     * from startContains, and it is case sensitive. */
    CHECK(startswith("abc", "abcdef"));
    CHECK(!startswith("abc", "ABCdef"));
    /* stringContains is case-insensitive, anywhere in the string. */
    CHECK(stringContains("the quick brown fox", "QUICK"));
    CHECK(!stringContains("the quick brown fox", "slow"));

    SECTION("wildcardMatch");
    CHECK(wildcardMatch("*", "anything"));
    CHECK(wildcardMatch("abc", "abc"));
    CHECK(!wildcardMatch("abc", "abd"));
    CHECK(wildcardMatch("a?c", "abc"));
    CHECK(!wildcardMatch("a?c", "ac"));
    CHECK(wildcardMatch("a*", "abcdef"));
    CHECK(wildcardMatch("a*", "a"));                 /* * may match nothing */
    CHECK(wildcardMatch("*fox", "the quick fox"));
    CHECK(wildcardMatch("*quick*", "the quick fox"));
    CHECK(!wildcardMatch("*dog", "the quick fox"));
    CHECK(wildcardMatch("", ""));
    CHECK(!wildcardMatch("", "x"));
    CHECK(wildcardMatch("*", ""));

    SECTION("validatePath rejects escapes and control characters");
    CHECK_INT(validatePath("maps/dm1.cfg"), PATH_VALID);
    CHECK_INT(validatePath("a/b/c"), PATH_VALID);
    CHECK_INT(validatePath("/etc/passwd"), PATH_INVALID);       /* absolute */
    CHECK_INT(validatePath("../../etc/passwd"), PATH_INVALID);  /* traversal */
    CHECK_INT(validatePath("ok/../bad"), PATH_INVALID);
    CHECK_INT(validatePath("bad\nname"), PATH_INVALID);         /* control char */
    CHECK_INT(validatePath(""), PATH_VALID);
    /* PATH_MIXED_CASE is in the enum but never returned. */
    CHECK_INT(validatePath("MiXeD/Case.cfg"), PATH_VALID);

    SECTION("Q_strlcpy / Q_strlcat truncate and always terminate");
    CHECK_INT(Q_strlcpy(out, "hello", sizeof(out)), 5);
    CHECK_STR(out, "hello");
    /* The return is the length it WANTED, so > size means truncated. */
    CHECK_INT(Q_strlcpy(out, "hello", 3), 5);
    CHECK_STR(out, "he");
    strcpy(out, "ab");
    CHECK_INT(Q_strlcat(out, "cd", sizeof(out)), 4);
    CHECK_STR(out, "abcd");
    strcpy(out, "ab");
    Q_strlcat(out, "cdef", 5);
    CHECK_STR(out, "abcd");

    SECTION("Q_concat");
    CHECK_INT(Q_concat(out, sizeof(out), "a", "bb", "ccc", NULL), 6);
    CHECK_STR(out, "abbccc");
    /* Over size: returns the wanted length and writes nothing oversized. */
    CHECK_INT(Q_concat(out, 4, "aa", "bb", "cc", NULL), 6);
    CHECK_INT(Q_concat(out, sizeof(out), NULL), 0);
    CHECK_STR(out, "");

    SECTION("Q_ceil / Q_floor");
    CHECK_INT(Q_ceil(1.0f), 1);
    CHECK_INT(Q_ceil(1.1f), 2);
    CHECK_INT(Q_ceil(0.0f), 0);
    CHECK_INT(Q_floor(1.9f), 1);
    CHECK_INT(Q_floor(2.0f), 2);

    SECTION("decayFactor halves over one half-life");
    CHECK(decayFactor(0.0f, 10.0f) == 1.0f);
    CHECK(decayFactor(10.0f, 10.0f) > 0.49f && decayFactor(10.0f, 10.0f) < 0.51f);
    CHECK(decayFactor(20.0f, 10.0f) > 0.24f && decayFactor(20.0f, 10.0f) < 0.26f);
    /* A zero or negative half-life means no decay, not a divide by zero. */
    CHECK(decayFactor(5.0f, 0.0f) == 1.0f);
    CHECK(decayFactor(5.0f, -1.0f) == 1.0f);

    SECTION("processString expands the config escapes");
    processString(out, "plain text", sizeof(out), 0);
    CHECK_STR(out, "plain text");
    processString(out, "line\\nbreak", sizeof(out), 0);
    CHECK_STR(out, "line\nbreak");
    processString(out, "a\\sb", sizeof(out), 0);
    CHECK_STR(out, "a b");
    processString(out, "say \\qhi\\q", sizeof(out), 0);
    CHECK_STR(out, "say \"hi\"");
    processString(out, "cost \\d5", sizeof(out), 0);
    CHECK_STR(out, "cost $5");
    processString(out, "in \\m/here", sizeof(out), 0);
    CHECK_STR(out, "in baseq2/here");
    /* Stops at the end character and reports where it stopped. */
    CHECK(*processString(out, "value\" trailing", sizeof(out), '\"') == '\"');
    CHECK_STR(out, "value");

    SECTION("breakLine splits name \"value\" config lines");
    CHECK(breakLine("option \"the value\"", a, b, sizeof(b)));
    CHECK_STR(a, "option");
    CHECK_STR(b, "the value");
    CHECK(breakLine("  padded   \"v\"  ", a, b, sizeof(b)));
    CHECK_STR(a, "padded");
    CHECK_STR(b, "v");
    CHECK(!breakLine("option novalue", a, b, sizeof(b)));  /* no quotes */
    CHECK(!breakLine("option", a, b, sizeof(b)));          /* no value */
    CHECK(!breakLine("", a, b, sizeof(b)));
    CHECK(!breakLine("option \"\"", a, b, sizeof(b)));     /* empty value */
    CHECK(!breakLine("option \"unterminated", a, b, sizeof(b)));

    SECTION("Info_ValueForKey reads userinfo");
    strcpy(info, "\\name\\bob\\rate\\25000\\skin\\male/grunt");
    CHECK_STR(Info_ValueForKey(info, "name"), "bob");
    CHECK_STR(Info_ValueForKey(info, "rate"), "25000");
    CHECK_STR(Info_ValueForKey(info, "skin"), "male/grunt");
    CHECK_STR(Info_ValueForKey(info, "absent"), "");
    /* Two calls alternate between buffers, so they can be compared. */
    CHECK(Info_ValueForKey(info, "name") != Info_ValueForKey(info, "rate"));

    SECTION("Info_SetValueForKey / Info_RemoveKey");
    strcpy(info, "\\name\\bob");
    Info_SetValueForKey(info, "rate", "8000");
    CHECK_STR(Info_ValueForKey(info, "rate"), "8000");
    Info_SetValueForKey(info, "name", "alice");
    CHECK_STR(Info_ValueForKey(info, "name"), "alice");
    Info_RemoveKey(info, "rate");
    CHECK_STR(Info_ValueForKey(info, "rate"), "");
    CHECK_STR(Info_ValueForKey(info, "name"), "alice");
    /* Refuses the characters that would let a value forge a new key. */
    strcpy(info, "\\name\\bob");
    Info_SetValueForKey(info, "evil", "a\\b");
    CHECK_STR(Info_ValueForKey(info, "evil"), "");
    Info_SetValueForKey(info, "evil", "a\"b");
    CHECK_STR(Info_ValueForKey(info, "evil"), "");
    Info_SetValueForKey(info, "evil", "a;b");
    CHECK_STR(Info_ValueForKey(info, "evil"), "");
    /* An empty key or value is a no-op. */
    Info_SetValueForKey(info, "k", "");
    CHECK_STR(Info_ValueForKey(info, "k"), "");

    SECTION("Info_Validate rejects malformed userinfo");
    CHECK(Info_Validate("\\name\\bob\\rate\\8000"));
    CHECK(!Info_Validate("\\name\\bob\\dangling"));   /* key with no value */
    CHECK(!Info_Validate("\\name\\has\"quote"));
    CHECK(!Info_Validate("\\name\\has;semi"));

    SECTION("randomString fills the length asked for");
    memset(out, 'x', sizeof(out));
    randomString(out, 10);
    CHECK_INT((int)strlen(out), 10);

    SECTION("va rotates 8 buffers, so a 9th call overwrites the 1st");
    {
        char *first = va("%d", 1);
        char *second = va("%d", 2);
        CHECK_STR(first, "1");
        CHECK_STR(second, "2");
        for (int i = 3; i <= 9; i++) {
            va("%d", i);
        }
        /* first has now been recycled - this is why results that must
         * outlive the next few calls have to be copied. */
        CHECK(strcmp(first, "1") != 0);
    }

    return test_summary();
}
