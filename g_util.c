/*
Copyright (C) 2000 Shane Powell

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

 */

#include "g_local.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * Makes a player's own console run a command, by writing a raw
 * SVC_STUFFTEXT message straight into their network stream.
 *
 * This is the mechanism nearly all of q2admin's client probing is built
 * on: there's no way to ask a client what its cvars or aliases are, so
 * instead it's told to run something that reports back (echo a cvar,
 * define an alias, reconnect), and the reply is watched for. It's also
 * how settings get enforced client-side, e.g. clamping cl_maxfps.
 *
 * e: the player to send to.
 * s: the console command text. Generally needs a trailing '\n' - without
 *    one it only lands in their console input line rather than running.
 */
void stuffPlayer(edict_t *e, char *s) {
    if (q2a_developer) {
        q2a_printf("STUFF(%s): %s\n", NAME(getEntOffset(e)-1), s);
    }
    gi.WriteByte(SVC_STUFFTEXT);
    gi.WriteString(s);
    gi.unicast(e, true);
}

/**
 * Strips trailing whitespace (space/tab/newline) from a string in place,
 * mainly for cleaning up lines read from config files where a stray
 * trailing space or the line's '\n' would otherwise end up inside a
 * parsed value.
 *
 * Despite what this comment used to claim, only the *end* is trimmed -
 * leading whitespace is left alone and the returned pointer is always
 * the one passed in. Callers wanting to skip leading blanks use the
 * SKIPBLANK macro instead.
 *
 * s: string to trim in place; NULL and empty are handled.
 *
 * Returns s.
 */
char *Q_trim(char *s) {
    char *ptr;
    if (!s) {
        return NULL;   // handle NULL string
    }
    if (!*s) {
        return s;      // handle empty string
    }
    for (ptr = s + q2a_strlen(s) - 1; (ptr >= s) && isspace(*ptr); --ptr);
    ptr[1] = '\0';
    return s;
}

/**
 * Formats a string printf style and returns it, so a formatted string
 * can be built inline as an argument to something else rather than
 * needing a caller-declared buffer for every one-off message. Used
 * heavily for log lines, kick reasons and console output.
 *
 * Results live in a rotating set of 8 static buffers. That rotation is
 * what makes it safe to use more than one va() in a single expression -
 * the second call doesn't stomp the first. Two consequences: the 9th
 * live result overwrites the 1st, so don't hold onto a returned pointer
 * across other va() calls or store it long term (copy it instead), and
 * output longer than MAX_STRING_CHARS is truncated.
 *
 * format: printf style format string, followed by its arguments.
 *
 * Returns a pointer to one of the static buffers.
 */
char *va(const char *format, ...) {
    static char strings[8][MAX_STRING_CHARS];
    static uint16_t index;

    char *string = strings[index++ % 8];
    va_list args;
    va_start(args, format);
    vsnprintf(string, MAX_STRING_CHARS, format, args);
    va_end(args);
    return string;
}

/**
 * Matches a string against a glob style pattern, so admins can write
 * things like "clan*" in a ban or name rule instead of needing a full
 * regex for simple cases:
 *   * - matches any run of characters, including none
 *   ? - matches exactly one character
 *
 * Recurses to backtrack: on '*' it tries both consuming a haystack
 * character and moving past the '*'. Note this is case *sensitive*,
 * unlike startContains()/stringContains(), and a pattern with several
 * '*'s against a long string can get expensive, so it's not for
 * hot paths.
 *
 * pattern:  the glob pattern.
 * haystack: the string to test against it.
 *
 * Returns true only on a whole-string match, not a partial one.
 */
bool wildcardMatch(char *pattern, char *haystack) {
    if (*pattern == '\0' && *haystack == '\0') {
        return true;
    }
    if (*pattern == '*' && *(pattern+1) != '\0' && *haystack == '\0') {
        return false;
    }
    if (*pattern == '?' || *pattern == *haystack) {
        return wildcardMatch(pattern+1, haystack+1);
    }
    if (*pattern == '*') {
        return wildcardMatch(pattern+1, haystack) || wildcardMatch(pattern, haystack+1);
    }
    return false;
}

/**
 * Whether haystack begins with needle, comparing case *sensitively* -
 * which is the one thing that distinguishes it from startContains()
 * below, which does the same test while ignoring case. Use this when the
 * exact casing matters (protocol tokens, literal markers), that one for
 * anything a human typed.
 *
 * needle:   the prefix to look for.
 * haystack: the string that may start with it.
 *
 * Note the argument order is the reverse of startContains(), which takes
 * the string being searched first - easy to get backwards.
 *
 * Returns true if haystack starts with needle.
 */
bool startswith(char *needle, char *haystack) {
    return (strncmp(needle, haystack, q2a_strlen(needle)) == 0);
}

