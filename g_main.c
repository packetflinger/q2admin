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

//
// q2admin
//
// g_main.c
//
// copyright 2000 Shane Powell
//

#include "g_local.h"

#if defined(_WIN32) || defined(_WIN64)
HINSTANCE hdll;
#else
void *hdll = NULL;
#endif

typedef game_export_t *GAMEAPI(game_import_t *import);

char zbot_teststring1[] = ZBOT_TESTSTRING1;
char zbot_teststring_test1[] = ZBOT_TESTSTRING_TEST1;
char zbot_teststring_test2[] = ZBOT_TESTSTRING_TEST2;
char zbot_teststring_test3[] = ZBOT_TESTSTRING_TEST3;
char zbot_testchar1;
char zbot_testchar2;

qboolean soloadlazy;

char moddir[256];

void ShutdownGame(void) {

    profile_init(1);
    profile_init(2);

    if (!dllloaded) return;

    if (whois_active) {
        whois_write_file();
        gi.TagFree(whois_details);
    }

    if (q2adminrunmode) {
        profile_start(1);
        logEvent(LT_SERVEREND, 0, NULL, NULL, 0, 0.0);
        profile_start(2);
    }

    gi.TagFree(finalentities);

    CA_Shutdown();

    // reset the password just in case something has gone wrong...
    lrcon_reset_rcon_password(0, 0, 0);
    ge_mod->Shutdown();

    if (q2adminrunmode) {
        profile_stop(2, "mod->ShutdownGame", 0, NULL);
    }

#if (defined(_WIN32) || defined(_WIN64))
    FreeLibrary(hdll);
#else
    dlclose(hdll);
#endif

    dllloaded = qfalse;

    if (q2adminrunmode) {
        profile_stop(1, "q2admin->ShutdownGame", 0, NULL);
    }
}

