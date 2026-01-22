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

logfile_t logFiles[32];

logtypes_t logtypes[] ={
    { "ZBOT", qfalse, 0, ""},
    { "ZBOTIMPULSES", qfalse, 0, ""},
    { "IMPULSES", qfalse, 0, ""},
    { "NAMECHANGE", qfalse, 0, ""},
    { "SKINCHANGE", qfalse, 0, ""},
    { "CHATBAN", qfalse, 0, ""},
    { "CLIENTCONNECT", qfalse, 0, ""},
    { "CLIENTBEGIN", qfalse, 0, ""},
    { "CLIENTDISCONNECT", qfalse, 0, ""},
    { "CLIENTKICK", qfalse, 0, ""},
    { "CLIENTCMDS", qfalse, 0, ""},
    { "CLIENTLRCON", qfalse, 0, ""},
    { "BAN", qfalse, 0, ""},
    { "CHAT", qfalse, 0, ""},
    { "SERVERSTART", qfalse, 0, ""},
    { "SERVERINIT", qfalse, 0, ""},
    { "SERVEREND", qfalse, 0, ""},
    { "INTERNALWARN", qfalse, 0, ""},
    { "PERFORMANCEMONITOR", qfalse, 0, ""},
    { "DISABLECMD", qfalse, 0, ""},
    { "ENTITYCREATE", qfalse, 0, ""},
    { "ENTITYDELETE", qfalse, 0, ""},
    { "INVALIDIP", qfalse, 0, ""},
    { "ADMINLOG", qfalse, 0, ""},
    { "CLIENTUSERINFO", qfalse, 0, ""},
    { "PRIVATELOG", qfalse, 0, ""},
};

/**
 *
 */
void expandOutPortNum(char *srcdest, int max) {
    char *org = srcdest;

    while (*srcdest && max) {
        if (*srcdest == '%' && (*(srcdest + 1) == 'p' || *(srcdest + 1) == 'P')) {
            char portnum[6];
            int len;
            unsigned int i;
            char *cp, *dp;

            Q_snprintf(portnum, sizeof(portnum), "%d", getport());
            len = q2a_strlen(portnum);

            *srcdest = portnum[0];

            if (len <= 1) {
                cp = srcdest + 2;
                dp = srcdest + 1;

                while (*cp) {
                    *dp++ = *cp++;
                }

                *dp = 0;
            } else {
                max--;
                srcdest++;

                *srcdest = portnum[1];

                if (len > 2) {
                    cp = srcdest + q2a_strlen(srcdest);
                    dp = cp + len - 2;

                    while (cp > srcdest) {
                        *dp-- = *cp--;
                    }

                    for (i = 2; i < len; i++) {
                        max--;
                        srcdest++;

                        if (!max) {
                            org[max] = 0;
                            return;
                        }

                        *srcdest = portnum[i];
                    }
                }
            }
        }

        max--;
        srcdest++;
    }

    if (max) {
        *srcdest = 0;
    }

    org[max] = 0;
}

/**
 *
 */