/**
 * Case insensitive string comparison, for the many places q2admin has to
 * match something a human typed (command names, config keywords, player
 * supplied values) where casing shouldn't matter.
 *
 * string1, string2: the strings to compare.
 *
 * Returns 0 if equal ignoring case, -1 if string1 sorts first, 1 if
 * string2 does - so `Q_stricmp(a, b) == 0` is the "are these the same"
 * test, and it's easy to misread as a boolean.
 *
 * Takes non-const char* (unlike Q_strncasecmp() below), so comparing
 * against a const string needs a cast at the call site.
 */
int Q_stricmp(char *string1, char *string2) {
    while (*string1 && *string2) {
        char s1c = tolower(*string1);
        char s2c = tolower(*string2);
        if (s1c != s2c) {
            if (s1c < s2c) {
                return -1;
            } else {
                return 1;
            }
        }
        string1++;
        string2++;
    }
    if (*string2) {
        return -1;
    }
    if (*string1) {
        return 1;
    }
    return 0;
}

/**
 * Pulls one value out of a userinfo string - the "\key\value\key\value"
 * format Quake 2 packs a client's settings into (name, skin, rate,
 * cl_maxfps, ip and so on). Reading those is how q2admin learns almost
 * everything about a connecting client, so this is one of the most used
 * functions here.
 *
 * Results alternate between two static buffers, which is what lets two
 * calls be compared against each other in one expression without the
 * second overwriting the first. A third live result wraps around, so
 * copy anything that needs to outlast the next couple of calls.
 *
 * s:   the userinfo string to search.
 * key: the key to look up, matched case sensitively.
 *
 * Returns the value, or an empty string if the key isn't present. Never
 * NULL, so the result is always safe to pass straight to string
 * functions.
 *
 * Assumes a well-formed info string no longer than MAX_INFO_STRING - the
 * key/value scratch buffers are sized for that and the parse loop has no
 * length checks of its own, so run untrusted input through
 * Info_Validate() first.
 */
char *Info_ValueForKey(char *s, char *key) {
    char pkey[512];
    static char value[2][512];  // Use two buffers so compares work without
                                // stomping on each other
    static int valueindex;
    char *o;

    valueindex ^= 1;
    if (*s == '\\') {
        s++;
    }
    while (1) {
        o = pkey;
        while (*s != '\\') {
            if (!*s) {
                return "";
            }
            *o++ = *s++;
        }
        *o = 0;
        s++;

        o = value[valueindex];

        while (*s != '\\' && *s) {
            if (!*s) {
                return "";
            }
            *o++ = *s++;
        }
        *o = 0;

        if (!q2a_strcmp(key, pkey)) {
            return value[valueindex];
        }

        if (!*s) {
            return "";
        }
        s++;
    }
}

/**
 * Checks that a userinfo string is structurally sound before anything
 * tries to parse it: alternating backslash separated key/value pairs,
 * nothing over MAX_INFO_KEY / MAX_INFO_VALUE / MAX_INFO_STRING, and only
 * printable characters.
 *
 * '"' and ';' are rejected outright wherever they appear. That's the
 * security-relevant part: userinfo values get interpolated into console
 * commands and config output elsewhere, where a quote or semicolon would
 * let a client break out of the intended command and append its own.
 *
 * This validates shape only - whether the individual values are sensible
 * is each caller's problem.
 *
 * s: the userinfo string to check.
 *
 * Returns true if the string is well formed.
 */
bool Info_Validate(char *s) {
    size_t len, total;
    int c;

    total = 0;
    while (true) {
        // validate key
        if (*s == '\\') {
            s++;
            if (++total == MAX_INFO_STRING) {
                return false;   // oversize infostring
            }
        }
        if (!*s) {
            return false;   // missing key
        }
        len = 0;
        while (*s != '\\') {
            c = *s++;
            if (!Q_isprint(c) || c == '\"' || c == ';') {
                return false;   // illegal characters
            }
            if (++len == MAX_INFO_KEY) {
                return false;   // oversize key
            }
            if (++total == MAX_INFO_STRING) {
                return false;   // oversize infostring
            }
            if (!*s) {
                return false;   // missing value
            }
        }

        // validate value
        s++;
        if (++total == MAX_INFO_STRING) {
            return false;   // oversize infostring
        }
        if (!*s) {
            return false;   // missing value
        }
        len = 0;
        while (*s != '\\') {
            c = *s++;
            if (!Q_isprint(c) || c == '\"' || c == ';') {
                return false;   // illegal characters
            }
            if (++len == MAX_INFO_VALUE) {
                return false;   // oversize value
            }
            if (++total == MAX_INFO_STRING) {
                return false;   // oversize infostring
            }
            if (!*s) {
                return true;    // end of string
            }
        }
    }
    return false;
}

/**
 * Deletes a key and its value from a userinfo string, shifting the rest
 * of the string down over the gap so the result stays a valid info
 * string. Used both to strip keys a client shouldn't be setting and, by
 * Info_SetValueForKey() below, as the first half of replacing a value.
 *
 * s:   the userinfo string, modified in place.
 * key: the key to remove; a key containing a backslash is refused, since
 *      that can't be a real key and would corrupt the string.
 *
 * Does nothing if the key isn't there.
 */
