/**
 * Q2Admin Logging
 * 
 * Both log file definitions and the mapping of which events go to which log
 * are defined in the same config file. 
 * 
 * For log files, each one is defined with a unique index (1-LOGMAX). 
 *   LOGFILE: <1-32> [MOD] "filename"
 * 
 * For log events, each type is defined as whether to actually log the event,
 * the index of the log file that should receive it, and the format each event
 * should take.
 *   <TYPE>: <YES/NO> <1-32> "format"
 */

#pragma once

#define LOGLISTFILE     "q2a_log.cfg"
#define LOGFILECMD      "[sv] !logfile [view <logfilenum> / edit [filenum(1-32)] [mod] [filename] / del [filenum(1-32)]]\n"
#define LOGEVENTCMD     "[sv] !logevent [view <logtype> / edit [logtype] <log [yes/no]> <logfiles [logfile[+logfile...]]> <format \"format\">]\n"
#define LOGTYPES_MAX    (sizeof(logtypes) / sizeof(logtypes[0]))
#define MAXLOGS         32

typedef struct {
    qboolean inuse;
    qboolean mod;
    char filename[256];
    FILE *fp;
} logfile_t;

typedef struct {
    char *logtype;
    qboolean log;
    unsigned long logfiles;
    char format[4096];
} logtypes_t;

/**
 * Possible kinds of log entries
 */
enum zb_logtypesenum {
    LT_ZBOT,
    LT_ZBOTIMPULSES,
    LT_IMPULSES,
    LT_NAMECHANGE,
    LT_SKINCHANGE,
    LT_CHATBAN,
    LT_CLIENTCONNECT,
    LT_CLIENTBEGIN,
    LT_CLIENTDISCONNECT,
    LT_CLIENTKICK,
    LT_CLIENTCMDS,
    LT_CLIENTLRCON,
    LT_BAN,
    LT_CHAT,
    LT_SERVERSTART,
    LT_SERVERINIT,
    LT_SERVEREND,
    LT_INTERNALWARN,
    LT_PERFORMANCEMONITOR,
    LT_DISABLECMD,
    LT_ENTITYCREATE,
    LT_ENTITYDELETE,
    LT_INVALIDIP,
    LT_ADMINLOG,
    LT_CLIENTUSERINFO,
    LT_PRIVATELOG,
};

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
void logEvent(enum zb_logtypesenum ltype, int client, edict_t *ent, char *message, int number, float number2, qboolean echo);
void logeventRun(int startarg, edict_t *ent, int client);
void logfileRun(int startarg, edict_t *ent, int client);
void openLogFiles(void);
void closeLogFiles(void);
qboolean isLogWritable(int index);