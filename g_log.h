/**
 * Q2Admin
 * Logging stuff
 */

#pragma once

#define LOGFILECMD    "[sv] !logfile [view <logfilenum> / edit [filenum(1-32)] [mod] [filename] / del [filenum(1-32)]]\n"
#define LOGEVENTCMD    "[sv] !logevent [view <logtype> / edit [logtype] <log [yes/no]> <logfiles [logfile[+logfile...]]> <format \"format\">]\n"
#define LOGTYPES_MAX    (sizeof(logtypes) / sizeof(logtypes[0]))

typedef struct {
    qboolean inuse;
    qboolean mod;
    char filename[256];
} logfile_t;

logfile_t logFiles[32];

typedef struct {
    char *logtype;
    qboolean log;
    unsigned long logfiles;
    char format[4096];
} logtypes_t;

void clearlogfileRun(int startarg, edict_t *ent, int client);
void convertToLogLine(char *dest, char *format, int client, edict_t *ent, char *message, int number, float number2);
void displayLogEventListCont(edict_t *ent, int client, long logevent, qboolean onetimeonly);
void displayLogFileCont(edict_t *ent, int client, long logfilereadpos);
void displayLogFileListCont(edict_t *ent, int client, long logfilenum);
void displaylogfileRun(int startarg, edict_t *ent, int client);
void expandOutPortNum(char *srcdest, int max);
qboolean isLogEvent(enum zb_logtypesenum ltype);
void loadLogList(void);
qboolean loadLogListFile(char *filename);
void logEvent(enum zb_logtypesenum ltype, int client, edict_t *ent, char *message, int number, float number2);
void logeventRun(int startarg, edict_t *ent, int client);
void logfileRun(int startarg, edict_t *ent, int client);