qboolean loadLogListFile(char *filename) {
    FILE *loglist;
    char readbuf[4096];
    unsigned int i, uptoLine = 0;
    int lognum;

    loglist = fopen(filename, "rt");
    if (!loglist) {
        return qfalse;
    }

    while (fgets(readbuf, 4096, loglist)) {
        char *cp = readbuf;

        SKIPBLANK(cp);

        uptoLine++;

        if (!(cp[0] == ';' || cp[0] == '\n' || isBlank(cp))) {
            // LOGFILE: LogNum [MOD] "LogFileName"
            if (startContains(cp, "LOGFILE:")) {
                cp += 8;

                SKIPBLANK(cp);

                lognum = q2a_atoi(cp);

                if (lognum >= 1 || lognum <= 32) {
                    lognum--;

                    while (isdigit(*cp)) {
                        cp++;
                    }

                    SKIPBLANK(cp);

                    if (startContains(cp, "MOD")) {
                        cp += 3;
                        SKIPBLANK(cp);

                        logFiles[lognum].mod = qtrue;
                    } else {
                        logFiles[lognum].mod = qfalse;
                    }

                    if (*cp == '\"') {
                        // copy filename
                        cp++;

                        cp = processstring(logFiles[lognum].filename, cp, sizeof (logFiles[lognum].filename) - 1, '\"');

                        expandOutPortNum(logFiles[lognum].filename, sizeof (logFiles[lognum].filename) - 1);

                        if (!isBlank(logFiles[lognum].filename)) {
                            logFiles[lognum].inuse = qtrue;
                        } else {
                            logFiles[lognum].inuse = qfalse;
                            gi.dprintf("Error loading LOGFILE from line %d in file %s\n", uptoLine, filename);
                        }
                    } else {
                        gi.dprintf("Error loading LOGFILE from line %d in file %s\n", uptoLine, filename);
                    }
                }
            } else {
                for (i = 0; i < LOGTYPES_MAX; i++) {
                    q2a_strncpy(buffer, logtypes[i].logtype, sizeof(buffer));
                    q2a_strcat(buffer, ":");

                    if (startContains(cp, buffer)) {
                        // [logtype]: YES/NO lognum [+ lognum [+ lognum ...]] "format"

                        cp += q2a_strlen(buffer);

                        SKIPBLANK(cp);

                        if (startContains(cp, "YES")) {
                            cp += 3;
                            SKIPBLANK(cp);

                            logtypes[i].logfiles = 0;

                            lognum = q2a_atoi(cp);
                            logtypes[i].log = qfalse;

                            if (lognum >= 1 || lognum <= 32) {
                                lognum--;

                                while (isdigit(*cp)) {
                                    cp++;
                                }

                                SKIPBLANK(cp);

                                logtypes[i].logfiles |= (0x1 << lognum);

                                while (*cp == '+') {
                                    cp++;
                                    SKIPBLANK(cp);

                                    lognum = q2a_atoi(cp);

                                    if (lognum >= 1 || lognum <= 32) {
                                        lognum--;

                                        while (isdigit(*cp)) {
                                            cp++;
                                        }

                                        SKIPBLANK(cp);

                                        logtypes[i].logfiles |= (0x1 << lognum);
                                    } else {
                                        break;
                                    }
                                }

                                if (*cp == '\"') {
                                    cp++;

                                    // copy format
                                    cp = processstring(logtypes[i].format, cp, sizeof (logtypes[i].format) - 1, '\"');

                                    if (!isBlank(logtypes[i].format)) {
                                        logtypes[i].log = qtrue;
                                    } else {
                                        gi.dprintf("Error loading LOGTYPE from line %d in file %s\n", uptoLine, filename);
                                    }
                                } else {
                                    gi.dprintf("Error loading LOGTYPE from line %d in file %s\n", uptoLine, filename);
                                }
                            }
                        } else if (startContains(cp, "NO")) {
                            logtypes[i].log = qfalse;
                        }

                        break;
                    }
                }

                if (i >= LOGTYPES_MAX) {
                    gi.dprintf("Error loading LOGTYPE from line %d in file %s\n", uptoLine, filename);
                }
            }
        }
    }
    fclose(loglist);
    return qtrue;
}

/**
 *
 */
void loadLogList(void) {
    unsigned int i;
    qboolean ret;

    q2a_memset(logFiles, 0x0, sizeof (logFiles));
    for (i = 0; i < LOGTYPES_MAX; i++) {
        logtypes[i].log = qfalse;
    }
    ret = loadLogListFile(configfile_log->string);
    Q_snprintf(buffer, sizeof(buffer), "%s/%s", moddir, configfile_log->string);
    if (loadLogListFile(buffer)) {
        ret = qtrue;
    }
    if (!ret) {
       gi.cprintf(NULL, PRINT_HIGH, "WARNING: %s could not be found\n", configfile_log->string);
    }
}

/**
 * Substitute any macro characters for their value and write the final log line
 * to the `dest` pointer.
 *
 * Macros:
 *   #n = client name
 *   #p = client ping
 *   #i = client IP address
 *   #r = client rate
 *   #s = client skin
 *   #t = current date/time (long format)
 *   #T = current date/time (short format (YYYYMMDDhhmmss))
 *   #e = impulse number, hack detected type, internal warning
 *   #f = function complete time (performance monitoring only)
 *   #m = message
 *
 * Context->value for #m:
 *   impulses: the impulse message
 *   name change: the old name
 *   skin change: the old skin
 *   kick: the kick msg
 *   chat: the banned chat msg
 *   performance: the function name being performance tested
 *   disabled command: the command attempted
 *   entity creation/deletion: the classname of the edict_t
 *   clientuserinfo change: the new userinfo string
 *
 * Hack detection types for #e
 *   50 to -2 = zbot detected
 *   -3 to -4 = ratbot detected
 *   -5       = timescale (speed)cheat detected
 *   -6       = Nitro2 / BW-proxy / Xania proxy detected
 *   -7       = cl_pitchspeed change detected
 *   -8       = generic hack detected
 *   -9       = cl_anglespeedkey change detected
 */
