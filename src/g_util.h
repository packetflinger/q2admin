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

#pragma once

#define SKIPBLANK(str) \
{\
    while(*str == ' ' || *str == '\t') \
    { \
        str++; \
    } \
}

typedef enum {
    PATH_VALID,
    PATH_MIXED_CASE,
    PATH_INVALID,
} pathtype_t;

float angleBetweenVectors(vec3_t a, vec3_t b);
void angleVectorsForward(vec3_t angles, vec3_t forward);
int breakLine(char *buffer, char *buff1, char *buff2, int buff2size);
int wordCount(const char *s);
float decayFactor(float elapsed, float halflife);
void G_MergeEdicts(void);
int getLastLine(char *buffer, FILE *dumpfile, long *fpos);
bool getLogicalValue(char *arg);
void Info_RemoveKey(char *s, const char *key);
void Info_SetValueForKey(char *s, const char *key, const char *value);
bool Info_Validate(char *s);
char *Info_ValueForKey(char *s, char *key);
int isBlank(char *buff1);
void lowerCase(char *c);
bool newCommandAllowed(int client);
char *processString(char *output, char *input, int max, char end);
void Q_printf(char *fmt, ...);
int Q_ceil(float x);
size_t Q_concat(char *dest, size_t size, ...);
int Q_floor(float x);
size_t Q_scnprintf(char *dest, size_t size, const char *fmt, ...);
size_t Q_snprintf(char *dest, size_t size, const char *fmt, ...);
char *Q_strcasestr(const char *s1, const char *s2);
int Q_stricmp(char *s1, char *s2);
size_t Q_strlcat(char *dst, const char *src, size_t size);
size_t Q_strlcpy(char *dst, const char *src, size_t size);
int Q_strncasecmp(const char *s1, const char *s2, size_t n);
char *Q_trim(char *s);
size_t Q_vscnprintf(char *dest, size_t size, const char *fmt, va_list argptr);
size_t Q_vsnprintf(char *dest, size_t size, const char *fmt, va_list argptr);
void randomString(char *buffer, int length);
bool startContains(char *src, char *cmp);
bool startswith(char *needle, char *haystack);
int stringContains(char *buff1, char *buff2);
void stuffPlayer(edict_t *e, char *s);
void upperCase(char *c);
char *va(const char *format, ...);
pathtype_t validatePath(const char *s);
bool wildcardMatch(char *pattern, char *haystack);
void Q_hexdump(char *desc, void *addr, int len);
