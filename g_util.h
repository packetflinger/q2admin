/**
 * Q2Admin
 * Utilities and helper functions
 */

#pragma once

int breakLine(char *buffer, char *buff1, char *buff2, int buff2size);
qboolean can_do_new_cmds(int client);
void G_MergeEdicts(void);
void generateRandomString(char *buffer, int length);
int getLastLine(char *buffer, FILE *dumpfile, long *fpos);
qboolean getLogicalValue(char *arg);
qboolean Info_Validate(char *s);
char *Info_ValueForKey(char *s, char *key);
int isBlank(char *buff1);
char *processstring(char *output, char *input, int max, char end);
size_t Q_concat(char *dest, size_t size, ...);
size_t Q_scnprintf(char *dest, size_t size, const char *fmt, ...);
size_t Q_snprintf(char *dest, size_t size, const char *fmt, ...);
char *Q_strcasestr(const char *s1, const char *s2);
int Q_stricmp(char *s1, char *s2);
size_t Q_strlcat(char *dst, const char *src, size_t size);
int Q_strncasecmp(const char *s1, const char *s2, size_t n);
void q_strupr(char *c);
size_t Q_vscnprintf(char *dest, size_t size, const char *fmt, va_list argptr);
size_t Q_vsnprintf(char *dest, size_t size, const char *fmt, va_list argptr);
int q2a_ceil(float x);
int q2a_floor(float x);
void q2admin_free();
char *q2admin_malloc(int size);
char *q2admin_realloc();
int startContains(char *src, char *cmp);
qboolean startswith(char *needle, char *haystack);
int stringContains(char *buff1, char *buff2);
void stuffcmd(edict_t *e, char *s);
char *trim(char *s);
char *va(const char *format, ...);
qboolean wildcard_match(char *pattern, char *haystack);