void convertToLogLine(char *dest, char *format, int client, edict_t *ent, char *message, int number, float number2) {
    char *cp;
    time_t ltimetemp;
	struct tm *timeinfo;

    while (*format) {
        if (*format == '#') {
            format++;

            if (*format == 'n') {
                if (ent) {
                    cp = proxyinfo[client].name;
                    while (*cp) {
                        *dest++ = *cp++;
                    }
                }
            } else if (*format == 'p') {
                if (ent) {
                    Q_snprintf(dest, sizeof(dest), "%d", ent->client->ping);
                    while (*dest) {
                        dest++;
                    }
                }
            } else if (*format == 'i') {
                if (ent) {
                    cp = IP(client);
                    while (*cp) {
                        *dest++ = *cp++;
                    }
                }
            } else if (*format == 'r') {
                if (ent) {
                    Q_snprintf(dest, sizeof(dest), "%d", proxyinfo[client].rate);
                    while (*dest) {
                        dest++;
                    }
                }
            } else if (*format == 's') {
                if (ent) {
                    cp = proxyinfo[client].skin;
                    while (*cp) {
                        *dest++ = *cp++;
                    }
                }
            } else if (*format == 't') {	// long format
                time(&ltimetemp);
                q2a_strncpy(buffer, ctime(&ltimetemp), sizeof(buffer));

                cp = buffer;
                while (*cp && *cp != '\n') {
                    *dest++ = *cp++;
                }
            } else if (*format == 'T') {	// short format
                time(&ltimetemp);
                timeinfo = localtime(&ltimetemp);
                strftime(buffer,15,"%Y%m%d%H%M%S", timeinfo);

                cp = buffer;
                while (*cp && *cp != '\n') {
                    *dest++ = *cp++;
                }
            } else if (*format == 'm') {
                if (message) {
                    cp = message;
                    while (*cp) {
                        if (*cp != '\n') {
                            *dest++ = *cp++;
                        } else {
                            cp++;
                        }
                    }
                }
            } else if (*format == 'e') {
                Q_snprintf(dest, sizeof(dest), "%d", number);
                while (*dest) {
                    dest++;
                }
            } else if (*format == 'f') {
                Q_snprintf(dest, sizeof(dest), "%g", number2);
                while (*dest) {
                    dest++;
                }
            } else {
                *dest++ = '#';
                if (*format) {
                    *dest++ = *format;
                }
            }

            if (*format) {
                format++;
            }
        } else {
            *dest++ = *format++;
        }
    }
    *dest = 0;
}

/**
 *
 */
qboolean isLogEvent(enum zb_logtypesenum ltype) {
    return logtypes[(int) ltype].log;
}

/**
 *
 */
void logEvent(enum zb_logtypesenum ltype, int client, edict_t *ent, char *message, int number, float number2) {
    if (logtypes[(int) ltype].log) {
        char logline[4096];
        char logname[356];
        unsigned long logfile;
        unsigned int i;
        FILE *logfilePtr;

        // prepare log line.
        convertToLogLine(logline, logtypes[(int) ltype].format, client, ent, message, number, number2);

        for (i = 0, logfile = 0x1; i < 32; i++, logfile <<= 1) {
            if ((logtypes[(int) ltype].logfiles & logfile) && logFiles[i].inuse) {
                if (logFiles[i].mod) {
                    Q_snprintf(logname, sizeof(logname), "%s/%s", moddir, logFiles[i].filename);
                } else {
                    q2a_strncpy(logname, logFiles[i].filename, sizeof(logname));
                }

                logfilePtr = fopen(logname, "at");

                if (logfilePtr) {
                    fprintf(logfilePtr, "%s\n", logline);
                    fclose(logfilePtr);
                }
            }
        }
    }
}

/**
 *
 */
