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

#ifdef Q2ADMINCLIB


char *q2a_strcpy(char *strDestination, const char *strSource) {
    char *ret = strDestination;

    while (*strSource) {
        *strDestination++ = *strSource++;
    }

    *strDestination = *strSource;

    return ret;
}

char *q2a_strncpy(char *strDest, const char *strSource, size_t count) {
    char *cp = strDest;

    if (!count) {
        return strDest;
    }

    count--;

    while (*strSource && count) {
        *cp++ = *strSource++;
        if (count) {
            count--;
        }
    }

    *cp = 0x0;

    return strDest;
}

char *q2a_strcat(char *strDestination, const char *strSource) {
    char *cp = strDestination;

    while (*cp) {
        cp++;
    }

    q2a_strcpy(cp, strSource);

    return strDestination;
}

int q2a_strcmp(const char *string1, const char *string2) {
    while (*string1 && *string2) {
        if (*string1 != *string2) {
            if (*string1 < *string2) {
                return-1;
            } else {
                return 1;
            }
        }

        string1++;
        string2++;
    }

    if (*string2) {
        return-1;
    }

    if (*string1) {
        return 1;
    }

    return 0;
}

char *q2a_strstr(const char *string, const char *strCharSet) {
    const char *cp;
    const char *ts;

    while (*string) {
        if (*string == *strCharSet) {
            cp = string;
            cp += 1;
            ts = strCharSet;
            ts += 1;

            while (*cp && *ts) {
                if (*cp != *ts) {
                    break;
                }
                cp++;
                ts++;
            }

            if (!(*ts)) {
                return (char *) string;
            }
        }

        string++;
    }

    return 0x0;
}

char *q2a_strchr(const char *string, int c) {
    while (*string) {
        if (*string == c) {
            return (char *) string;
        }
        string++;
    }

    return 0x0;
}

size_t q2a_strlen(const char *string) {
    size_t len = 0;

    while (*string) {
        len++;
        string++;
    }

    return len;
}

int q2a_memcmp(const void *buf1, const void *buf2, size_t count) {
    unsigned long *dwbuf1 = (unsigned long *) buf1;
    unsigned long *dwbuf2 = (unsigned long *) buf2;
    unsigned char *bbuf1, *bbuf2;

    size_t dwcount = count / sizeof (unsigned long);
    count = count % sizeof (unsigned long);


    while (dwcount) {
        if (*dwbuf1 != *dwbuf2) {
            return 1;
        }

        dwbuf1++;
        dwbuf2++;
        dwcount--;
    }

    if (count) {
        bbuf1 = (unsigned char *) dwbuf1;
        bbuf2 = (unsigned char *) dwbuf2;

        while (count) {
            if (*bbuf1 != *bbuf2) {
                return 1;
            }

            bbuf1++;
            bbuf2++;
            count--;
        }
    }

    return 0;
}

void *q2a_memcpy(void *dest, const void *src, size_t count) {
    unsigned long *dwbuf1 = (unsigned long *) dest;
    unsigned long *dwbuf2 = (unsigned long *) src;
    unsigned char *bbuf1, *bbuf2;

    size_t dwcount = count / sizeof (unsigned long);
    count = count % sizeof (unsigned long);


    while (dwcount) {
        *dwbuf1++ = *dwbuf2++;
        dwcount--;
    }

    if (count) {
        bbuf1 = (unsigned char *) dwbuf1;
        bbuf2 = (unsigned char *) dwbuf2;

        while (count) {
            *bbuf1++ = *bbuf2++;
            count--;
        }
    }

    return dest;
}

void *q2a_memmove(void *dest, const void *src, size_t count) {
    if ((unsigned char *) dest > (unsigned char *) src && (unsigned char *) dest < (unsigned char *) src + count) {/* overlap... */
        char *buf = gi.TagMalloc(count, TAG_LEVEL);
        void *ret;
        q2a_memcpy(buf, src, count);
        ret = q2a_memcpy(dest, buf, count);
        gi.TagFree(buf);
        return ret;
    }

    return q2a_memcpy(dest, src, count);
}

void *q2a_memset(void *dest, int c, size_t count) {
    unsigned char *bbuf1 = (unsigned char *) dest;

    while (count) {
        *bbuf1++ = c;
        count--;
    }

    return dest;
}

int q2a_atoi(const char *string) {
    int retvalue = 0;

    switch (*string) {
        case'-':
            retvalue = -retvalue;
            string++;
            break;

        case'+':
            retvalue = +retvalue;
            string++;
            break;
    }

    while (isdigit(*string)) {
        retvalue = (retvalue * 10)+(*string - '0');
        string++;
    }

    return retvalue;
}

double q2a_atof(const char *string) {
    double retvalue = 0;
    unsigned int divide = 10;

    switch (*string) {
        case'-':
            retvalue = -retvalue;
            string++;
            break;

        case'+':
            retvalue = +retvalue;
            string++;
            break;
    }

    while (isdigit(*string)) {
        retvalue = (retvalue * 10.0)+(*string - '0');
        string++;
    }

    if (*string == '.') {
        string++;

        while (isdigit(*string)) {
            retvalue += (*string - '0') / divide;
            divide *= 10;
            string++;
        }
    }

    return retvalue;
}

