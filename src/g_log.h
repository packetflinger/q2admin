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

#define LOGLISTFILE     "q2a_log.cfg"
#define LOGFILECMD      "[sv] !logfile [view <logfilenum> / edit [filenum(1-32)] [mod] [filename] / del [filenum(1-32)]]\n"
#define LOGEVENTCMD     "[sv] !logevent [view <logtype> / edit [logtype] <log [yes/no]> <logfiles [logfile[+logfile...]]> <format \"format\">]\n"
#define LOGTYPES_MAX    (sizeof(logtypes) / sizeof(logtypes[0]))
#define MAXLOGS         32

typedef struct {
    bool inuse;
    bool mod;
    char filename[256];
    FILE *fp;
} logfile_t;

typedef struct {
    char *logtype;
    bool log;
    unsigned long logfiles;
    char format[4096];
} logtypes_t;

/**
 * The "#e" value on an LT_ZBOT log line: which check decided the client was
 * cheating.
 *
 * All negative, because the same field carries the zbot test-string index
 * (0 and up) while nothing has been decided yet - proxyinfo_t.charindex is
 * both, which is why the detections store their reason there and the log
 * call just passes it along.
 *
 * The numbers are what has always been written to logs, so they are kept
 * as they are rather than tidied into a run; ZR_USERINFO_SKON really does
 * skip -10 to -13.
 */
typedef enum {
    ZR_STARTUP_NOREPLY  = -1,   // handshake unanswered, QCMD_STARTUP
    ZR_STARTUP_NOREPLY2 = -2,   // and again at the QCMD_STARTUPTEST retry
    ZR_RATBOT           = -3,   // ratbotDetected()
    ZR_RATBOT_NAME      = -4,   // ratbot name test
    ZR_TIMESCALE        = -5,   // timescaleDetected()
    ZR_PROXY            = -6,   // Nitro2 / BW-proxy / Xania
    ZR_PITCHSPEED       = -7,   // cl_pitchspeed changed
    ZR_HACK             = -8,   // hackDetected(), no more specific reason
    ZR_ANGLESPEEDKEY    = -9,   // cl_anglespeedkey changed
    ZR_USERINFO_SKON    = -14,  // \skon\ in userinfo, the zgh_frk signature
} zbotreason_t;

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
    LT_CLIENTVERSION,
    LT_SIGNALRAISED,
    LT_SIGNALCLEARED,
    LT_CLOUDCONNECT,
    LT_CLOUDDISCONNECT,
    LT_CLOUDERROR,
};

void clearlogfileRun(int startarg, edict_t *ent, int client);
void convertToLogLine(char *dest, char *format, int client, edict_t *ent, char *message, int number, float number2);
void displayLogEventListCont(edict_t *ent, int client, long logevent, bool onetimeonly);
void displayLogFileCont(edict_t *ent, int client, long logfilereadpos);
void displayLogFileListCont(edict_t *ent, int client, long logfilenum);
void displaylogfileRun(int startarg, edict_t *ent, int client);
void expandOutPortNum(char *srcdest, int max);
void flushLogs(void);
void flushLogsRun(int startarg, edict_t *ent, int client);
bool isLogEvent(enum zb_logtypesenum ltype);
void loadLogList(void);
bool loadLogListFile(char *filename);
void logEvent(enum zb_logtypesenum ltype, int client, edict_t *ent, char *message, int number, float ret_time, bool echo);
void logeventRun(int startarg, edict_t *ent, int client);
void logfileRun(int startarg, edict_t *ent, int client);
void openLogFiles(void);
void closeLogFiles(void);
bool isLogWritable(int index);