void Info_RemoveKey(char *s, const char *key) {
    char *start;
    char pkey[512];
    char value[512];
    char *o;

    if (strchr(key, '\\')) {
        return;
    }
    for (;;) {
        start = s;
        if (*s == '\\') {
            s++;
        }
        o = pkey;
        while (*s != '\\') {
            if (!*s) {
                return;
            }
            *o++ = *s++;
        }
        *o = 0;
        s++;
        o = value;
        while (*s != '\\' && *s) {
            if (!*s) {
                return;
            }
            *o++ = *s++;
        }
        *o = 0;
        if (!q2a_strcmp(key, pkey)) {
            size_t memlen = q2a_strlen(s);
            q2a_memmove(start, s, memlen);
            start[memlen] = 0;
            return;
        }
        if (!*s) {
            return;
        }
    }
}

/**
 * Sets a key in a userinfo string, by removing any existing copy and
 * appending the pair at the end - so the value is replaced rather than
 * duplicated, and ordering isn't preserved. This is how q2admin rewrites
 * what the real game mod will see, e.g. replacing an oversized skin or
 * stuffing in a rejection message.
 *
 * Refuses anything that would corrupt the string or let a value escape
 * into a console command: empty key or value, a backslash/semicolon/
 * double quote in either, a key or value at the length limit, or a
 * result that wouldn't fit in MAX_INFO_STRING. Characters outside
 * printable ASCII are dropped as it appends.
 *
 * s:     the userinfo string, modified in place. Must have room for
 *        MAX_INFO_STRING.
 * key:   key to set.
 * value: value to set it to.
 *
 * Silently does nothing if any of those checks fail - there's no way to
 * tell success from refusal, so callers that care have to re-read the
 * key afterwards. Note the length check on the value compares against
 * MAX_INFO_KEY rather than MAX_INFO_VALUE; harmless while the two are
 * equal, but wrong if they ever diverge.
 */
void Info_SetValueForKey(char *s, const char *key, const char *value) {
    char newi[MAX_INFO_STRING], *v;
    int c;

    if (!key || !key[0] || !value || !value[0]) {
        return;
    }
    char *nope = "\\;\""; // no slashes, semicolons or double quotes
    while (*nope) {
        if (strchr(key, *nope) || strchr(value, *nope++)) {
            return;
        }
    }
    if (q2a_strlen(key) > MAX_INFO_KEY - 1 || q2a_strlen(value) > MAX_INFO_KEY - 1) {
        return;
    }
    Info_RemoveKey(s, key);
    sprintf(newi, "\\%s\\%s", key, value);
    if ((q2a_strlen(newi) + q2a_strlen(s)) > MAX_INFO_STRING) {
        return;
    }
    s += q2a_strlen(s);
    v = newi;
    while (*v) {
        c = *v++;
        c &= 127;                   // strip high bits
        if (c >= 32 && c < 127) {   // only ascii values
            *s++ = c;
        }
    }
    s[0] = 0;
}

/**
 * Re-syncs q2admin's own game_export_t with the real game mod's, and is
 * the heart of how the MITM setup stays consistent.
 *
 * The server was handed q2admin's `ge` struct and reads the entity array
 * straight out of it, but the actual entities belong to the wrapped mod,
 * whose `edicts` pointer and counts move as it spawns and frees things.
 * Any forwarded call can change them, so this is called immediately
 * after each one - without it the server would keep reading a stale
 * pointer or an out of date num_edicts and see the wrong entities.
 *
 * Takes no parameters; copies the array pointer, counts and sizes from
 * ge_mod into ge.
 */
void G_MergeEdicts(void) {
    ge.apiversion = ge_mod->apiversion;
    ge.edict_size = ge_mod->edict_size;
    ge.edicts = ge_mod->edicts;
    ge.num_edicts = ge_mod->num_edicts;
    ge.max_edicts = ge_mod->max_edicts;
}

/**
 * Break up a configuration file line into the variable and value. For example:
 *
 * Input line:  `variable1 "value1"`
 *   `buffer` is the entire input line
 *   `buff1` will be "variable1" upon return
 *   `buff2` will be "value1" upon return (the original quotes are stripped)
 *
 * Return value
 *   1 for successfully parsing the line
 *   0 for being unsuccessful (formatting issues)
 *
 * Note: There can be any number of spaces or tabs at the beginning of the line
 * or (and only) between the variable and the value. The value MUST be quoted.
 *
 * buffer:    the whole input line (not modified).
 * buff1:     receives the variable name. Sized by the caller, and not
 *            length checked here, so it needs to be able to hold the
 *            longest name a line could contain.
 * buff2:     receives the value with the quotes stripped.
 * buff2size: capacity of buff2, which is honoured.
 *
 * The value is run through processString(), so escapes like \n and \m
 * are expanded as part of parsing.
 */