void displaylogfileRun(int startarg, edict_t *ent, int client) {
    int logToDisplay = 0;

    if (gi.argc() > startarg) {
        logToDisplay = q2a_atoi(gi.argv(startarg));
    }

    if (logToDisplay >= 1 && logToDisplay <= 32) {
        proxyinfo[client].logfilenum = logToDisplay - 1;
        //   proxyinfo[client].logfilereadpos = 0;

        if (logFiles[proxyinfo[client].logfilenum].inuse) {
            gi.cprintf(ent, PRINT_HIGH, "Start Logfile %d (%s)\n", logToDisplay, logFiles[proxyinfo[client].logfilenum].filename);
            addCmdQueue(client, QCMD_DISPLOGFILE, 0, 0, 0);
        } else {
            gi.cprintf(ent, PRINT_HIGH, "Log file %d not in use.\n", logToDisplay);
        }
    } else {
        gi.cprintf(ent, PRINT_HIGH, "[sv] !displaylogfile logfilenum(1-32)\n");
    }
}

/**
 *
 */
void displayLogFileCont(edict_t *ent, int client, long logfilereadpos) {
    int logNum = proxyinfo[client].logfilenum;
    char logname[356];
    char logline[4096];
    FILE *logfilePtr;

    if (logFiles[logNum].mod) {
        Q_snprintf(logname, sizeof(logname), "%s/%s", moddir, logFiles[logNum].filename);
    } else {
        q2a_strncpy(logname, logFiles[logNum].filename, sizeof(logname));
    }

    logfilePtr = fopen(logname, "rt");

    if (logfilePtr) {
        fseek(logfilePtr, logfilereadpos, SEEK_SET);

        if (fgets(logline, 4096, logfilePtr) != NULL) {
            gi.cprintf(ent, PRINT_HIGH, "%s", logline);
            logfilereadpos = ftell(logfilePtr);
            addCmdQueue(client, QCMD_DISPLOGFILE, 0, logfilereadpos, 0);
        } else {
            gi.cprintf(ent, PRINT_HIGH, "End Logfile %d (%s)\n", logNum + 1, logFiles[logNum].filename);
        }

        fprintf(logfilePtr, "%s\n", logline);
        fclose(logfilePtr);
    } else {
        gi.cprintf(ent, PRINT_HIGH, "End Logfile %d (%s)\n", logNum + 1, logFiles[logNum].filename);
    }
}

/**
 *
 */
void clearlogfileRun(int startarg, edict_t *ent, int client) {
    int logToDisplay = 0;

    if (gi.argc() > startarg) {
        logToDisplay = q2a_atoi(gi.argv(startarg));
    }

    if (logToDisplay >= 1 && logToDisplay <= 32) {
        logToDisplay--;

        if (logFiles[logToDisplay].inuse) {
            char logname[356];
            FILE *logfilePtr;

            if (logFiles[logToDisplay].mod) {
                Q_snprintf(logname, sizeof(logname), "%s/%s", moddir, logFiles[logToDisplay].filename);
            } else {
                q2a_strncpy(logname, logFiles[logToDisplay].filename, sizeof(logname));
            }

            logfilePtr = fopen(logname, "w+t");
            if (!logfilePtr) {
                gi.cprintf(ent, PRINT_HIGH, "logfilename \"%s\" couldn't be opened!\n", logFiles[logToDisplay].filename);
            } else {
                fclose(logfilePtr);
                gi.cprintf(ent, PRINT_HIGH, "Log file %d (%s) cleared\n", logToDisplay + 1, logFiles[logToDisplay].filename);
            }
        } else {
            gi.cprintf(ent, PRINT_HIGH, "Log file %d not in use.\n", logToDisplay + 1);
        }
    } else {
        gi.cprintf(ent, PRINT_HIGH, "[sv] !clearlogfile logfilenum(1-32)\n");
    }
}

/**
 *
 */
