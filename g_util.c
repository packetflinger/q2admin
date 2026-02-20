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

// required for proxy testing

/**
 * Force entity to do a command
 */
void stuffcmd(edict_t *e, char *s) {
    gi.WriteByte(SVC_STUFFTEXT);
    gi.WriteString(s);
    gi.unicast(e, true);
}

/**
 * Remove whitespace (space/tab/newline) from the beginning and end of a string
 */
char *trim(char *s) {
    char *ptr;
    if (!s) {
        return NULL;   // handle NULL string
    }
    if (!*s) {
        return s;      // handle empty string
    }
    for (ptr = s + strlen(s) - 1; (ptr >= s) && isspace(*ptr); --ptr);
    ptr[1] = '\0';
    return s;
}

/**
 * Variable assignment. Build a string printf style inline
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
 * Recursively compare strings with wildcards.
 */
bool wildcard_match(char *pattern, char *haystack) {
    if (*pattern == '\0' && *haystack == '\0') {
        return true;
    }

    if (*pattern == '*' && *(pattern+1) != '\0' && *haystack == '\0') {
        return false;
    }

    if (*pattern == '?' || *pattern == *haystack) {
        return wildcard_match(pattern+1, haystack+1);
    }

    if (*pattern == '*') {
        return wildcard_match(pattern+1, haystack) || wildcard_match(pattern, haystack+1);
    }
    return false;
}

/**
 * Maybe not needed? Use startContains instead?
 */
bool startswith(char *needle, char *haystack) {
    return (strncmp(needle, haystack, strlen(needle)) == 0);
}

/**
 * Case insensitive string compare
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
 *
 */
char *q2admin_malloc(int size) {
    char *mem = gi.TagMalloc(size + sizeof (int), TAG_GAME);
    *(int *) mem = size;
    return mem + sizeof (int);
}

/**
 *
 */
char *q2admin_realloc(char *oldmem, int newsize) {
    int oldsize;
    int *start = (int *) (oldmem - sizeof (int));
    char *newmem;

    oldsize = *start;
    if (oldsize >= newsize) {
        return oldmem;
    }
    newmem = gi.TagMalloc(newsize + sizeof (int), TAG_GAME);
    *(int *) newmem = newsize;
    newmem += sizeof (int);
    q2a_memcpy(newmem, oldmem, newsize - oldsize);
    gi.TagFree(start);
    return newmem;
}

/**
 *
 */
void q2admin_free(char *mem) {
    gi.TagFree(mem - sizeof (int));
}

/**
 * Searches the string for the given key and returns the associated value, or
 * an empty string.
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
 * Some characters are illegal in info strings because they can mess up the
 * server's parsing.
 */
bool Info_Validate(char *s) {
    if (q2a_strstr(s, "\"")) {
        return false;
    }
    if (q2a_strstr(s, ";")) {
        return false;
    }
    return true;
}

/**
 * Copy all edicts and states from the forward game library. This is called
 * all over the place and is the heart of how q2admin works.
 */
void G_MergeEdicts(void) {
    ge.apiversion = ge_mod->apiversion;
    ge.edict_size = ge_mod->edict_size;
    ge.edicts = ge_mod->edicts;
    ge.num_edicts = ge_mod->num_edicts;
    ge.max_edicts = ge_mod->max_edicts;
}

/**
 *
 */
int breakLine(char *buffer, char *buff1, char *buff2, int buff2size) {
    char *cp, *dp;

    cp = buffer;
    dp = buff1;
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
    cp = processstring(buff2, cp, buff2size, '\"');
    if (!buff2[0] || *cp != '\"') {
        return 0;
    }
    return 1;
}

/**
 * Is cmp at the beginning of src?
 *
 * Why does this not return a bool?
 */