#endif

#define SPRINTF(x) ((size_t)sprintf x)
static const char *q2a_inet_ntop4 (const u_char *src, char *dst, socklen_t size);
static const char *q2a_inet_ntop6 (const u_char *src, char *dst, socklen_t size);

/* char *
 * inet_ntop(af, src, dst, size)
 *        convert a network format address to presentation format.
 * return:
 *        pointer to presentation format address (`dst'), or NULL (see errno).
 * author:
 *        Paul Vixie, 1996.
 */
const char *q2a_inet_ntop (int af, const void *src, char *dst, socklen_t size)
{
        switch (af) {
        case AF_INET:
                return (q2a_inet_ntop4(src, dst, size));
        case AF_INET6:
                return (q2a_inet_ntop6(src, dst, size));
        default:
                //__set_errno (EAFNOSUPPORT);
                return (NULL);
        }
        /* NOTREACHED */
}

// libc_hidden_def (inet_ntop)
/* const char *
 * inet_ntop4(src, dst, size)
 *        format an IPv4 address
 * return:
 *        `dst' (as a const)
 * notes:
 *        (1) uses no statics
 *        (2) takes a u_char* not an in_addr as input
 * author:
 *        Paul Vixie, 1996.
 */
static const char *q2a_inet_ntop4 (const u_char *src, char *dst, socklen_t size)
{
        static const char fmt[] = "%u.%u.%u.%u";
        char tmp[sizeof "255.255.255.255"];
        if (SPRINTF((tmp, fmt, src[0], src[1], src[2], src[3])) >= size) {
                //__set_errno (ENOSPC);
                return (NULL);
        }
        return strcpy(dst, tmp);
}

/* const char *
 * inet_ntop6(src, dst, size)
 *        convert IPv6 binary address into presentation (printable) format
 * author:
 *        Paul Vixie, 1996.
 */
static const char *q2a_inet_ntop6 (const u_char *src, char *dst, socklen_t size)
{
        /*
         * Note that int32_t and int16_t need only be "at least" large enough
         * to contain a value of the specified size.  On some systems, like
         * Crays, there is no such thing as an integer variable with 16 bits.
         * Keep this in mind if you think this function should have been coded
         * to use pointer overlays.  All the world's not a VAX.
         */
        char tmp[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"], *tp;
        struct { int base, len; } best, cur;
        u_int words[NS_IN6ADDRSZ / NS_INT16SZ];
        int i;
        /*
         * Preprocess:
         *        Copy the input (bytewise) array into a wordwise array.
         *        Find the longest run of 0x00's in src[] for :: shorthanding.
         */
        memset(words, '\0', sizeof words);
        for (i = 0; i < NS_IN6ADDRSZ; i += 2)
                words[i / 2] = (src[i] << 8) | src[i + 1];
        best.base = -1;
        cur.base = -1;
        best.len = 0;
        cur.len = 0;
        for (i = 0; i < (NS_IN6ADDRSZ / NS_INT16SZ); i++) {
                if (words[i] == 0) {
                        if (cur.base == -1)
                                cur.base = i, cur.len = 1;
                        else
                                cur.len++;
                } else {
                        if (cur.base != -1) {
                                if (best.base == -1 || cur.len > best.len)
                                        best = cur;
                                cur.base = -1;
                        }
                }
        }
        if (cur.base != -1) {
                if (best.base == -1 || cur.len > best.len)
                        best = cur;
        }
        if (best.base != -1 && best.len < 2)
                best.base = -1;
        /*
         * Format the result.
         */
        tp = tmp;
        for (i = 0; i < (NS_IN6ADDRSZ / NS_INT16SZ); i++) {
                /* Are we inside the best run of 0x00's? */
                if (best.base != -1 && i >= best.base &&
                    i < (best.base + best.len)) {
                        if (i == best.base)
                                *tp++ = ':';
                        continue;
                }
                /* Are we following an initial run of 0x00s or any real hex? */
                if (i != 0)
                        *tp++ = ':';
                /* Is this address an encapsulated IPv4? */
                if (i == 6 && best.base == 0 &&
                    (best.len == 6 || (best.len == 5 && words[5] == 0xffff))) {
                        if (!q2a_inet_ntop4(src+12, tp, sizeof tmp - (tp - tmp)))
                                return (NULL);
                        tp += strlen(tp);
                        break;
                }
                tp += SPRINTF((tp, "%x", words[i]));
        }
        /* Was it a trailing run of 0x00's? */
        if (best.base != -1 && (best.base + best.len) ==
            (NS_IN6ADDRSZ / NS_INT16SZ))
                *tp++ = ':';
        *tp++ = '\0';
        /*
         * Check for overflow, copy, and we're done.
         */
        if ((socklen_t)(tp - tmp) > size) {
                //__set_errno (ENOSPC);
                return (NULL);
        }
        return strcpy(dst, tmp);
}