void logfileRun(int startarg, edict_t *ent, int client) {
    char *cmd;
    int logfilenum;
    char filename[256];
    int mod;

    if (gi.argc() <= startarg) {
        gi.cprintf(ent, PRINT_HIGH, LOGFILECMD);
        return;
    }

    cmd = gi.argv(startarg);

    if (Q_stricmp(cmd, "VIEW") == 0) {
        gi.cprintf(ent, PRINT_HIGH, "Start Logfile List\n\nFileNum  Mod  Filename\n");

        if (gi.argc() > startarg + 1) {
            logfilenum = q2a_atoi(gi.argv(startarg + 1));

            if (logfilenum > 0 && logfilenum <= 32 && logFiles[logfilenum - 1].inuse) {
                logfilenum--;
                gi.cprintf(ent, PRINT_HIGH, "  %3d    %s  %s\n", logfilenum + 1, logFiles[logfilenum].mod ? "Yes" : " No", logFiles[logfilenum].filename);
            }

            gi.cprintf(ent, PRINT_HIGH, "\nEnd Logfile List\n");
        } else {
            for (logfilenum = 0; logfilenum < 32; logfilenum++) {
                if (logFiles[logfilenum].inuse) {
                    break;
                }
            }

            if (logfilenum < 32) {
                addCmdQueue(client, QCMD_DISPLOGFILELIST, 0, logfilenum, 0);
            } else {
                gi.cprintf(ent, PRINT_HIGH, "\nEnd Logfile List\n");
            }
        }
    } else if (Q_stricmp(cmd, "EDIT") == 0) {
        logfilenum = q2a_atoi(gi.argv(startarg + 1));

        if (logfilenum < 1 || logfilenum > 32) {
            gi.cprintf(ent, PRINT_HIGH, LOGFILECMD);
            return;
        }

        logfilenum--;

        cmd = gi.argv(startarg + 2);
        mod = startContains(cmd, "mod");

        processstring(filename, mod ? gi.argv(startarg + 3) : cmd, sizeof (filename) - 1, 0);

        if (!isBlank(filename)) {
            logFiles[logfilenum].mod = mod;
            q2a_strncpy(logFiles[logfilenum].filename, filename, sizeof(logFiles[logfilenum].filename));
            logFiles[logfilenum].inuse = qtrue;
            gi.cprintf(ent, PRINT_HIGH, "Log file Added!\n");
        } else {
            gi.cprintf(ent, PRINT_HIGH, LOGFILECMD);
            return;
        }


    } else if (Q_stricmp(cmd, "DEL") == 0) {
        logfilenum = q2a_atoi(gi.argv(startarg + 1));

        if (logfilenum < 1 || logfilenum > 32) {
            gi.cprintf(ent, PRINT_HIGH, LOGFILECMD);
            return;
        }

        logfilenum--;

        if (!logFiles[logfilenum].inuse) {
            gi.cprintf(ent, PRINT_HIGH, "Log file not in use!\n");
        } else {
            logFiles[logfilenum].inuse = qfalse;
            gi.cprintf(ent, PRINT_HIGH, "Log file turned off!\n");
        }
    } else {
        gi.cprintf(ent, PRINT_HIGH, LOGFILECMD);
    }
}

/**
 *
 */
void displayLogFileListCont(edict_t *ent, int client, long logfilenum) {
    gi.cprintf(ent, PRINT_HIGH, "  %3d    %s  %s\n", logfilenum + 1, logFiles[logfilenum].mod ? "Yes" : " No", logFiles[logfilenum].filename);
    for (logfilenum++; logfilenum < 32; logfilenum++) {
        if (logFiles[logfilenum].inuse) {
            break;
        }
    }
    if (logfilenum < 32) {
        addCmdQueue(client, QCMD_DISPLOGFILELIST, 0, logfilenum, 0);
    } else {
        gi.cprintf(ent, PRINT_HIGH, "\nEnd Logfile List\n");
    }
}

/**
 *
 */