int startContains(char *src, char *cmp) {
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
 *
 */
int stringContains(char *buff1, char *buff2) {
    char strbuffer1[4096];
    char strbuffer2[4096];

    q2a_strncpy(strbuffer1, buff1, sizeof(strbuffer1));
    q_strupr(strbuffer1);
    q2a_strncpy(strbuffer2, buff2, sizeof(strbuffer2));
    q_strupr(strbuffer2);
    return (q2a_strstr(strbuffer1, strbuffer2) != NULL);
}

/**
 *
 */
int isBlank(char *buff1) {
    while (*buff1 == ' ') {
        buff1++;
    }
    return !(*buff1);
}

// TODO: rename this in proper camelcase
/**
 * Resolve special tokens in input string and render output
 *
 *  n = newline
 *  d = dollar sign $
 *  q = double quote "
 *  s = single quote '
 *  m = mod directory name
 *  t = current timestamp
 *
 *  (case insensitive)
 */
char *processstring(char *output, char *input, int max, char end) {
    while (*input && *input != end && max) {
        if (*input == '\\') {
            *input++;

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
                int modlen = strlen(moddir);
                if (max >= modlen && modlen) {
                    q2a_strcpy(output, moddir);
                    output += modlen;
                    max -= (modlen - 1);
                }
                input++;
            } else if ((*input == 't') || (*input == 'T')) {
                struct tm*timestamptm;
                time_t timestampsec;
                char *timestampcp;
                int timestamplen;

                time(&timestampsec); /* Get time in seconds */
                timestamptm = localtime(&timestampsec); /* Convert time to struct */
                /* tm form */

                timestampcp = asctime(timestamptm); /* get string version of date / time */
                timestamplen = strlen(timestampcp) - 1; /* length minus the '\n' */

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
 * Get a boolean version of a string value.
 *
 * Case insensitive
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
 *
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
 * Change string to all upper case
 */
void q_strupr(char *c) {
    while (*c) {
        if (islower((*c))) {
            *c = toupper((*c));
        }
        c++;
    }
}


/**
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
            memcpy(dest, s, len);
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
 * Returns length of the source and destinations strings combined.
 */
size_t Q_strlcat(char *dst, const char *src, size_t size) {
    size_t len = strlen(dst);
    return len + Q_strlcpy(dst + len, src, size - len);
}

/**
 * Returns length of the source string
 */
size_t Q_strlcpy(char *dst, const char *src, size_t size) {
    size_t ret = strlen(src);

    if (size) {
        size_t len = min(ret, size - 1);
        memcpy(dst, src, len);
        dst[len] = 0;
    }

    return ret;
}

/**
 * Size limited case insensitive string compare
 *
 * Stolen from Q2Pro
 */
int Q_strncasecmp(const char *s1, const char *s2, size_t n) {
    int        c1, c2;

    do {
        c1 = *s1++;
        c2 = *s2++;

        if (!n--) {
            return 0;        /* strings are equal until end point */
        }

        if (c1 != c2) {
            c1 = Q_tolower(c1);
            c2 = Q_tolower(c2);
            if (c1 < c2) {
                return -1;
            }
            if (c1 > c2) {
                return 1;        /* strings not equal */
            }
        }
    } while (c1);
    return 0;        /* strings are equal */
}


/**
 * Case insensitive version of strstr. If s2 is a substring of s1,
 * return a pointer to it in s1. Empty s2 will just return the
 * beginning of s1. No match will return NULL
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
 * The version in math.h was weird with values between 0-1
 */
int q2a_ceil(float x) {
    float temp;

    temp = x - (int)x;
    if (temp > 0) {
        return ((int)x) + 1;
    } else {
        return (int)x;
    }
}

/**
 * Just to complement q2a_ceil
 */
int q2a_floor(float x) {
   return (int)x;
}

/**
 * Throttle command usage
 */
bool can_do_new_cmds(int client) {
    if (proxyinfo[client].newcmd_timeout <= ltime) {
        proxyinfo[client].newcmd_timeout = ltime + 3;
        return true;
    } else {
        return false;
    }
}

/**
 * Random characters of whatever length
 */
void generateRandomString(char *buffer, int length) {
    unsigned int index;
    for (index = 0; index < length; index++) {
        buffer[index] = RANDCHAR();
    }
    buffer[index] = 0;
}

/**
 * Ensure a filesystem path is valid and appropriate.
 * - relative paths only
 * - no ".."s
 * - only printable characters
 */
pathtype_t validatePath(const char *s) {
    int res = PATH_VALID;

    if (*s == "/") {
        return PATH_INVALID;
    }
    if (stringContains(s, "..")) {
        return PATH_INVALID;
    }
    for (; *s; s++) {
        if (!Q_isprint(*s)) {
            return PATH_INVALID;
        }
    }
    return res;
}