void G_RunFrame(void) {
    unsigned int j, required_cmdlist;

    int maxdoclients;
    static int client = -1;
    edict_t *ent;
    byte command;
    unsigned long data;
    char *str;
    char checkConnectProxy[RANDOM_STRING_LENGTH + 1];
    char ReconnectString[RANDOM_STRING_LENGTH + 1];
    char rndConnectString[RANDOM_STRING_LENGTH + 1];

    profile_init_2(1);
    profile_init_2(2);

    if (!dllloaded) return;

    if (q2adminrunmode == 0) {
        ge_mod->RunFrame();
        G_MergeEdicts();
        return;
    }

    profile_start(1);

    lframenum++;
    ltime = lframenum * FRAMETIME;

    if (serverinfoenable && (lframenum > 10)) {
        Q_snprintf(buffer, sizeof(buffer), "set q2admin \"%s\" s\n", version);
        gi.AddCommandString(buffer);
        serverinfoenable = 0;
    }

    // check if a lrcon password has timed out
    check_lrcon_password();

    if (maxReconnectList) {
        unsigned int i;

        for (i = 0; i < maxReconnectList; i++) {
            if (reconnectlist[i].reconnecttimeout < ltime) {
                unsigned int j;

                // remove the retry list entry if needed...
                for (j = 0; j < maxReconnectList; j++) {
                    if ((j != i) && (reconnectlist[j].retrylistidx == reconnectlist[i].retrylistidx)) {
                        break;
                    }
                }

                if (j >= maxReconnectList) {
                    if ((reconnectlist[i].retrylistidx + 1) < maxretryList) {
                        q2a_memmove(&(retrylist[reconnectlist[i].retrylistidx]), &(retrylist[reconnectlist[i].retrylistidx + 1]), (maxretryList - (reconnectlist[i].retrylistidx + 1)) * sizeof (retrylist_info));
                    }
                    maxretryList--;
                }

                if ((i + 1) < maxReconnectList) {
                    q2a_memmove(&(reconnectlist[i]), &(reconnectlist[i + 1]), (maxReconnectList - (i + 1)) * sizeof (reconnect_info));
                    i--;
                }
                maxReconnectList--;
            }
        }
    }

    if (framesperprocess && ((lframenum % framesperprocess) != 0)) {
        ge_mod->RunFrame();
        G_MergeEdicts();
        return;
    }

    maxdoclients = client;
    maxdoclients += maxclientsperframe;

    if (maxdoclients > maxclients->value) {
        maxdoclients = maxclients->value;
    }

    for (; client < maxdoclients; client++) {
        if (client < 0) {
            ent = NULL;
        } else {
            ent = getEnt((client + 1));
        }

        if (timers_active) {
            timer_action(client, ent);
		}

        if (getCommandFromQueue(client, &command, &data, &str)) {
            if (!proxyinfo[client].inuse) {
                if (command == QCMD_STARTUP) {
                    addCmdQueue(client, QCMD_STARTUPTEST, 2, 0, 0);
                    proxyinfo[client].clientcommand |= CCMD_STARTUPTEST;
                } else if (command == QCMD_STARTUPTEST) {

                    addCmdQueue(client, QCMD_EXECMAPCFG, 5, 0, 0);
                    if (do_franck_check) {
                        stuffcmd(ent, "riconnect; roconnect; connect; set frkq2 disconnect; set quake2frk disconnect; set q2frk disconnect\n");
                    }

                    if (do_vid_restart) {
                        if (!proxyinfo[client].vid_restart) {
                            proxyinfo[client].vid_restart = qtrue;
                            stuffcmd(ent, "vid_restart\n");
                        }
                    }

                    addCmdQueue(client, QCMD_SHOWMOTD, 0, 0, 0);

                    if (proxyinfo[client].clientcommand & CCMD_ZBOTDETECTED) {
                        break;
                    }

                    proxyinfo[client].inuse = 1;

                    if (proxyinfo[client].retries > MAXSTARTTRY) {
                        if (zbotdetect) {
                            serverLogZBot(ent, client);
                            proxyinfo[client].clientcommand &= ~CCMD_STARTUPTEST;
                            proxyinfo[client].clientcommand |= (CCMD_ZBOTDETECTED | CCMD_ZPROXYCHECK2);
                            addCmdQueue(client, QCMD_ZPROXYCHECK2, 1, IW_STARTUP, 0);
                            proxyinfo[client].charindex = -1;
                            logEvent(LT_INTERNALWARN, client, ent, "Startup Init Fail", IW_STARTUPFAIL, 0.0);
                        }
                        break;
                    }

                    stuffcmd(ent, zbot_teststring1);
                    addCmdQueue(client, QCMD_STARTUPTEST, 5, 0, 0);
                    proxyinfo[client].retries++;
                } else if ((command == QCMD_DISCONNECT) || (command == QCMD_KICK)) {
                    //stuffcmd(ent, "disconnect\n");
                    proxyinfo[client].clientcommand |= CCMD_KICKED;
                    logEvent(LT_CLIENTKICK, client, ent, str, 0, 0.0);
                    gi.cprintf(ent, PRINT_HIGH, "You have been kicked %s\n", proxyinfo[client].name);
                    Q_snprintf(buffer, sizeof(buffer), "\nkick %d\n", client);
                    gi.AddCommandString(buffer);
                } else if (command == QCMD_RECONNECT) {
                    unsigned int i;
                    char ipbuffer[40];
                    char *ip = ipbuffer;
                    char *bp = ip;

                    q2a_strncpy(ipbuffer, IP(client), sizeof(ipbuffer));

                    while (*bp && (*bp != ':')) {
                        bp++;
                    }

                    *bp = 0;

                    if (*ip) {
                        q2a_strcpy(reconnectlist[maxReconnectList].userinfo, proxyinfo[client].userinfo);
                        reconnectlist[maxReconnectList].reconnecttimeout = ltime;
                        reconnectlist[maxReconnectList].reconnecttimeout += reconnect_time;

                        // check for retry entry..
                        for (i = 0; i < maxretryList; i++) {
                            if (q2a_strcmp(retrylist[i].ip, ip) == 0) {
                                break;
                            }
                        }

                        if (i < maxretryList) {
                            if (retrylist[i].retry >= 5) {
                                unsigned int j;

                                // remove the retry list entry if needed...
                                for (j = 0; j < maxReconnectList; j++) {
                                    if (reconnectlist[j].retrylistidx == i) {
                                        break;
                                    }
                                }
                                if (j >= maxReconnectList) {
                                    if ((i + 1) < maxretryList) {
                                        q2a_memmove(&(retrylist[i]), &(retrylist[i + 1]), (maxretryList - (i + 1)) * sizeof (retrylist_info));
                                    }
                                    maxretryList--;
                                }

                                // cut off here...
                                Q_snprintf(buffer, sizeof(buffer), "\ndisconnect\n");
                                stuffcmd(ent, buffer);
                                break;
                            }

                            retrylist[i].retry++;
                            reconnectlist[maxReconnectList].retrylistidx = i;
                        } else {
                            q2a_strncpy(retrylist[maxretryList].ip, ip, MAX_INFO_STRING + 45);
                            retrylist[maxretryList].retry = 0;
                            maxretryList++;
                        }

                        maxReconnectList++;
                    }

                    q2a_strncpy(buffer, ("%s\n", defaultreconnectmessage), sizeof(buffer));
                    gi.cprintf(ent, PRINT_HIGH, buffer);

                    generateRandomString(ReconnectString, 5);
                    generateRandomString(rndConnectString, 5);
                    Q_snprintf(
                        buffer,
                        sizeof(buffer),
                        "\nset %s %s\nset %s connect\n",
                        ReconnectString,
                        reconnect_address,
                        rndConnectString
                    );
                    stuffcmd(ent, buffer);

                    generateRandomString(proxyinfo[client].hack_teststring3, RANDOM_STRING_LENGTH);
                    generateRandomString(checkConnectProxy, RANDOM_STRING_LENGTH);

                    Q_snprintf(
                        buffer,
                        sizeof(buffer),
                        "\nalias connect %s\nalias %s $%s $%s\n%s\n",
                        proxyinfo[client].hack_teststring3,
                        checkConnectProxy,
                        rndConnectString,
                        ReconnectString,
                        checkConnectProxy
                    );

                    proxyinfo[client].clientcommand |= CCMD_WAITFORCONNECTREPLY;
                    stuffcmd(ent, buffer);
                    //addCmdQueue(client, QCMD_KICK, 0, 0, NULL);
                } else {
                    // add command to back of line for processing later..
                    addCmdQueue(client, command, 0, data, str);
                }
            } else if (command == QCMD_STARTUPTEST) {
                if (proxyinfo[client].clientcommand & CCMD_ZBOTDETECTED) {
                    break;
                }

                if (proxyinfo[client].retries > MAXSTARTTRY) {
                    if (zbotdetect) {
                        serverLogZBot(ent, client);
                        proxyinfo[client].clientcommand &= ~CCMD_STARTUPTEST;
                        proxyinfo[client].clientcommand |= (CCMD_ZBOTDETECTED | CCMD_ZPROXYCHECK2);
                        addCmdQueue(client, QCMD_ZPROXYCHECK2, 1, IW_STARTUPTEST, 0);
                        proxyinfo[client].charindex = -2;
                        logEvent(LT_INTERNALWARN, client, ent, "Startup Init Fail 2", IW_STARTUPFAIL, 0.0);
                        break;
                    }
                }

                stuffcmd(ent, zbot_teststring1);
                addCmdQueue(client, QCMD_STARTUPTEST, 5, 0, 0);
                proxyinfo[client].retries++;
            } else if (command == QCMD_LETRATBOTQUIT) {
                if (zbotdetect) {
                    Q_snprintf(buffer, sizeof(buffer), "\n%s\n", zbot_teststring_test3);
                    stuffcmd(ent, buffer);
                    stuffcmd(ent, buffer);
                    stuffcmd(ent, buffer);
                }
            } else if (command == QCMD_RESTART) {
                if (zbotdetect) {
                    if (!(proxyinfo[client].clientcommand & CCMD_ZBOTCLEAR)) {
                        addCmdQueue(client, QCMD_ZPROXYCHECK1, 0, 0, 0); // retry check for proxy
                    }
                }
            } else if (command == QCMD_ZPROXYCHECK1) {
                // are we at the end?
                if (proxyinfo[client].charindex >= testcharslength) {
                    break;
                }

                // begin test for proxies
                proxyinfo[client].teststr[0] = testchars[proxyinfo[client].charindex];
                proxyinfo[client].teststr[1] = BOTDETECT_CHAR1;
                proxyinfo[client].teststr[2] = BOTDETECT_CHAR2;
                proxyinfo[client].teststr[3] = zbot_testchar1;
                proxyinfo[client].teststr[4] = zbot_testchar2;
                proxyinfo[client].teststr[5] = RANDCHAR();
                proxyinfo[client].teststr[6] = RANDCHAR();
                proxyinfo[client].teststr[7] = 0;
                proxyinfo[client].teststr[8] = 0;

                Q_snprintf(buffer, sizeof(buffer), "\n%s\n%s\n", proxyinfo[client].teststr, zbot_teststring_test2);
                stuffcmd(ent, buffer);

                proxyinfo[client].clientcommand |= CCMD_ZPROXYCHECK2;
                addCmdQueue(client, QCMD_ZPROXYCHECK2, clientsidetimeout, IW_ZBOTTEST, 0);
            } else if (command == QCMD_ZPROXYCHECK2) // are we dealing with a proxy???
            {
                char text[35];

                if (!(proxyinfo[client].clientcommand & CCMD_ZPROXYCHECK2)) {
                    Q_snprintf(text, sizeof(text), "I(%d) Exp(%s)", proxyinfo[client].charindex, proxyinfo[client].teststr);
                    logEvent(LT_INTERNALWARN, client, ent, text, data, 0.0);
                    break;
                }

                if (proxyinfo[client].charindex >= testcharslength) {
                    Q_snprintf(text, sizeof(text), "I(%d >= end) Exp(%s)", proxyinfo[client].charindex, proxyinfo[client].teststr);
                    logEvent(LT_INTERNALWARN, client, ent, text, data, 0.0);
                    break;
                }

                // yes...  detected by long timeout or the normal timeout on detect
                if (!(proxyinfo[client].clientcommand & CCMD_ZBOTDETECTED)) {
                    if (checkForOverflows(ent, client)) {
                        break;
                    }

                    if (proxyinfo[client].retries < MAXDETECTRETRIES) {
                        // try and get "unknown command" off the screen as fast as possible
                        proxyinfo[client].clientcommand &= ~CCMD_ZPROXYCHECK2;
                        addCmdQueue(client, QCMD_CLEAR, 0, 0, 0);
                        addCmdQueue(client, QCMD_RESTART, 2 + (3 * random()), 0, 0);
                        proxyinfo[client].retries++;
                        break;
                    }

                    serverLogZBot(ent, client);
                }

                proxyinfo[client].clientcommand &= ~CCMD_ZPROXYCHECK2;
                proxyinfo[client].clientcommand |= CCMD_ZBOTDETECTED;

                if (displayzbotuser) {
                    unsigned int i;

                    q2a_strncpy(buffer, ("%s\n", zbotuserdisplay), sizeof(buffer));

                    for (i = 0; i < numofdisplays; i++) {
                        gi.bprintf(PRINT_HIGH, buffer, proxyinfo[client].name);
                    }
                }

                if (customClientCmd[0]) {
                    addCmdQueue(client, QCMD_CUSTOM, 0, 0, 0);
                }

                if (disconnectuser) {
                    addCmdQueue(client, QCMD_DISCONNECT, 1, 0, zbotuserdisplay);
                }
            }
            else if (command == QCMD_TESTSTANDARDPROXY) {
                if (private_commands[0].command[0]) {
                    addCmdQueue(client, QCMD_PRIVATECOMMAND, 10, 0, 0);
                    stuff_private_commands(client, ent);
                }

                if (!proxyinfo[client].q2a_bypass) {
                    if (dopversion) {
                        /*if (client_check > 0)
                        {
                            addCmdQueue(client, QCMD_PMODVERTIMEOUT, 0, 0, 0);
                            gi.cprintf(ent, PRINT_HIGH, "%s: p_version Standard Proxy Test\r\n", proxyinfo[client].name);
                        }*/
                    }
                }
            }
            else if (command == QCMD_TESTRATBOT) {
                gi.cprintf(ent, PRINT_HIGH, "ratbot Detect Test ( %s )\r\n", "rbkck &%trf .disconnect");
                addCmdQueue(client, QCMD_TESTRATBOT2, clientsidetimeout, 0, 0);
                proxyinfo[client].clientcommand |= CCMD_RATBOTDETECT;
                addCmdQueue(client, QCMD_TESTRATBOT3, 2, 0, 0);
            } else if (command == QCMD_TESTRATBOT2) {
                if (!(proxyinfo[client].clientcommand & CCMD_RATBOTDETECT)) {
                    logEvent(LT_INTERNALWARN, client, ent, "RatBot detect problem", 0, 0.0);
                    break;
                }
                //proxyinfo[client].clientcommand &= ~CCMD_RATBOTDETECT;
            } else if (command == QCMD_TESTRATBOT3) {
                proxyinfo[client].clientcommand |= CCMD_RATBOTDETECTNAME;
                addCmdQueue(client, QCMD_TESTRATBOT4, clientsidetimeout, 0, 0);
                Q_snprintf(
                    buffer,
                    sizeof(buffer),
                    "\nname " RATBOT_CHANGENAMETEST ";wait;wait;name \"%s\"\n",
                    proxyinfo[client].name
                );
                stuffcmd(ent, buffer);
            } else if (command == QCMD_TESTRATBOT4) {
                if (!(proxyinfo[client].clientcommand & CCMD_RATBOTDETECTNAME)) {
                    logEvent(LT_INTERNALWARN, client, ent, "RatBot Detect 2 problem", 0, 0.0);
                    break;
                }

                // yes...  detected by long timeout or the normal timeout on detect
                if (!(proxyinfo[client].clientcommand & CCMD_ZBOTDETECTED)) {
                    if (checkForOverflows(ent, client)) {
                        addCmdQueue(client, QCMD_TESTRATBOT3, 2, 0, 0);
                        break;
                    }

                    if (proxyinfo[client].rbotretries < MAXDETECTRETRIES) {
                        //            proxyinfo[client].clientcommand &= ~CCMD_RATBOTDETECTNAME;
                        addCmdQueue(client, QCMD_TESTRATBOT3, 2 + (3 * random()), 0, 0);
                        proxyinfo[client].rbotretries++;
                        break;
                    }

                    proxyinfo[client].charindex = -4;
                    serverLogZBot(ent, client);
                }

                proxyinfo[client].clientcommand &= ~CCMD_RATBOTDETECTNAME;
                proxyinfo[client].clientcommand |= CCMD_ZBOTDETECTED;

                if (displayzbotuser) {
                    unsigned int i;

                    q2a_strncpy(buffer, ("%s\n", zbotuserdisplay), sizeof(buffer));

                    for (i = 0; i < numofdisplays; i++) {
                        gi.bprintf(PRINT_HIGH, buffer, proxyinfo[client].name);
                    }
                }

                if (customClientCmd[0]) {
                    addCmdQueue(client, QCMD_CUSTOM, 0, 0, 0);
                }

                if (disconnectuser) {
                    addCmdQueue(client, QCMD_DISCONNECT, 1, 0, zbotuserdisplay);
                }
            } else if (command == QCMD_TESTALIASCMD1) {
                generateRandomString(proxyinfo[client].hack_teststring1, RANDOM_STRING_LENGTH);
                generateRandomString(proxyinfo[client].hack_teststring2, RANDOM_STRING_LENGTH);
                Q_snprintf(buffer, sizeof(buffer), "\nalias %s %s\n", proxyinfo[client].hack_teststring1, proxyinfo[client].hack_teststring2);
                stuffcmd(ent, buffer);
                proxyinfo[client].clientcommand |= CCMD_WAITFORALIASREPLY1;
                addCmdQueue(client, QCMD_TESTALIASCMD2, 1, 0, NULL);
            } else if (command == QCMD_TESTALIASCMD2) {
                Q_snprintf(buffer, sizeof(buffer), "\n%s\n", proxyinfo[client].hack_teststring1);
                stuffcmd(ent, buffer);
                proxyinfo[client].clientcommand |= CCMD_WAITFORALIASREPLY2;
            } else if (command == QCMD_DISPLOGFILE) {
                displayLogFileCont(ent, client, data);
            } else if (command == QCMD_DISPLOGFILELIST) {
                displayLogFileListCont(ent, client, data);
            } else if (command == QCMD_DISPLOGEVENTLIST) {
                displayLogEventListCont(ent, client, data, qfalse);
            } else if (command == QCMD_GETIPALT) {
                // open logfile and read IP address from log
                readIpFromLog(client, ent);
                addCmdQueue(client, QCMD_GETIPALT, 0, 0, 0);
            } else if (command == QCMD_LOGTOFILE1) {
                logEvent(LT_ZBOT, client, ent, NULL, proxyinfo[client].charindex, 0.0);
            } else if (command == QCMD_LOGTOFILE2) {
                logEvent(LT_ZBOTIMPULSES, client, ent, impulsemessages[proxyinfo[client].impulse - 169], proxyinfo[client].impulse, 0.0);
            } else if (command == QCMD_LOGTOFILE3) {
                logEvent(LT_IMPULSES, client, ent, NULL, proxyinfo[client].impulse, 0.0);
            } else if (command == QCMD_CONNECTCMD) {
                if (customClientCmdConnect[0]) {
                    Q_snprintf(buffer, sizeof(buffer), "%s\n", customClientCmdConnect);
                    stuffcmd(ent, buffer);
                }

                if (customServerCmdConnect[0]) {
                    // copy string across to buffer, replacing %c with client number
                    char *cp = customServerCmdConnect;
                    char *dp = buffer;

                    while (*cp) {
                        if ((*cp == '%') && (tolower(*(cp + 1)) == 'c')) {
                            sprintf(dp, "%d", client);
                            dp += q2a_strlen(dp);
                            cp += 2;
                        } else {
                            *dp++ = *cp++;
                        }
                    }

                    *dp = 0x0;

                    gi.AddCommandString(buffer);
                }
            } else if (command == QCMD_CLEAR) {
                stuffcmd(ent, "clear\n");
            } else if (command == QCMD_CUSTOM) {
                if (customClientCmd[0]) {
                    Q_snprintf(buffer, sizeof(buffer), "%s\n", customClientCmd);
                    stuffcmd(ent, buffer);
                }
            } else if ((command == QCMD_DISCONNECT) || (command == QCMD_KICK)) {
                //stuffcmd(ent, "disconnect\n");
                proxyinfo[client].clientcommand |= CCMD_KICKED;
                logEvent(LT_CLIENTKICK, client, ent, str, 0, 0.0);
                gi.cprintf(ent, PRINT_HIGH, "You have been kicked %s\n", proxyinfo[client].name);
                Q_snprintf(buffer, sizeof(buffer), "\nkick %d\n", client);
                gi.AddCommandString(buffer);
            } else if (command == QCMD_RECONNECT) {
                Q_snprintf(buffer, sizeof(buffer), "\nconnect %s\n", reconnect_address);
                stuffcmd(ent, buffer);
                //        addCmdQueue(client, QCMD_KICK, 0, 0, NULL);
            } else if (command == QCMD_CLIPTOMAXRATE) {
                Q_snprintf(buffer, sizeof(buffer), "rate %d\n", maxrateallowed);
                stuffcmd(ent, buffer);
            } else if (command == QCMD_CLIPTOMINRATE) {
                Q_snprintf(buffer, sizeof(buffer), "rate %d\n", minrateallowed);
                stuffcmd(ent, buffer);
            } else if (command == QCMD_SETUPMAXFPS) {
                stuffcmd(ent, "set cl_maxfps $cl_maxfps u\n");
                addCmdQueue(client, QCMD_FORCEUDATAUPDATE, 0, 0, 0);
            } else if (command == QCMD_FORCEUDATAUPDATE) {
                if (proxyinfo[client].rate) {
                    Q_snprintf(buffer, sizeof(buffer), "set rate %d\nset rate %d\n", proxyinfo[client].rate + 1, proxyinfo[client].rate);
                    stuffcmd(ent, buffer);
                }
            } else if (command == QCMD_SETMAXFPS) {
                if (maxfpsallowed) {
                    Q_snprintf(buffer, sizeof(buffer), "cl_maxfps %d\n", maxfpsallowed);
                    stuffcmd(ent, buffer);
                }
            } else if (command == QCMD_SETMINFPS) {
                if (minfpsallowed) {
                    Q_snprintf(buffer, sizeof(buffer), "cl_maxfps %d\n", minfpsallowed);
                    stuffcmd(ent, buffer);
                }
            } else if (command == QCMD_DISPBANS) {
                displayNextBan(ent, client, data);
            } else if (command == QCMD_DISPLRCONS) {
                displayNextLRCon(ent, client, data);
            } else if (command == QCMD_DISPFLOOD) {
                displayNextFlood(ent, client, data);
            } else if (command == QCMD_DISPSPAWN) {
                displayNextSpawn(ent, client, data);
            }
            else if (command == QCMD_DISPVOTE) {
                displayNextVote(ent, client, data);
            } else if (command == QCMD_DISPDISABLE) {
                displayNextDisable(ent, client, data);
            } else if (command == QCMD_DISPCHECKVAR) {
                displayNextCheckvar(ent, client, data);
            } else if (command == QCMD_CHECKVARTESTS) {
                checkVariableTest(ent, client, data);
            } else if (command == QCMD_CHANGENAME) {
                Q_snprintf(buffer, sizeof(buffer), "name \"%s\"\n", proxyinfo[client].name);
                stuffcmd(ent, buffer);
            } else if (command == QCMD_CHANGESKIN) {
                Q_snprintf(buffer, sizeof(buffer), "skin \"%s\"\n", proxyinfo[client].skin);
                stuffcmd(ent, buffer);
            } else if (command == QCMD_BAN) {
                gi.cprintf(NULL, PRINT_HIGH, "%s: %s\n", proxyinfo[client].name, proxyinfo[client].buffer);
                gi.cprintf(ent, PRINT_HIGH, "%s: %s\n", proxyinfo[client].name, proxyinfo[client].buffer);
                addCmdQueue(client, QCMD_DISCONNECT, 1, 0, proxyinfo[client].buffer);
            } else if (command == QCMD_DISPCHATBANS) {
                displayNextChatBan(ent, client, data);
            } else if (command == QCMD_STUFFCLIENT) {
                stuffNextLine(ent, client);
            } else if (command == QCMD_TESTADMIN) {
                stuffcmd(ent, "!setadmin $q2adminpassword\n");
            }
            else if (command == QCMD_TESTADMIN2) {
                stuffcmd(ent, "!admin $q2adminuser $q2adminpass\n");
            } else if (command == QCMD_TESTADMIN3) {
                stuffcmd(ent, "!bypass $clientuser $clientpass\n");
            } else if (command == QCMD_PMODVERTIMEOUT) {
                //no reply? kick the bastard
                if (!proxyinfo[client].q2a_bypass) {
                    /*if (client_check > 0) {
                        if (proxyinfo[client].pmod != 1) {
                            gi.bprintf(PRINT_HIGH,NOMATCH_KICK_MSG,proxyinfo[client].name);
                            sprintf(buffer,client_msg,version_check);
                            gi.cprintf(getEnt((client + 1)),PRINT_HIGH,"%s\n",buffer);
                            addCmdQueue(client, QCMD_DISCONNECT, 1, 0, Q2A_NOMATCH_KICK_MSG);
                        } else if (proxyinfo[client].cmdlist == 7) { //Kick false NoCheat clients
                            gi.bprintf(PRINT_HIGH, PRV_KICK_MSG, proxyinfo[client].name);
                            sprintf(buffer,client_msg,version_check);
                            gi.cprintf(getEnt((client + 1)),PRINT_HIGH,"%s\n",buffer);
                            addCmdQueue(client, QCMD_DISCONNECT, 1, 0, FRKQ2_KICK_MSG);
                        }
                    }*/
                }
            } else if (command == QCMD_PRIVATECOMMAND) {
                for (j = 0; j < PRIVATE_COMMANDS; j++) {
                    //check each command, if we didnt get a response log it
                    if (private_commands[j].command[0]) {
                        if (((!proxyinfo[client].private_command_got[j]) && (j < 4)) || ((proxyinfo[client].private_command_got[j]) && (j > 3))) {
                            //log
                            logEvent(LT_PRIVATELOG, client, ent, private_commands[j].command, 0, 0.0);

                            //kick on private_command
                            if (private_command_kick) {
                                gi.bprintf(PRINT_HIGH, PRV_KICK_MSG, proxyinfo[client].name);
                                addCmdQueue(client, QCMD_DISCONNECT, 1, 0, Q2A_PRV_KICK_MSG);
                                //dont want this printed
                                //return qfalse;
                            }
                        }
                    }
                }
            } else if (command == QCMD_PMODVERTIMEOUT_INGAME) {
                if (!proxyinfo[client].q2a_bypass) {
                    /*if (client_check > 0)
                    {
                    //no reply? increase no reply count
                            if (!proxyinfo[client].pmod)
                            {
                                    proxyinfo[client].pmod_noreply_count++;
                                    if (proxyinfo[client].pmod_noreply_count > max_pmod_noreply)
                                    {
                                            gi.bprintf(PRINT_HIGH,MOD_KICK_MSG,proxyinfo[client].name,proxyinfo[client].pmod);
                                            sprintf(buffer,client_msg,version_check);
                                            gi.cprintf(getEnt((client + 1)),PRINT_HIGH,"%s\n",buffer);
                                            addCmdQueue(client, QCMD_DISCONNECT, 1, 0, Q2A_MOD_KICK_MSG);
                                    }
                            }
                            else if (proxyinfo[client].pmod != 1)
                            {
                                    gi.bprintf(PRINT_HIGH,MOD_KICK_MSG,proxyinfo[client].name,proxyinfo[client].pmod);
                                    sprintf(buffer,client_msg,version_check);
                                    gi.cprintf(getEnt((client + 1)),PRINT_HIGH,"%s\n",buffer);
                                    addCmdQueue(client, QCMD_DISCONNECT, 1, 0, Q2A_MOD_KICK_MSG);
                            }
                    }*/
                }
            } else if (command == QCMD_GL_CHECK) {
            } else if (command == QCMD_SETUPTIMESCALE) {
                stuffcmd(ent, "set timescale $timescale u\n");
            } else if (command == QCMD_SETTIMESCALE) {
                if (timescaledetect) {
                    q2a_strcpy(buffer, "set timescale 1\n");
                    stuffcmd(ent, buffer);
                }
            } else if (command == QCMD_SPAMBYPASS) {
                if (proxyinfo[client].q2a_bypass) {
                    gi.bprintf(PRINT_HIGH, "%s has logged on without an anti-cheat client because of an arrangement\nƒ  with the server admin.  This is most likely because %s is using a linux\nƒ  or mac client - contact the server admin if you have issues with %s.\n", proxyinfo[client].name, proxyinfo[client].name, proxyinfo[client].name);
                }
            } else if (command == QCMD_GETCMDQUEUE) {
                addCmdQueue(client, QCMD_TESTCMDQUEUE, 5, 0, 0);
                proxyinfo[client].cmdlist_timeout = ltime;
                proxyinfo[client].cmdlist_timeout += 5;
                proxyinfo[client].cmdlist = 1;
                //1.20
                if (!proxyinfo[client].done_server_and_blocklist) {
                    proxyinfo[client].blocklist = random()*(MAX_BLOCK_MODELS - 1);
                    Q_snprintf(buffer, sizeof(buffer), "p_blocklist %i\n", proxyinfo[client].blocklist);
                    stuffcmd(ent, buffer);
                    generateRandomString(proxyinfo[client].serverip, 15);
                    Q_snprintf(buffer, sizeof(buffer), "p_server %s\n", proxyinfo[client].serverip);
                    stuffcmd(ent, buffer);
                    //q2ace responds with blahblah %i %s
                }
            } else if (command == QCMD_TESTCMDQUEUE) {
                if (proxyinfo[client].done_server_and_blocklist)
                    required_cmdlist = 1;
                else
                    required_cmdlist = 7;

                if (!proxyinfo[client].cmdlist) {
                    proxyinfo[client].pcmd_noreply_count++;
                    if (proxyinfo[client].pcmd_noreply_count > max_pmod_noreply) {
                        gi.bprintf(PRINT_HIGH, MOD_KICK_MSG, proxyinfo[client].name, 16);
                        //sprintf(buffer,client_msg,version_check);
                        //gi.cprintf(getEnt((client + 1)),PRINT_HIGH,"%s\n",buffer);
                        addCmdQueue(client, QCMD_DISCONNECT, 1, 0, Q2A_MOD_KICK_MSG);
                    }
                } else if (proxyinfo[client].cmdlist == required_cmdlist) {
                    //all 3 checks came thru fine
                    proxyinfo[client].done_server_and_blocklist = 1;
                } else {
                    //just kick anyway
                    //gi.bprintf(PRINT_HIGH,MOD_KICK_MSG,proxyinfo[client].name,proxyinfo[client].cmdlist);
                    //sprintf(buffer,client_msg,version_check);
                    //gi.cprintf(getEnt((client + 1)),PRINT_HIGH,"%s\n",buffer);
                    addCmdQueue(client, QCMD_DISCONNECT, 1, 0, Q2A_MOD_KICK_MSG);
                }
            } else if (command == QCMD_EXECMAPCFG) {
                if (client_map_cfg & 1) {
                    Q_snprintf(buffer, sizeof(buffer), "set map_name %s\n", gmapname);
                    stuffcmd(ent, buffer);
                } else if (client_map_cfg & 2) {
                    Q_snprintf(buffer, sizeof(buffer), "exec cfg/%s.cfg\n", gmapname);
                    stuffcmd(ent, buffer);
                } else if (client_map_cfg & 4) {
                    Q_snprintf(buffer, sizeof(buffer), "exec cfg/all.cfg\n");
                    stuffcmd(ent, buffer);
                }
            } else if (command == QCMD_SHOWMOTD) {
                if (zbotmotd[0]) {
                    gi.centerprintf(ent, motd);
                }
            } else if (command == QCMD_RUNVOTECMD) {
                gi.AddCommandString(cmdpassedvote);
            } else if (command == QCMD_TESTTIMESCALE) {
                if (timescaledetect) {
                    generateRandomString(proxyinfo[client].hack_timescale, RANDOM_STRING_LENGTH);
                    Q_snprintf(buffer, sizeof(buffer), "%s $timescale\n", proxyinfo[client].hack_timescale);
                    stuffcmd(ent, buffer);
                    addCmdQueue(client, QCMD_TESTTIMESCALE, 15, 0, 0);
                }
            } else if (command == QCMD_SETUPCL_PITCHSPEED) {
                stuffcmd(ent, "set cl_pitchspeed $cl_pitchspeed u\n");
                addCmdQueue(client, QCMD_FORCEUDATAUPDATEPS, 0, 0, 0);
            } else if (command == QCMD_FORCEUDATAUPDATEPS) {
                if (proxyinfo[client].cl_pitchspeed) {
                    Q_snprintf(buffer, sizeof(buffer), "set cl_pitchspeed %d\nset cl_pitchspeed %d\n", proxyinfo[client].cl_pitchspeed + 1, proxyinfo[client].cl_pitchspeed);
                    stuffcmd(ent, buffer);
                }
            } else if (command == QCMD_SETUPCL_ANGLESPEEDKEY) {
                stuffcmd(ent, "set cl_anglespeedkey $cl_anglespeedkey u\n");
                addCmdQueue(client, QCMD_FORCEUDATAUPDATEAS, 0, 0, 0);
            } else if (command == QCMD_FORCEUDATAUPDATEAS) {
                if (proxyinfo[client].cl_anglespeedkey) {
                    Q_snprintf(buffer, sizeof(buffer), "set cl_anglespeedkey %g\nset cl_anglespeedkey %g\n", proxyinfo[client].cl_anglespeedkey + 1.0, proxyinfo[client].cl_anglespeedkey);
                    stuffcmd(ent, buffer);
                }
            } else if (command == QCMD_MSGDISCONNECT) {
                Q_snprintf(buffer, sizeof(buffer), "Client 'msg' mode has to be set to less than %d on this server!\n", maxMsgLevel + 1);
                gi.cprintf(ent, PRINT_HIGH, buffer);
                addCmdQueue(client, QCMD_DISCONNECT, 1, 0, buffer);
            }
        } else {
            if (maxdoclients < maxclients->value) {
                maxdoclients++;
            }
        }
    }

    if (client >= maxclients->value) {
        client = -1;
    }

    checkOnVoting();

    HTTP_RunDownloads();

    profile_start(2);
    ge_mod->RunFrame();
    CA_RunFrame();
    profile_stop_2(2, "mod->G_RunFrame", 0, NULL);

    G_MergeEdicts();

    profile_stop_2(1, "q2admin->G_RunFrame", 0, NULL);
}