int breakLine(char *buffer, char *buff1, char *buff2, int buff2size) {
    char *cp, *dp;

    cp = buffer;
    dp = buff1;
    SKIPBLANK(cp);
    while (*cp && *cp != ' ' && *cp != '\t') {
        *dp++ = *cp++;
    }
    *dp = 0x0;
    if (dp == buff1 || !*cp) {
        return 0;
    }
    dp = buff2;
    SKIPBLANK(cp);
    if (*cp != '\"') {
        return 0;
    }
    cp++;
    cp = processString(buff2, cp, buff2size, '\"');
    if (!buff2[0] || *cp != '\"') {
        return 0;
    }
    return 1;
}

/**
 * Whether src begins with cmp, ignoring case. This is the workhorse for
 * recognising keywords at the front of a line or command - config file
 * prefixes like "BAN:"/"SW:", argument keywords like "LIKE"/"RE" - where
 * what follows the keyword still needs parsing, so a full compare won't
 * do.
 *
 * src: the string being examined.
 * cmp: the prefix to look for.
 *
 * Returns true if src starts with cmp; an empty cmp always matches. For
 * a case sensitive version, and the opposite argument order, see
 * startswith() above.
 */
bool startContains(char *src, char *cmp) {
    while (*cmp) {
        if (!(*src) || toupper(*src) != toupper(*cmp)) {
            return 0;
        }
        src++;
        cmp++;
    }
    return 1;
}

/**
 * Whether buff2 appears anywhere inside buff1, ignoring case - the
 * substring counterpart to startContains(), for "does this contain that"
 * checks like matching part of a player name or spotting a marker in a
 * command.
 *
 * Works by upper-casing copies of both into local scratch buffers rather
 * than comparing in place, so neither input is modified.
 *
 * buff1: the string to search.
 * buff2: the substring to look for.
 *
 * Returns non-zero if found. Inputs longer than the 4KB scratch buffers
 * are silently truncated before comparing, so it can't be relied on for
 * arbitrarily long strings.
 */
int stringContains(char *buff1, char *buff2) {
    char strbuffer1[4096];
    char strbuffer2[4096];

    q2a_strncpy(strbuffer1, buff1, sizeof(strbuffer1)-1);
    upperCase(strbuffer1);
    q2a_strncpy(strbuffer2, buff2, sizeof(strbuffer2)-1);
    upperCase(strbuffer2);
    return (q2a_strstr(strbuffer1, strbuffer2) != NULL);
}

/**
 * How much of a value should survive after a given amount of time, for
 * exponential decay - multiply an accumulator by this each time it's
 * updated and old contributions fade out smoothly instead of counting
 * forever.
 *
 * Decay is the alternative to a fixed reset window for "recent activity"
 * metrics: there's no boundary for someone to pace themselves across,
 * the value never jumps, and it costs two floats rather than a set of
 * per-period counters.
 *
 * elapsed:  seconds since the value was last decayed.
 * halflife: seconds after which a contribution counts for half as much.
 *
 * Returns the factor to multiply by, between 0 and 1. A non-positive
 * halflife returns 1, i.e. no decay, so a misconfigured value degrades
 * to plain cumulative behaviour rather than wiping the accumulator.
 */
float decayFactor(float elapsed, float halflife) {
    if (halflife <= 0.0f) {
        return 1.0f;
    }
    return expf(-elapsed * 0.69314718f / halflife); // ln(2) / halflife
}

/**
 * Counts the words in a string, a word being any run of non-whitespace.
 * Used to measure how much a player is actually saying, which is a
 * steadier signal than raw character count - one long URL isn't chatty
 * the way a dozen short words are.
 *
 * s: the string to count.
 *
 * Returns the number of words; 0 for an empty or whitespace-only string.
 * Punctuation isn't special, so "hi!!!" counts once and "a - b" counts
 * three times.
 */
int wordCount(const char *s) {
    int words = 0;
    bool inword = false;

    for (; *s; s++) {
        if (isspace((unsigned char) *s)) {
            inword = false;
        } else if (!inword) {
            inword = true;
            words++;
        }
    }
    return words;
}

/**
 * Whether a string is empty or nothing but spaces, used by the config
 * and ban file readers to skip over blank lines.
 *
 * buff1: the string to check.
 *
 * Returns non-zero if there's nothing but spaces. Note it only treats
 * ' ' as blank - a line of tabs, or one that still has its trailing
 * '\n', is *not* considered blank here, which is why callers tend to
 * test for '\n' separately.
 */
int isBlank(char *buff1) {
    while (*buff1 == ' ') {
        buff1++;
    }
    return !(*buff1);
}

