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

int clientsidetimeout = 30; // 30 seconds should be good for internet play
int zbotdetectactivetimeout = 0; // -1 == random

// char testchars[] = "!@#%^&*()_=|?.>,<[{]}\':1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM";
char testchars[] = "!@#%^&*()_=|?.>,<[{]}\':";
//                  012345678901234567890 1234567890123456789012345678901234567890123456789012345678901234
//                            1         2          3         4         5         6         7         8
int testcharslength = sizeof (testchars) - 1;


qboolean zbc_enable = TRUE;
qboolean timescaledetect = TRUE;
qboolean swap_attack_use = FALSE;
qboolean dopversion = TRUE;


byte impulsesToKickOn[MAXIMPULSESTOTEST];
byte maxImpulses = 0;

char *impulsemessages[] ={
    "169 (zbot toggle menu command)",
    "170 (zbot toggle menu command)",
    "171 (zbot toggle menu command)",
    "172 (zbot toggle menu command)",
    "173 (zbot toggle menu command)",
    "174 (zbot toggles bot on/off command)",
    "175 (zbot toggles scanner display command)"
};



//===================================================================

void generateRandomString(char *buffer, int length) {
    unsigned int index;
    for (index = 0; index < length; index++) {
        buffer[index] = RANDCHAR();
    }
    buffer[index] = 0;
}

qboolean checkImpulse(byte impulse) {
    unsigned int i;

    if (!maxImpulses) {
        return TRUE;
    }

    for (i = 0; i < maxImpulses; i++) {
        if (impulsesToKickOn[i] == impulse) {
            return TRUE;
        }
    }

    return FALSE;
}

void readIpFromLog(int client, edict_t *ent) {
    FILE *dumpfile;
    long fpos;

    if (HASIP(client)) {
        return;
    }

    Q_snprintf(buffer, sizeof(buffer), "%s/qconsole.log", moddir);
    dumpfile = fopen(buffer, "rt");
    if (!dumpfile) {
        return;
    }

    fseek(dumpfile, 0, SEEK_END);
    fpos = ftell(dumpfile) - 1;

    while (getLastLine(buffer, dumpfile, &fpos)) {
        if (startContains(buffer, "ip")) {
            char *cp = buffer + 3;
            char *dp = IP(client);

            SKIPBLANK(cp);

            while (*cp && *cp != ' ' && *cp != '\n') {
                *dp++ = *cp++;
            }

            *dp = 0;
            break;
        } else if (startContains(buffer, "userinfo")) {
            break;
        }
    }

    fclose(dumpfile);
}

int checkForOverflows(edict_t *ent, int client) {
    FILE *q2logfile;
    char checkmask1[100], checkmask2[100];
    unsigned int ret = 0;

    Q_snprintf(buffer, sizeof(buffer), "%s/qconsole.log", moddir);
    q2logfile = fopen(buffer, "rt");
    if (!q2logfile) {
        return 0; // assume ok
    }

    fseek(q2logfile, proxyinfo[client].logfilecheckpos, SEEK_SET);

    Q_snprintf(checkmask1, sizeof(checkmask1), "WARNING: msg overflowed for %s", proxyinfo[client].name);
    Q_snprintf(checkmask2, sizeof(checkmask2), "%s overflowed", proxyinfo[client].name);

    while (fgets(buffer, 256, q2logfile)) {
        if (startContains(buffer, checkmask1) || startContains(buffer, checkmask2)) { // we have a problem...
            ret = 1;
            proxyinfo[client].clientcommand &= ~CCMD_ZPROXYCHECK2;
            removeClientCommand(client, QCMD_ZPROXYCHECK1);
            removeClientCommand(client, QCMD_ZPROXYCHECK2);
            addCmdQueue(client, QCMD_RESTART, 2 + (5 * random()), 0, 0);

            Q_snprintf(checkmask1, sizeof(checkmask1), "I(%d) Exp(%s) (%s) (overflow detected)", proxyinfo[client].charindex, proxyinfo[client].teststr, buffer);
            logEvent(LT_INTERNALWARN, client, ent, checkmask1, IW_OVERFLOWDETECT, 0.0);
            break;
        }
    }

    fseek(q2logfile, 0, SEEK_END);
    proxyinfo[client].logfilecheckpos = ftell(q2logfile);
    fclose(q2logfile);

    return ret;
}