void logeventRun(int startarg, edict_t *ent, int client) {
    char *cmd;
    char *lt;
    unsigned int i;
    int argi;
    qboolean log;
    unsigned long logfiles;
    unsigned long lognum;
    char format[4096];

    if (gi.argc() <= startarg) {
        gi.cprintf(ent, PRINT_HIGH, LOGEVENTCMD);
        return;
    }

    cmd = gi.argv(startarg);

    if (Q_stricmp(cmd, "VIEW") == 0) {
        if (gi.argc() > startarg + 1) {
            lt = gi.argv(startarg + 1);

            gi.cprintf(ent, PRINT_HIGH, "Start Logevent List\n\nLogevent             Log  LogFiles      Format\n");

            for (i = 0; i < LOGTYPES_MAX; i++) {
                if (startContains(logtypes[i].logtype, lt)) {
                    displayLogEventListCont(ent, client, i, qtrue);
                }
            }

            gi.cprintf(ent, PRINT_HIGH, "\nEnd Logevent List\n");
        } else {
            gi.cprintf(ent, PRINT_HIGH, "Start Logevent List\n\nLogevent             Log  LogFiles      Format\n");
            addCmdQueue(client, QCMD_DISPLOGEVENTLIST, 0, 0, 0);
        }
    } else if (Q_stricmp(cmd, "EDIT") == 0) {
        if (gi.argc() > startarg + 1) {
            lt = gi.argv(startarg + 1);

            for (i = 0; i < LOGTYPES_MAX; i++) {
                if (Q_stricmp(logtypes[i].logtype, lt) == 0) {
                    break;
                }
            }

            if (i >= LOGTYPES_MAX) {
                gi.cprintf(ent, PRINT_HIGH, LOGEVENTCMD);
                return;
            }

            log = logtypes[i].log;
            logfiles = logtypes[i].logfiles;
            q2a_strncpy(format, logtypes[i].format, sizeof(format));

            for (argi = startarg + 2; gi.argc() > argi; argi++) {
                cmd = gi.argv(argi);

                if (Q_stricmp(cmd, "LOG") == 0) {
                    argi++;
                    if (gi.argc() <= argi) {
                        gi.cprintf(ent, PRINT_HIGH, LOGEVENTCMD);
                        return;
                    }

                    cmd = gi.argv(argi);

                    if (startContains(cmd, "YES")) {
                        log = qtrue;
                    } else {
                        log = qfalse;
                    }

                } else if (Q_stricmp(cmd, "LOGFILES") == 0) {
                    argi++;
                    if (gi.argc() <= argi) {
                        gi.cprintf(ent, PRINT_HIGH, LOGEVENTCMD);
                        return;
                    }

                    cmd = gi.argv(argi);

                    lognum = q2a_atoi(cmd);

                    if (lognum >= 1 || lognum <= 32) {
                        lognum--;

                        while (isdigit(*cmd)) {
                            cmd++;
                        }

                        SKIPBLANK(cmd);

                        logfiles = (0x1 << lognum);

                        while (*cmd == '+') {
                            cmd++;
                            SKIPBLANK(cmd);

                            if (*cmd == 0) {
                                break;
                            }

                            lognum = q2a_atoi(cmd);

                            if (lognum >= 1 || lognum <= 32) {
                                lognum--;

                                while (isdigit(*cmd)) {
                                    cmd++;
                                }

                                SKIPBLANK(cmd);

                                logfiles |= (0x1 << lognum);
                            } else {
                                gi.cprintf(ent, PRINT_HIGH, LOGEVENTCMD);
                                return;
                            }
                        }
                    } else {
                        gi.cprintf(ent, PRINT_HIGH, LOGEVENTCMD);
                        return;
                    }
                } else if (Q_stricmp(cmd, "FORMAT") == 0) {
                    argi++;
                    if (gi.argc() <= argi) {
                        gi.cprintf(ent, PRINT_HIGH, LOGEVENTCMD);
                        return;
                    }

                    cmd = gi.argv(argi);

                    processstring(format, cmd, sizeof (format) - 1, 0);

                    if (isBlank(format)) {
                        gi.cprintf(ent, PRINT_HIGH, LOGEVENTCMD);
                        return;
                    }
                } else {
                    gi.cprintf(ent, PRINT_HIGH, LOGEVENTCMD);
                    return;
                }
            }

            logtypes[i].log = log;
            logtypes[i].logfiles = logfiles;
            q2a_strcpy(logtypes[i].format, format);

            displayLogEventListCont(ent, client, i, qtrue);
        } else {
            gi.cprintf(ent, PRINT_HIGH, LOGEVENTCMD);
        }
    } else {
        gi.cprintf(ent, PRINT_HIGH, LOGEVENTCMD);
    }
}

/**
 *
 */
void displayLogEventListCont(edict_t *ent, int client, long logevent, qboolean onetimeonly) {
	unsigned long logfile;
    unsigned int i;

    q2a_strcpy(buffer, "  ");

    for (i = 0, logfile = 0x1; i < 32; i++, logfile <<= 1) {
        if ((logtypes[logevent].logfiles & logfile)) {
            if (!logFiles[i].inuse) {
                q2a_strcat(buffer, "(");
            }

            Q_snprintf(buffer + q2a_strlen(buffer), sizeof(buffer) - q2a_strlen(buffer), "%d", i + 1);

            if (!logFiles[i].inuse) {
                q2a_strcat(buffer, ")");
            }

            q2a_strcat(buffer, " ");
        }
    }

    gi.cprintf(ent, PRINT_HIGH, "%-20s %s %s \"%s\"\n", logtypes[logevent].logtype, logtypes[logevent].log ? "Yes" : " No", buffer, logtypes[logevent].format);

    if (onetimeonly) {
        return;
    }

    logevent++;
    if (logevent < LOGTYPES_MAX) {
        addCmdQueue(client, QCMD_DISPLOGEVENTLIST, 0, logevent, 0);
    } else {
        gi.cprintf(ent, PRINT_HIGH, "\nEnd Logevent List\n");
    }
}