/**
 * Copies input to output, expanding backslash escapes as it goes, and
 * stops at a caller-chosen terminator. This is what lets config values
 * and admin messages contain things that either can't be typed inside a
 * quoted config value or aren't known until runtime:
 *
 *  \n = newline
 *  \d = dollar sign $
 *  \q = double quote "
 *  \s = space (despite older comments here saying single quote)
 *  \m = mod directory name
 *  \t = current timestamp
 *
 * (case insensitive). An unrecognised escape emits the character that
 * followed the backslash, so "\\" yields a literal backslash.
 *
 * output: destination buffer, always NUL terminated.
 * input:  source string.
 * max:    space available in output. The \m and \t expansions are
 *         skipped rather than truncated when they wouldn't fit.
 * end:    character to stop at (e.g. '"' when reading a quoted config
 *         value); pass 0 to run to the end of input.
 *
 * Returns a pointer to where it stopped in *input* - at the terminator
 * or the NUL - which is how callers like breakLine() know whether the
 * closing quote was actually present and where to resume parsing.
 */
char *processString(char *output, char *input, int max, char end) {
    while (*input && *input != end && max) {
        if (*input == '\\') {
            input++;

            if ((*input == 'n') || (*input == 'N')) {
                *output++ = '\n';
                input++;
            } else if ((*input == 'd') || (*input == 'D')) {
                *output++ = '$';
                input++;
            } else if ((*input == 'q') || (*input == 'Q')) {
                *output++ = '\"';
                input++;
            } else if ((*input == 's') || (*input == 'S')) {
                *output++ = ' ';
                input++;
            } else if ((*input == 'm') || (*input == 'M')) {
                int modlen = q2a_strlen(moddir);
                if (max >= modlen && modlen) {
                    q2a_strcpy(output, moddir);
                    output += modlen;
                    max -= (modlen - 1);
                }
                input++;
            } else if ((*input == 't') || (*input == 'T')) {
                struct tm *timestamptm;
                time_t timestampsec;
                char *timestampcp;
                int timestamplen;

                time(&timestampsec);
                timestamptm = localtime(&timestampsec);
                timestampcp = asctime(timestamptm);
                timestamplen = q2a_strlen(timestampcp) - 1; // minus the \n

                if (timestamplen && max >= timestamplen) {
                    q2a_strncpy(output, timestampcp, timestamplen);
                    output += timestamplen;
                    max -= (timestamplen - 1);
                }
                input++;
            } else {
                *output++ = *input++;
            }

            max--;
        } else {
            *output++ = *input++;
            max--;
        }
    }
    *output = 0x0;
    return input;
}

/**
 * Interprets a config file or console argument as a boolean, so settings
 * can be written the way an admin would naturally type them rather than
 * requiring 1/0.
 *
 * arg: the string to interpret. "yes", "y" and "1" are true, in any
 *      casing.
 *
 * Returns true only for those; anything else - including "true",
 * unrecognised text and an empty string - is false. That default means a
 * typo'd setting reads as off rather than erroring.
 */
bool getLogicalValue(char *arg) {
    if (Q_stricmp(arg, "Yes") == 0 ||
            Q_stricmp(arg, "1") == 0 ||
            Q_stricmp(arg, "Y") == 0) {
        return true;
    }
    return false;
}

/**
 * Reads the line ending at *fpos by scanning *backwards* through a file,
 * so log files can be shown newest-first without loading the whole thing
 * or making a forward pass just to find the end.
 *
 * Walks back a byte at a time collecting characters until it hits a
 * newline or start of file, then reverses what it collected into
 * `buffer`. *fpos is left just before the line it returned, so repeated
 * calls walk steadily back through the file.
 *
 * buffer:   receives the line, NUL terminated. Needs room for 256 bytes,
 *           the cap on how much of a long line is kept.
 * dumpfile: open file to read; its position is moved around freely.
 * fpos:     in/out, the offset to read back from. Updated ready for the
 *           next call; a negative value means the start of the file has
 *           been passed.
 *
 * Returns 1 if a line was produced, 0 once *fpos has gone negative and
 * there's nothing left to read.
 *
 * Uses the global buffer2 as scratch, so it clobbers anything else
 * relying on that and can't be nested.
 */
int getLastLine(char *buffer, FILE *dumpfile, long *fpos) {
    char *bp = buffer2;
    int length = 255;

    if (*fpos < 0) {
        return 0;
    }

    while (length && *fpos >= 0) {
        fseek(dumpfile, *fpos, SEEK_SET);
        (*fpos)--;
        if (fread(bp, 1, 1, dumpfile) != 1) {
            break;
        }
        if (*bp == '\n') {
            break;
        }
        bp++;
        length--;
    }
    if (bp != buffer2) {
        bp--;
        // reverse string
        while (bp >= buffer2) {
            *buffer++ = *bp--;
        }
    }
    *buffer = 0;
    return 1;
}

/**
 * Upper-cases a string in place. Mostly used to normalise both sides of
 * a comparison before matching, and to fold regex patterns and the text
 * they're matched against so those rules end up case insensitive.
 *
 * c: the string to convert in place; the caller's buffer is modified, so
 *    copy first if the original is still needed.
 */
void upperCase(char *c) {
    while (*c) {
        if (islower((*c))) {
            *c = toupper((*c));
        }
        c++;
    }
}

/**
 * Lower-cases a string in place, the counterpart to upperCase() for the
 * places that want the other normalisation - filenames, and values
 * compared against lowercase literals.
 *
 * c: the string to convert in place; the caller's buffer is modified, so
 *    copy first if the original is still needed.
 */