void serverLogZBot(edict_t *ent, int client) {
    addCmdQueue(client, QCMD_LOGTOFILE1, 0, 0, 0);

    if (customServerCmd[0]) {
        // copy string across to buffer, replacing %c with client number
        char *cp = customServerCmd;
        char *dp = buffer;

        while (*cp) {
            if (*cp == '%' && tolower(*(cp + 1)) == 'c') {
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
}

void Pmove_internal(pmove_t *pmove) {
    if (q2adminrunmode == 0) {
        gi.Pmove(pmove);
        return;
    }

    gi.Pmove(pmove);
}

int VectorCompare(vec3_t v1, vec3_t v2) {
    if (v1[0] != v2[0] || v1[1] != v2[1] || v1[2] != v2[2])
        return 0;

    return 1;
}

void ClientThink(edict_t *ent, usercmd_t *ucmd) {
    int client;
    char *msg = 0;
    INITPERFORMANCE_2(1);
    INITPERFORMANCE_2(2);

    if (!dllloaded) return;

    if (q2adminrunmode == 0) {
        ge_mod->ClientThink(ent, ucmd);
        G_MergeEdicts();
        return;
    }

    client = getEntOffset(ent);
    client -= 1;

    STARTPERFORMANCE(1);

    proxyinfo[client].frames_count++;

    if (lframenum > proxyinfo[client].msec_start) {
        if (proxyinfo[client].show_fps) {
            if (proxyinfo[client].msec_count == 500) {
                gi.cprintf(ent, PRINT_HIGH, "%3.2f fps\n", (float) proxyinfo[client].frames_count * 2);
            }
        }

        if (proxyinfo[client].msec_count > msec_max) {
            if (msec_kick_on_bad) {
                proxyinfo[client].msec_bad++;
                if (proxyinfo[client].msec_bad >= msec_kick_on_bad) {
                    //kick
                    gi.bprintf(PRINT_HIGH, PRV_KICK_MSG, proxyinfo[client].name);
                    addCmdQueue(client, QCMD_DISCONNECT, 1, 0, "Speed hack.");
                }
            } else {
                if (proxyinfo[client].enteredgame + 5 < ltime) {
                    proxyinfo[client].speedfreeze = ltime;
                    proxyinfo[client].speedfreeze += 3;
                }
            }
        }

        proxyinfo[client].msec_start = lframenum;
        proxyinfo[client].msec_start += msec_int * 10;
        proxyinfo[client].msec_last = proxyinfo[client].msec_count;
        proxyinfo[client].msec_count = 0;
        proxyinfo[client].frames_count = 0;
    }

    proxyinfo[client].msec_count += ucmd->msec;

    if (proxyinfo[client].speedfreeze) {
        if (proxyinfo[client].speedfreeze > ltime) {
            ucmd->msec = 0;
        } else {
            if (speedbot_check_type & 2) {
                gi.bprintf(PRINT_HIGH, "%s has been frozen for exceeding the speed limit.\n", proxyinfo[client].name);
            }
            proxyinfo[client].speedfreeze = 0;
        }

    }

    if (ucmd->impulse) {
        if (client >= maxclients->value) return;

        if (displayimpulses) {
            if (ucmd->impulse >= 169 && ucmd->impulse <= 175) {
                msg = impulsemessages[ucmd->impulse - 169];
                gi.bprintf(PRINT_HIGH, "%s generated an impulse %s\n", proxyinfo[client].name, msg);
            } else {
                msg = "generated an impulse";
                gi.bprintf(PRINT_HIGH, "%s generated an impulse %d\n", proxyinfo[client].name, ucmd->impulse);
            }
        }

        if (ucmd->impulse >= 169 && ucmd->impulse <= 175) {
            proxyinfo[client].impulse = ucmd->impulse;
            addCmdQueue(client, QCMD_LOGTOFILE2, 0, 0, 0);
        } else {
            proxyinfo[client].impulse = ucmd->impulse;
            addCmdQueue(client, QCMD_LOGTOFILE3, 0, 0, 0);
        }

        if (disconnectuserimpulse && checkImpulse(ucmd->impulse)) {
            proxyinfo[client].impulsesgenerated++;

            if (proxyinfo[client].impulsesgenerated >= maximpulses) {
                addCmdQueue(client, QCMD_DISCONNECT, 1, 0, msg);
            }
        }
    }

    if (swap_attack_use) {
        byte temp = (ucmd->buttons & BUTTON_ATTACK);

        if (ucmd->buttons & BUTTON_USE) {
            ucmd->buttons |= BUTTON_ATTACK;
        } else {
            ucmd->buttons &= ~BUTTON_ATTACK;
        }

        if (temp) {
            ucmd->buttons |= BUTTON_USE;
        } else {
            ucmd->buttons &= ~BUTTON_USE;
        }
    }

    if (!(proxyinfo[client].clientcommand & BANCHECK)) {
        if (zbc_enable && !(proxyinfo[client].clientcommand & CCMD_ZBOTDETECTED)) {
            if (zbc_ZbotCheck(client, ucmd)) {
                proxyinfo[client].clientcommand |= (CCMD_ZBOTDETECTED | CCMD_ZPROXYCHECK2);
                removeClientCommand(client, QCMD_ZPROXYCHECK1);
                addCmdQueue(client, QCMD_ZPROXYCHECK2, 1, IW_ZBCHECK, 0);
                addCmdQueue(client, QCMD_RESTART, 1, IW_ZBCHECK, 0);
            }
        }

        STARTPERFORMANCE(2);
        ge_mod->ClientThink(ent, ucmd);
        STOPPERFORMANCE_2(2, "mod->ClientThink", 0, NULL);

        G_MergeEdicts();
    }

    STOPPERFORMANCE_2(1, "q2admin->ClientThink", 0, NULL);
}


void PMOD_TimerCheck(int client) {
    edict_t *ent;
    ent = getEnt((client + 1));

    proxyinfo[client].pmodver = ltime + 10;
    proxyinfo[client].pmod = 0;
    proxyinfo[client].pver = 0;
    addCmdQueue(client, QCMD_PMODVERTIMEOUT_INGAME, 10, 0, 0);
    gi.cprintf(ent, PRINT_HIGH, "q2admin: p_modified Standard Proxy Test\r\n");

    if (gl_driver_check & 1)
        stuffcmd(ent, "say Q2ADMIN_GL_DRIVER_CHECK $gl_driver / $vid_ref / $gl_mode\n");

    if (q2a_command_check) {
        addCmdQueue(client, QCMD_GETCMDQUEUE, 5, 0, 0);
    }
}

priv_t private_commands[PRIVATE_COMMANDS];
int private_command_count;

void stuff_private_commands(int client, edict_t *ent) {
    unsigned int i;
    char temp[256];

    proxyinfo[client].private_command = ltime + 10;

    for (i = 0; i < PRIVATE_COMMANDS; i++) {
        if (private_commands[i].command[0]) {
            //stuff this
            Q_snprintf(temp, sizeof(temp), "%s\r\n", private_commands[i].command);
            stuffcmd(ent, temp);
        }
        proxyinfo[client].private_command_got[i] = qfalse;
    }
}

qboolean can_do_new_cmds(int client) {
    if (proxyinfo[client].newcmd_timeout <= ltime) {
        proxyinfo[client].newcmd_timeout = ltime + 3;
        return TRUE;
    } else {
        return FALSE;
    }

}

//new whois code
void whois_getid(int client, edict_t *ent);
void whois(int client, edict_t *ent);
void whois_adduser(int client, edict_t *ent);
void whois_newname(int client, edict_t *ent);
void whois_update_seen(int client, edict_t *ent);
void whois_dumpdetails(int client, edict_t *ent, int userid);
user_details* whois_details;
int WHOIS_COUNT = 0;
int whois_active = 0;
qboolean timers_active = qfalse;
int timers_min_seconds = 10;
int timers_max_seconds = 180;

void whois(int client, edict_t *ent) {
    char a1[256];
    unsigned int i;
    int temp;

    if (gi.argc() < 2) {
        gi.cprintf(ent, PRINT_HIGH, "\nIncorrect syntax, use: 'whois <name>' or 'whois <id>'\n");
        ADMIN_players(ent, client);
        return;
    }

    strncpy(a1, gi.argv(1), sizeof (a1));
    a1[sizeof (a1) - 1] = 0;

    temp = q2a_atoi(a1);
    if ((temp == 0) && (strcmp(a1, "0"))) {
        temp = -1;
    }

    //do numbers first
    if ((temp < maxclients->value) && (temp >= 0)) {
        if ((proxyinfo[temp].inuse) && (proxyinfo[temp].userid >= 0)) {
            //got match, dump details except if admin has proper flag
            if (proxyinfo[temp].q2a_admin & 64) {
                gi.cprintf(ent, PRINT_HIGH, "  Unable to fetch info for %i\n", temp);
                return;
            }
            gi.cprintf(ent, PRINT_HIGH, "\n  Whois details for client %i\n", temp);
            whois_dumpdetails(client, ent, proxyinfo[temp].userid);
            return;
        }

    }
    //then process all connected clients
    for (i = 0; i < maxclients->value; i++) {
        if ((proxyinfo[i].inuse) && (proxyinfo[i].userid >= 0)) {
            //only do partial match on these, dump all that apply
            if (q2a_strcmp(proxyinfo[i].name, a1) == 0) {
                if (proxyinfo[i].q2a_admin & 64) {
                    gi.cprintf(ent, PRINT_HIGH, "  Unable to fetch info for %s\n", a1);
                    return;
                }
                gi.cprintf(ent, PRINT_HIGH, "\n  Whois details for %s\n", proxyinfo[i].name);
                whois_dumpdetails(client, ent, proxyinfo[i].userid);
                //got match, dump details
                return;
            }
        }
    }
    //then if still no match process our stored list
    for (i = 0; i < WHOIS_COUNT; i++) {
        if ((whois_details[i].dyn[0].name[0]) || (whois_details[i].dyn[1].name[0]) || (whois_details[i].dyn[2].name[0]) ||
                (whois_details[i].dyn[3].name[0]) || (whois_details[i].dyn[4].name[0]) || (whois_details[i].dyn[5].name[0]) ||
                (whois_details[i].dyn[6].name[0]) || (whois_details[i].dyn[7].name[0]) || (whois_details[i].dyn[8].name[0]) ||
                (whois_details[i].dyn[9].name[0])) {
            //r1ch: wtf?
            if (((q2a_strcmp(whois_details[i].dyn[0].name, a1) == 0)) ||
                    ((q2a_strcmp(whois_details[i].dyn[1].name, a1) == 0)) ||
                    ((q2a_strcmp(whois_details[i].dyn[2].name, a1) == 0)) ||
                    ((q2a_strcmp(whois_details[i].dyn[3].name, a1) == 0)) ||
                    ((q2a_strcmp(whois_details[i].dyn[4].name, a1) == 0)) ||
                    ((q2a_strcmp(whois_details[i].dyn[5].name, a1) == 0)) ||
                    ((q2a_strcmp(whois_details[i].dyn[6].name, a1) == 0)) ||
                    ((q2a_strcmp(whois_details[i].dyn[7].name, a1) == 0)) ||
                    ((q2a_strcmp(whois_details[i].dyn[8].name, a1) == 0)) ||
                    ((q2a_strcmp(whois_details[i].dyn[9].name, a1) == 0))) {
                gi.cprintf(ent, PRINT_HIGH, "\n  Whois details for %s\n", a1);
                whois_dumpdetails(client, ent, i);
                //got a match, dump details
                return;
            }

        }
    }
    gi.cprintf(ent, PRINT_HIGH, "  No entry found for %s\n", a1);
}

void whois_dumpdetails(int client, edict_t *ent, int userid) {
    unsigned int i;
    for (i = 0; i < 10; i++) {
        if (whois_details[userid].dyn[i].name[0]) {
            if (!proxyinfo[client].q2a_admin)
                gi.cprintf(ent, PRINT_HIGH, "    %02i. %s\n", i + 1, whois_details[userid].dyn[i].name);
            else
                gi.cprintf(ent, PRINT_HIGH, "    %02i. %s %s\n", i + 1, whois_details[userid].dyn[i].name, whois_details[userid].ip);
        }
    }
    gi.cprintf(ent, PRINT_HIGH, "  Last seen: %s\n\n", whois_details[userid].seen);
}

void whois_adduser(int client, edict_t *ent) {
    if (WHOIS_COUNT >= whois_active) {
        WHOIS_COUNT = WHOIS_COUNT - 1; //If max reached, replace latest entry with new client
        //		return;
    }

    whois_details[WHOIS_COUNT].id = WHOIS_COUNT;
    q2a_strncpy(whois_details[WHOIS_COUNT].ip, IP(client), 22);
    q2a_strncpy(whois_details[WHOIS_COUNT].dyn[0].name, proxyinfo[client].name, 16);
    proxyinfo[client].userid = WHOIS_COUNT;
    WHOIS_COUNT++;
}

void whois_newname(int client, edict_t *ent) {
    //called when a client changes name
    unsigned int i;

    if (proxyinfo[client].userid == -1) {
        whois_getid(client, ent);
        return;
    } else {
        for (i = 0; i < 10; i++) {
            if (!whois_details[proxyinfo[client].userid].dyn[i].name[0]) {
                //this is empty, so add here
                q2a_strcpy(whois_details[proxyinfo[client].userid].dyn[i].name, proxyinfo[client].name);
                return;
            }
            if (q2a_strcmp(whois_details[proxyinfo[client].userid].dyn[i].name, proxyinfo[client].name) == 0) {
                //the name already exists, return
                return;
            }
        }
    }
    //if we got here we have a new name but no free slots, so remove 1 for insertion
    for (i = 0; i < 9; i++) {
        q2a_strcpy(whois_details[proxyinfo[client].userid].dyn[i].name, whois_details[proxyinfo[client].userid].dyn[i + 1].name);
    }
    q2a_strcpy(whois_details[proxyinfo[client].userid].dyn[9].name, proxyinfo[client].name);
}

void whois_getid(int client, edict_t *ent) {
    //called when a client connects
    unsigned int i;
    for (i = 0; i < WHOIS_COUNT; i++) {
        if (q2a_strcmp(whois_details[i].ip, IP(client)) == 0) {
            //got a match, store new id
            proxyinfo[client].userid = i;
            whois_newname(client, ent);
            return;
        }
    }
    whois_adduser(client, ent);
}

void whois_update_seen(int client, edict_t *ent) {
    //to be called on client connect and disconnect
    time_t ltimetemp;
    time(&ltimetemp);
    if (proxyinfo[client].userid >= 0) {
        q2a_strcpy(whois_details[proxyinfo[client].userid].seen, ctime(&ltimetemp));
        whois_details[proxyinfo[client].userid].seen[strlen(whois_details[proxyinfo[client].userid].seen) - 1] = 0;
    }
}

void whois_write_file(void) {
    //file format...?
    //id ip seen names
    //when do we want to write this file? it might be processor hungry
    //maybe create a timer that is checked on each spawnentities
    //if 1 day has elapsed then write file
    FILE *f;
    char name[256];
    char temp[256];
    int temp_len;
    unsigned int i, j, k;

    Q_snprintf(name, sizeof(name), "%s/q2adminwhois.txt", moddir);

    f = fopen(name, "wb");
    if (!f) {
        return;
    }

    for (i = 0; i < WHOIS_COUNT; i++) {
        if (whois_details[i].ip[0] == 0)
            continue;

        q2a_strncpy(temp, whois_details[i].ip, sizeof(temp));
        temp_len = strlen(temp);

        //convert spaces to �
        for (j = 0; j < temp_len; j++) {
            if (temp[j] == ' ')
                temp[j] = '?';
        }
        fprintf(f, "%i %s ", whois_details[i].id, temp);

        q2a_strncpy(temp, whois_details[i].seen, sizeof(temp));
        temp_len = strlen(temp);

        for (j = 0; j < temp_len; j++) {
            if (temp[j] == ' ')
                temp[j] = '?';
        }
        fprintf(f, "%s ", temp);

        for (j = 0; j < 10; j++) {
            if (whois_details[i].dyn[j].name[0]) {
                q2a_strncpy(temp, whois_details[i].dyn[j].name, sizeof(temp));
                temp_len = strlen(temp);

                for (k = 0; k < temp_len; k++) {
                    if (temp[k] == ' ')
                        temp[k] = '?';
                }
                fprintf(f, "%s ", temp);
            } else {
                fprintf(f, "? ");
            }
        }
        fprintf(f, "\n");
    }
    fclose(f);
}

void whois_read_file(void) {
    FILE *f;
    char name[256];
    unsigned int i, j;
    int temp_len, name_len;

    Q_snprintf(name, sizeof(name), "%s/q2adminwhois.txt", moddir);

    f = fopen(name, "rb");
    if (!f) {
        gi.dprintf("WARNING: %s could not be found\n", name);
        return;
    }

    WHOIS_COUNT = 0;
    while ((!feof(f)) && (WHOIS_COUNT < whois_active)) {
        fscanf(f, "%i %s %s %s %s %s %s %s %s %s %s %s %s",
                &whois_details[WHOIS_COUNT].id,
                &whois_details[WHOIS_COUNT].ip,
                &whois_details[WHOIS_COUNT].seen,
                &whois_details[WHOIS_COUNT].dyn[0].name,
                &whois_details[WHOIS_COUNT].dyn[1].name,
                &whois_details[WHOIS_COUNT].dyn[2].name,
                &whois_details[WHOIS_COUNT].dyn[3].name,
                &whois_details[WHOIS_COUNT].dyn[4].name,
                &whois_details[WHOIS_COUNT].dyn[5].name,
                &whois_details[WHOIS_COUNT].dyn[6].name,
                &whois_details[WHOIS_COUNT].dyn[7].name,
                &whois_details[WHOIS_COUNT].dyn[8].name,
                &whois_details[WHOIS_COUNT].dyn[9].name);

        //convert all � back to spaces
        temp_len = strlen(whois_details[WHOIS_COUNT].ip);
        for (i = 0; i < temp_len; i++) {
            if (whois_details[WHOIS_COUNT].ip[i] == '?') {
                whois_details[WHOIS_COUNT].ip[i] = ' ';
            }
        }

        temp_len = strlen(whois_details[WHOIS_COUNT].seen);
        for (i = 0; i < temp_len; i++) {
            if (whois_details[WHOIS_COUNT].seen[i] == '?') {
                whois_details[WHOIS_COUNT].seen[i] = ' ';
            }
        }

        for (i = 0; i < 10; i++) {
            if ((whois_details[WHOIS_COUNT].dyn[i].name[0] == 255)
                    || (whois_details[WHOIS_COUNT].dyn[i].name[0] == -1)
                    || (whois_details[WHOIS_COUNT].dyn[i].name[0] == '?')) {
                whois_details[WHOIS_COUNT].dyn[i].name[0] = 0;
            } else {
                name_len = strlen(whois_details[WHOIS_COUNT].dyn[i].name);
                for (j = 0; j < name_len; j++) {
                    if (whois_details[WHOIS_COUNT].dyn[i].name[j] == '?') {
                        whois_details[WHOIS_COUNT].dyn[i].name[j] = ' ';
                    }
                }
            }
        }
        WHOIS_COUNT++;
    }
    fclose(f);
}

void reloadWhoisFileRun(int startarg, edict_t *ent, int client) {
    whois_read_file();
    gi.cprintf(ent, PRINT_HIGH, "Whois file reloaded.\n");
}

//quad/ps etc timer code

void timer_start(int client, edict_t *ent) {
    int seconds;
    int num;

    if (gi.argc() < 4) {
        gi.cprintf(ent, PRINT_HIGH, "Incorrect syntax, use: 'timer_start <number> <seconds> <action>'\n");
        //ADMIN_players(ent,client);
        return;
    }

    if (!can_do_new_cmds(client)) {
        gi.cprintf(ent, PRINT_HIGH, "Please wait 5 seconds\n");
        return; //wait 5 secs before starting the timer again
    }

    num = q2a_atoi(gi.argv(1));
    seconds = q2a_atoi(gi.argv(2));
    if ((seconds < timers_min_seconds) || (seconds > timers_max_seconds)) {
        gi.cprintf(ent, PRINT_HIGH, "Timer seconds falls outside acceptable range of %i to %i.\n", timers_min_seconds, timers_max_seconds);
        return;
    }
    if ((num < 1) || (num > TIMERS_MAX)) {
        gi.cprintf(ent, PRINT_HIGH, "Invalid timer number\n");
        return;
    }
    proxyinfo[client].timers[num].start = ltime + seconds;
    q2a_strncpy(proxyinfo[client].timers[num].action, gi.argv(3), sizeof(proxyinfo[client].timers[num].action));

}

void timer_stop(int client, edict_t *ent) {
    int num;

    if (gi.argc() < 2) {
        gi.cprintf(ent, PRINT_HIGH, "Invalid Timer\n");
        return;
    }
    num = q2a_atoi(gi.argv(1));
    if ((num < 1) || (num > TIMERS_MAX)) {
        gi.cprintf(ent, PRINT_HIGH, "Invalid timer number\n");
        return;
    }

    proxyinfo[client].timers[num].start = 0;
}

void timer_action(int client, edict_t *ent) {
    int num;
    for (num = 0; num < TIMERS_MAX; num++)
        if (proxyinfo[client].timers[num].start) {
            if (proxyinfo[client].timers[num].start <= ltime) {
                proxyinfo[client].timers[num].start = 0;
                stuffcmd(ent, proxyinfo[client].timers[num].action);
            }
        }
}