/*
=================
GetGameAPI

Returns a pointer to the structure with all entry points
and global variables
=================
 */
q_exported game_export_t *GetGameAPI(game_import_t *import) {
    GAMEAPI *getapi;
    cvar_t *gamelib;

    dllloaded = qfalse;
    gi = *import;

    q2a_strcpy(version, "r");
    q2a_strcat(version, Q2A_COMMIT);

    //gi.dprintf("Q2Admin %s\n", version);

    // real game lib will use these internal functions
    import->bprintf = bprintf_internal;
    import->cprintf = cprintf_internal;
    import->dprintf = dprintf_internal;
    import->AddCommandString = AddCommandString_internal;
    import->Pmove = Pmove_internal;
    import->linkentity = linkentity_internal;
    import->unlinkentity = unlinkentity_internal;

    ge.Init = InitGame;
    ge.Shutdown = ShutdownGame;
    ge.SpawnEntities = SpawnEntities;

    ge.WriteGame = WriteGame;
    ge.ReadGame = ReadGame;
    ge.WriteLevel = WriteLevel;
    ge.ReadLevel = ReadLevel;

    ge.ClientThink = ClientThink;
    ge.ClientConnect = ClientConnect;
    ge.ClientUserinfoChanged = ClientUserinfoChanged;
    ge.ClientDisconnect = ClientDisconnect;
    ge.ClientBegin = ClientBegin;
    ge.ClientCommand = ClientCommand;

    ge.RunFrame = G_RunFrame;

    ge.ServerCommand = ServerCommand;

    serverbindip = gi.cvar("ip", "", 0);
    port = gi.cvar("port", "", 0);
    rcon_password = gi.cvar("rcon_password", "", 0);
    q2aconfig = gi.cvar("q2aconfig", CFGFILE, CVAR_GENERAL);

    // override default config files
    configfile_ban = gi.cvar("q2a_banfile", BANLISTFILE, CVAR_LATCH);
    configfile_bypass = gi.cvar("q2a_bypassfile", BYPASSFILE, CVAR_LATCH);
    configfile_cloud = gi.cvar("q2a_cloudfile", CLOUDFILE, CVAR_LATCH);
    configfile_cvar = gi.cvar("q2a_cvarfile", CHECKVARFILE, CVAR_LATCH);
    configfile_disable = gi.cvar("q2a_disablefile", DISABLEFILE, CVAR_LATCH);
    configfile_flood = gi.cvar("q2a_floodfile", FLOODFILE, CVAR_LATCH);
    configfile_login = gi.cvar("q2a_loginfile", LOGINFILE, CVAR_LATCH);
    configfile_log = gi.cvar("q2a_logfile", LOGLISTFILE, CVAR_LATCH);
    configfile_rcon = gi.cvar("q2a_rconfile", LRCONFILE, CVAR_LATCH);
    configfile_spawn = gi.cvar("q2a_spawnfile", SPAWNFILE, CVAR_LATCH);
    configfile_vote = gi.cvar("q2a_votefile", VOTEFILE, CVAR_LATCH);

    q2adminbantxt = gi.cvar("q2adminbantxt", "", 0);
    q2adminbanremotetxt_enable = gi.cvar("q2adminbanremotetxt_enable", "0", 0);
    q2adminbanremotetxt = gi.cvar("q2adminbanremotetxt_file", "", 0);
    q2adminanticheat_enable = gi.cvar("q2adminanticheat_enable", "0", 0);
    q2adminanticheat_file = gi.cvar("q2adminanticheat_file", "", 0);
    q2adminhashlist_enable = gi.cvar("q2adminhashlist_enable", "0", 0);
    q2adminhashlist_dir = gi.cvar("q2adminhashlist_dir", "", 0);

    gamedir = gi.cvar("game", "baseq2", 0);
    q2a_strncpy(moddir, gamedir->string, sizeof(moddir));

    if (moddir[0] == 0) {
        q2a_strcpy(moddir, "baseq2");
    }

    unsigned int i;
    for (i = 0; i < PRIVATE_COMMANDS; i++) {
        private_commands[i].command[0] = 0;
    }


    q2a_strcpy(client_msg, DEFAULTQ2AMSG);
    q2a_strcpy(zbotuserdisplay, DEFAULTUSERDISPLAY);
    q2a_strcpy(timescaleuserdisplay, DEFAULTTSDISPLAY);
    q2a_strcpy(hackuserdisplay, DEFAULTHACKDISPLAY);
    q2a_strcpy(skincrashmsg, DEFAULTSKINCRASHMSG);
    q2a_strcpy(defaultreconnectmessage, DEFAULTRECONNECTMSG);
    q2a_strcpy(defaultBanMsg, DEFAULTBANMSG);
    q2a_strcpy(nameChangeFloodProtectMsg, DEFAULTFLOODMSG);
    q2a_strcpy(skinChangeFloodProtectMsg, DEFAULTSKINFLOODMSG);
    q2a_strcpy(defaultChatBanMsg, DEFAULTCHABANMSG);
    q2a_strcpy(chatFloodProtectMsg, DEFAULTCHATFLOODMSG);
    q2a_strcpy(clientVoteCommand, DEFAULTVOTECOMMAND);
    q2a_strcpy(cl_pitchspeed_kickmsg, DEFAULTCL_PITCHSPEED_KICKMSG);
    q2a_strcpy(cl_anglespeedkey_kickmsg, DEFAULTCL_ANGLESPEEDKEY_KICKMSG);
    q2a_strcpy(lockoutmsg, DEFAULTLOCKOUTMSG);

    adminpassword[0] = 0;
    customServerCmd[0] = 0;
    customClientCmd[0] = 0;
    customClientCmdConnect[0] = 0;
    customServerCmdConnect[0] = 0;

    readCfgFiles();

    if (q2adminrunmode) {
        loadLogList();
    }

    srand((unsigned) time(NULL));

    zbot_teststring1[7] = zbot_teststring_test1[7] = '0' + (int) (9.9 * random());
    zbot_teststring1[8] = zbot_teststring_test1[8] = '0' + (int) (9.9 * random());
    zbot_teststring_test2[3] = '0' + (int) (9.9 * random());
    zbot_teststring_test2[4] = '0' + (int) (9.9 * random());
    zbot_testchar1 = '0' + (int) (9.9 * random());
    zbot_testchar2 = '0' + (int) (9.9 * random());

    // take cvar first, then gamelibrary from config, then old game.real.ext
    gamelib = gi.cvar("gamelib", (*gamelibrary) ? gamelibrary : DLLNAME, CVAR_LATCH);

    snprintf(dllname, sizeof(dllname), "%s/%s", moddir, gamelib->string);

#if defined(_WIN32) || defined(_WIN64)

    hdll = LoadLibrary(dllname);
    if (hdll == NULL) {
        gi.cprintf(NULL, PRINT_HIGH, "Unable to load %s, trying from baseq2 directory\n", dllname);
        Q_snprintf(dllname, sizeof(dllname), "baseq2/%s", gamelib->string);

        hdll = LoadLibrary(dllname);
        if (hdll == NULL) {
            gi.error("Unable to load game DLL %s\n", dllname);
            return &ge;
        }
    }

    getapi = (GAMEAPI *) GetProcAddress(hdll, "GetGameAPI");
    if (getapi == NULL) {
        FreeLibrary(hdll);
        gi.error("No GetGameApi() entry in DLL %s.\n", dllname);
        return &ge;
    }
#else
    int loadtype;
    loadtype = soloadlazy ? RTLD_LAZY : RTLD_NOW;

    hdll = dlopen(dllname, loadtype);
    if (hdll == NULL) {
        gi.cprintf(NULL, PRINT_HIGH, "Unable to load %s, trying from baseq2 directory\n", dllname);
        Q_snprintf(dllname, sizeof(dllname), "baseq2/%s", gamelib->string);

        hdll = dlopen(dllname, loadtype);
        if (hdll == NULL) {
            gi.error("Unable to load game library %s\n", dllname);
            return &ge;
        }
    }

    getapi = (GAMEAPI *) dlsym(hdll, "GetGameAPI");
    if (getapi == NULL) {
        dlclose(hdll);
        gi.error("No GetGameApi() entry in game library %s.\n", dllname);
        return &ge;
    }
#endif

    gi.cprintf(NULL, PRINT_HIGH, "Q2Admin %s -> %s\n", version, dllname);

    ge_mod = (*getapi)(import);
    dllloaded = qtrue;
    G_MergeEdicts();

    if (q2adminrunmode) {
        logEvent(LT_SERVERSTART, 0, NULL, NULL, 0, 0.0);
    }

    return &ge;
}