void lowerCase(char *c) {
    while (*c) {
        if (isupper((*c))) {
            *c = tolower((*c));
        }
        c++;
    }
}

/**
 * vsnprintf() wrapper that papers over the Win32 runtime's differing
 * behaviour (where the MSVC variants don't NUL terminate on truncation
 * and need a separate call to measure), so the rest of q2admin gets one
 * predictable formatting primitive on every platform. The other
 * Q_*printf functions here all funnel through this.
 *
 * dest:   destination buffer, always NUL terminated.
 * size:   capacity of dest, including the terminator.
 * fmt:    printf style format string.
 * argptr: the already-started va_list of arguments.
 *
 * Returns number of characters that would be written into the buffer,
 * excluding trailing '\0'. If the returned value is equal to or greater than
 * buffer size, resulting string is truncated.
 *
 * WARNING: On Win32, until MinGW-w64 vsnprintf() bug is fixed, this may return
 * SIZE_MAX on overflow. Only use return value to test for overflow, don't use
 * it to allocate memory.
 */
size_t Q_vsnprintf(char *dest, size_t size, const char *fmt, va_list argptr) {
    int ret;

#ifdef _WIN32
    if (size) {
        ret = _vsnprintf(dest, size - 1, fmt, argptr);
        if (ret < 0 || ret >= size - 1)
            dest[size - 1] = 0;
    } else {
        ret = _vscprintf(fmt, argptr);
    }
#else
    ret = vsnprintf(dest, size, fmt, argptr);
#endif

    return ret;
}

/**
 * Like Q_vsnprintf(), but reports what actually landed in the buffer
 * rather than what would have. That makes the return value safe to use
 * as an offset for appending more text, which the "would have" count
 * isn't once truncation happens.
 *
 * dest:   destination buffer, always NUL terminated when size is non-zero.
 * size:   capacity of dest, including the terminator.
 * fmt:    printf style format string.
 * argptr: the already-started va_list of arguments.
 *
 * Returns number of characters actually written into the buffer,
 * excluding trailing '\0'. If buffer size is 0, this function does nothing
 * and returns 0.
 */
size_t Q_vscnprintf(char *dest, size_t size, const char *fmt, va_list argptr) {
    if (size) {
        size_t ret = Q_vsnprintf(dest, size, fmt, argptr);
        return min(ret, size - 1);
    }
    return 0;
}

/**
 * The variadic form of Q_vsnprintf() - the everyday "format into this
 * buffer safely" call, used in preference to raw snprintf() so the Win32
 * differences stay handled in one place.
 *
 * dest: destination buffer, always NUL terminated.
 * size: capacity of dest, including the terminator.
 * fmt:  printf style format string, followed by its arguments.
 *
 * Returns number of characters that would be written into the buffer,
 * excluding trailing '\0'. If the returned value is equal to or greater
 * than buffer size, resulting string is truncated.
 *
 * WARNING: On Win32, until MinGW-w64 vsnprintf() bug is fixed, this may return
 * SIZE_MAX on overflow. Only use return value to test for overflow, don't use
 * it to allocate memory.
 */
size_t Q_snprintf(char *dest, size_t size, const char *fmt, ...) {
    va_list argptr;
    size_t  ret;

    va_start(argptr, fmt);
    ret = Q_vsnprintf(dest, size, fmt, argptr);
    va_end(argptr);

    return ret;
}

/**
 * The variadic form of Q_vscnprintf(). Use this one instead of
 * Q_snprintf() when the return value will be used to keep appending -
 * repeatedly advancing a pointer by the result is only correct with the
 * actually-written count.
 *
 * dest: destination buffer, always NUL terminated when size is non-zero.
 * size: capacity of dest, including the terminator.
 * fmt:  printf style format string, followed by its arguments.
 *
 * Returns number of characters actually written into the buffer, excluding
 * trailing '\0'. If buffer size is 0, this function does nothing and returns
 * 0.
 *
 * Stolen from q2pro
 */
size_t Q_scnprintf(char *dest, size_t size, const char *fmt, ...) {
    va_list argptr;
    size_t  ret;

    va_start(argptr, fmt);
    ret = Q_vscnprintf(dest, size, fmt, argptr);
    va_end(argptr);
    return ret;
}

/**
 * Joins any number of strings into one buffer in a single call, as a
 * bounded alternative to a chain of strcat()s - handy for assembling
 * paths and messages from several pieces.
 *
 * dest: destination buffer, NUL terminated as long as size is non-zero.
 * size: capacity of dest, including the terminator.
 * ...:  the strings to concatenate, terminated by a NULL argument. That
 *       sentinel is mandatory; forgetting it walks off the end of the
 *       argument list.
 *
 * Returns number of characters that would be written into the buffer,
 * excluding trailing '\0'. If the returned value is equal to or greater than
 * buffer size, resulting string is truncated.
 *
 * Stolen from q2pro
 */
size_t Q_concat(char *dest, size_t size, ...) {
    va_list argptr;
    const char *s;
    size_t len, total = 0;

    va_start(argptr, size);
    while ((s = va_arg(argptr, const char *)) != NULL) {
        len = strlen(s);
        if (total + len < size) {
            q2a_memcpy(dest, s, len);
            dest += len;
        }
        total += len;
    }
    va_end(argptr);

    if (size) {
        *dest = 0;
    }

    return total;
}

/**
 * Appends src to dst without overrunning size, the bounded replacement
 * for strcat().
 *
 * dst:  destination string, appended to in place and NUL terminated.
 * src:  string to append.
 * size: total capacity of dst, including the terminator - not the space
 *       remaining.
 *
 * Returns the length the result would need (existing length plus src
 * length); a value >= size means it was truncated.
 *
 * dst must already be NUL terminated and no longer than size - the
 * remaining space is computed as size minus the current length, which
 * underflows into a huge value if that isn't true.
 */
size_t Q_strlcat(char *dst, const char *src, size_t size) {
    size_t len = q2a_strlen(dst);
    return len + Q_strlcpy(dst + len, src, size - len);
}

/**
 * Copies src into dst, always NUL terminating and never writing past
 * size - the bounded replacement for strcpy(), and safer than strncpy()
 * which leaves the result unterminated on truncation.
 *
 * dst:  destination buffer.
 * src:  string to copy.
 * size: capacity of dst, including the terminator.
 *
 * Returns the length of src, i.e. what it would have taken to copy it
 * whole - so a return >= size means the copy was truncated. Note that's
 * the source length, not the number of bytes written.
 */
size_t Q_strlcpy(char *dst, const char *src, size_t size) {
    size_t ret = q2a_strlen(src);

    if (size) {
        size_t len = min(ret, size - 1);
        q2a_memcpy(dst, src, len);
        dst[len] = 0;
    }
    return ret;
}

/**
 * Case insensitive comparison of at most the first n characters, for
 * matching a known-length prefix without needing to copy it out first.
 * Also the primitive Q_strcasestr() is built on.
 *
 * s1, s2: strings to compare. Unlike Q_stricmp() these are const, so
 *         this one can be used on string literals without casting.
 * n:      maximum number of characters to compare.
 *
 * Returns 0 if the first n characters match ignoring case, -1 if s1
 * sorts first, 1 if s2 does.
 *
 * Stolen from Q2Pro
 */
int Q_strncasecmp(const char *s1, const char *s2, size_t n) {
    int c1, c2;

    do {
        c1 = *s1++;
        c2 = *s2++;
        if (!n--) {
            return 0;
        }
        if (c1 != c2) {
            c1 = Q_tolower(c1);
            c2 = Q_tolower(c2);
            if (c1 < c2) {
                return -1;
            }
            if (c1 > c2) {
                return 1;
            }
        }
    } while (c1);
    return 0;
}


/**
 * Case insensitive version of strstr. Where stringContains() only
 * answers yes/no and copies its inputs to do it, this returns the
 * position of the match and touches neither string, so use it when the
 * location matters or the inputs are large.
 *
 * s1: the string to search.
 * s2: the substring to find.
 *
 * If s2 is a substring of s1, return a pointer to it in s1. Empty s2
 * will just return the beginning of s1. No match will return NULL.
 * The result points into s1, so it's only valid while s1 is.
 */
char *Q_strcasestr(const char *s1, const char *s2) {
    size_t l1, l2;

    l2 = q2a_strlen(s2);
    if (!l2) {
        return (char *)s1;
    }
    l1 = q2a_strlen(s1);
    while (l1 >= l2) {
        l1--;
        if (!Q_strncasecmp(s1, s2, l2)) {
            return (char *)s1;
        }
        s1++;
    }
    return NULL;
}

/**
 * Rounds up to an int, returning an int directly rather than the double
 * that math.h's ceil() gives back. Exists because that one behaved
 * oddly here for values between 0 and 1.
 *
 * x: the value to round up.
 *
 * Returns the smallest integer not less than x.
 */
int Q_ceil(float x) {
    float temp;

    temp = x - (int)x;
    if (temp > 0) {
        return ((int)x) + 1;
    } else {
        return (int)x;
    }
}

/**
 * Rounds down to an int, to complement Q_ceil().
 *
 * x: the value to round down.
 *
 * Returns (int)x, which is a plain truncation toward zero rather than a
 * true floor. That matches floor() for positive values but not negative
 * ones: this gives -1 for -1.5 where floor() gives -2. Fine for the
 * non-negative quantities it's used on (times, counts, percentages).
 */
int Q_floor(float x) {
   return (int)x;
}

/**
 * Converts pitch/yaw angles (degrees) into a normalized forward direction
 * vector. Roll is ignored, it doesn't affect where a player is looking.
 *
 * Turning a player's view angles into a direction is what lets the aim
 * checks reason geometrically about where someone is pointing - tracing
 * along it, or measuring it against the direction to another player.
 *
 * angles:  pitch/yaw/roll in degrees, as sent in a usercmd_t (convert
 *          from the wire's short encoding with SHORT2ANGLE first).
 * forward: receives the resulting unit vector.
 */
void angleVectorsForward(vec3_t angles, vec3_t forward) {
    float yaw = (float)(angles[YAW] * (M_PI / 180.0));
    float pitch = (float)(angles[PITCH] * (M_PI / 180.0));
    float sy = sinf(yaw), cy = cosf(yaw);
    float sp = sinf(pitch), cp = cosf(pitch);

    forward[0] = cp * cy;
    forward[1] = cp * sy;
    forward[2] = -sp;
}

/**
 * Angle in degrees between two vectors, order doesn't matter, vectors need
 * not be normalized or the same length.
 *
 * This is how "how far off target is this player looking" gets measured -
 * comparing a view direction against the direction to another player, or
 * against the same player's view a frame earlier to see how far they
 * swung.
 *
 * a, b: the two vectors; lengths are divided out, so raw
 *       eye-to-target differences can be passed without normalizing.
 *
 * Returns 0 through 180. A zero-length vector has no direction to
 * compare, so that returns 0 rather than a NaN from dividing by zero.
 */
float angleBetweenVectors(vec3_t a, vec3_t b) {
    float lena = sqrtf(DotProduct(a, a));
    float lenb = sqrtf(DotProduct(b, b));
    float cosangle;

    if (lena < 0.0001f || lenb < 0.0001f) {
        return 0;
    }

    cosangle = DotProduct(a, b) / (lena * lenb);
    if (cosangle > 1.0f) {
        cosangle = 1.0f;
    } else if (cosangle < -1.0f) {
        cosangle = -1.0f;
    }

    return acosf(cosangle) * (float)(180.0 / M_PI);
}

/**
 * Rate limits a client to one command every 3 seconds, so a player can't
 * spam commands that cost the server real work or spray output at
 * everyone.
 *
 * client: the client index to check.
 *
 * Returns true if they're allowed to run one now, false if they're still
 * inside the cooldown.
 *
 * Not a pure query - returning true *consumes* the allowance and starts
 * the next cooldown, so call it once at the point of deciding and reuse
 * the answer rather than calling it again to re-check.
 */
bool newCommandAllowed(int client) {
    if (proxyinfo[client].newcmd_timeout <= ltime) {
        proxyinfo[client].newcmd_timeout = ltime + 3;
        return true;
    } else {
        return false;
    }
}

/**
 * Fills a buffer with random alphanumeric characters.
 *
 * These are the unguessable tokens the client probes are built around:
 * an alias or cvar is named with one of these and the client is asked to
 * echo it back, so a cheat client can't recognise a fixed string and
 * pre-canned a reply. Fresh randomness per probe is the whole point.
 *
 * buffer: destination, which must have room for length + 1 bytes - a
 *         terminator is written at buffer[length].
 * length: how many random characters to generate.
 */
void randomString(char *buffer, int length) {
    unsigned int i;
    for (i = 0; i < length; i++) {
        buffer[i] = RANDCHAR();
    }
    buffer[i] = 0;
}

/**
 * Ensure a filesystem path is valid and appropriate.
 * - relative paths only
 * - no ".."s
 * - only printable characters
 *
 * This is a path traversal guard: several settings and commands name
 * files to read or write, and without this a leading '/' or a "../"
 * could reach outside the mod directory.
 *
 * s: the path to check.
 *
 * Returns PATH_VALID if acceptable, PATH_INVALID otherwise. Note
 * PATH_MIXED_CASE exists in pathtype_t but is never returned here, so
 * callers only ever see the two outcomes.
 */
pathtype_t validatePath(const char *s) {
    int res = PATH_VALID;

    if (*s == '/') {
        return PATH_INVALID;
    }
    if (stringContains((char *)s, "..")) {
        return PATH_INVALID;
    }
    for (; *s; s++) {
        if (!Q_isprint(*s)) {
            return PATH_INVALID;
        }
    }
    return res;
}

/**
 * Print a formatted string to the server console prepended with an identifier
 * to make it obvious the message was from q2admin. Primary use-case is for
 * console logging and info prints.
 *
 * Since q2admin sits between the server and a game mod that's also
 * printing to the same console, the "[q2a]" tag is what tells an admin
 * reading the log which side a line came from.
 *
 * fmt: printf style format string, followed by its arguments. Output
 *      over 8KB is truncated.
 *
 * Goes to the server console only - passing NULL as the edict means no
 * player sees it, so this is safe for messages that shouldn't be
 * broadcast.
 */
void q2a_printf(char *fmt, ...) {
    char cbuffer[8192];
    va_list arglist;

    va_start(arglist, fmt);
    Q_vsnprintf(cbuffer, sizeof(cbuffer), fmt, arglist);
    va_end(arglist);

    // NULL edict sends to the console only
    gi.cprintf(NULL, PRINT_HIGH, "[q2a] %s", cbuffer);
}
