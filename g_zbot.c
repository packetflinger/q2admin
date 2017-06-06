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

extern cvar_t *remote_enabled;

admin_type admin_pass[MAX_ADMINS];
admin_type q2a_bypass_pass[MAX_ADMINS];
int num_admins = 0;
int num_q2a_admins = 0;

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

    if (proxyinfo[client].ipaddress[0]) {
        return;
    }

    sprintf(buffer, "%s/qconsole.log", moddir);
    dumpfile = fopen(buffer, "rt");
    if (!dumpfile) {
        return;
    }

    fseek(dumpfile, 0, SEEK_END);
    fpos = ftell(dumpfile) - 1;

    while (getLastLine(buffer, dumpfile, &fpos)) {
        if (startContains(buffer, "ip")) {
            char *cp = buffer + 3;
            char *dp = proxyinfo[client].ipaddress;

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

    sprintf(buffer, "%s/qconsole.log", moddir);
    q2logfile = fopen(buffer, "rt");
    if (!q2logfile) {
        return 0; // assume ok
    }

    fseek(q2logfile, proxyinfo[client].logfilecheckpos, SEEK_SET);

    sprintf(checkmask1, "WARNING: msg overflowed for %s", proxyinfo[client].name);
    sprintf(checkmask2, "%s overflowed", proxyinfo[client].name);

    while (fgets(buffer, 256, q2logfile)) {
        if (startContains(buffer, checkmask1) || startContains(buffer, checkmask2)) { // we have a problem...
            ret = 1;
            proxyinfo[client].clientcommand &= ~CCMD_ZPROXYCHECK2;
            removeClientCommand(client, QCMD_ZPROXYCHECK1);
            removeClientCommand(client, QCMD_ZPROXYCHECK2);
            addCmdQueue(client, QCMD_RESTART, 2 + (5 * random()), 0, 0);

            sprintf(checkmask1, "I(%d) Exp(%s) (%s) (overflow detected)", proxyinfo[client].charindex, proxyinfo[client].teststr, buffer);
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



//===================================================================

/*
static pmove_done = 0;
static vec3_t pmove_origin, pmove_old, pmove_orgvel, pmove_newvel;
static short pmove_orggrav;
 */


void Pmove_internal(pmove_t *pmove) {
    if (q2adminrunmode == 0) {
        gi.Pmove(pmove);
        return;
    }

    //  pmove_orggrav = pmove->s.gravity;
    //  VectorCopy(pmove->s.velocity, pmove_orgvel);
    //  VectorCopy(pmove->s.origin, pmove_old);

    gi.Pmove(pmove);

    //  VectorCopy(pmove->s.origin, pmove_origin);
    //  VectorCopy(pmove->s.velocity, pmove_newvel);

    //  pmove_done = 1;
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
        copyDllInfo();
        return;
    }

    client = getEntOffset(ent);
    client -= 1;

    STARTPERFORMANCE(1);

    //*** UPDATE START ***
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
    //*** UPDATE END ***

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

        copyDllInfo();
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



int get_admin_level(char *givenpass, char *givenname) {
    int got_level = 0;
    unsigned int i;

    for (i = 0; i < num_admins; i++) {
        if (!admin_pass[i].level)
            break;
        if ((strcmp(givenpass, admin_pass[i].password) == 0) && (strcmp(givenname, admin_pass[i].name) == 0)) {
            got_level = admin_pass[i].level;
            break;
        }
    }
    return got_level;
}

int get_bypass_level(char *givenpass, char *givenname) {
    int got_level = 0;
    unsigned int i;

    for (i = 0; i < num_q2a_admins; i++) {
        if (!q2a_bypass_pass[i].level)
            break;
        if ((strcmp(givenpass, q2a_bypass_pass[i].password) == 0) && (strcmp(givenname, q2a_bypass_pass[i].name) == 0)) {
            got_level = q2a_bypass_pass[i].level;
            break;
        }
    }
    return got_level;
}

void List_Admin_Commands(edict_t *ent, int client) {

    if (proxyinfo[client].q2a_admin & 1) {
        gi.cprintf(ent, PRINT_HIGH, "    - !boot <number>\n");
    }
    if (proxyinfo[client].q2a_admin & 2) {
        gi.cprintf(ent, PRINT_HIGH, "    - !dumpmsec\n");
    }
    if (proxyinfo[client].q2a_admin & 4) {
        gi.cprintf(ent, PRINT_HIGH, "    - !changemap <mapname>\n");
    }
    if (proxyinfo[client].q2a_admin & 8) {
        gi.cprintf(ent, PRINT_HIGH, "    - !dumpuser <num>\n");
    }
    if (proxyinfo[client].q2a_admin & 16) {
        gi.cprintf(ent, PRINT_HIGH, "    - !auth\n");
        gi.cprintf(ent, PRINT_HIGH, "    - !gfx\n");
    }
    if (proxyinfo[client].q2a_admin & 32) {
        gi.cprintf(ent, PRINT_HIGH, "    - !dostuff <num> <commands>\n");
    }
    if (proxyinfo[client].q2a_admin & 128) {
        if (whois_active)
            gi.cprintf(ent, PRINT_HIGH, "    - !writewhois\n");
    }
    gi.cprintf(ent, PRINT_HIGH, "\n");
}

void Read_Admin_cfg(void) {
    FILE *f;
    char name[256];
    int i, i2;

    sprintf(name, "%s/q2adminlogin.txt", moddir);

    f = fopen(name, "rb");
    if (!f) {
        gi.dprintf("WARNING: %s could not be found\n", name);
        goto file2;
        return;
    }

    i = 0;
    while ((!feof(f)) && (i < MAX_ADMINS)) {
        fscanf(f, "%s %s %d", &admin_pass[i].name, &admin_pass[i].password, &admin_pass[i].level);
        i++;
    }
    if (!admin_pass[i].level)
        i--;
    num_admins = i;
    if (i < MAX_ADMINS)
        for (i2 = i; i2 < MAX_ADMINS; i2++)
            admin_pass[i2].level = 0;

    //read em in
    fclose(f);

file2:
    ;
    sprintf(name, "%s/q2adminbypass.txt", moddir);

    f = fopen(name, "rb");
    if (!f) {
        gi.dprintf("WARNING: %s could not be found\n", name);
        return;
    }

    i = 0;
    while ((!feof(f)) && (i < MAX_ADMINS)) {
        fscanf(f, "%s %s %d", &q2a_bypass_pass[i].name, &q2a_bypass_pass[i].password, &q2a_bypass_pass[i].level);
        i++;
    }
    if (!q2a_bypass_pass[i].level)
        i--;
    num_q2a_admins = i;
    if (i < MAX_ADMINS)
        for (i2 = i; i2 < MAX_ADMINS; i2++)
            q2a_bypass_pass[i2].level = 0;

    //read em in
    fclose(f);
}

void ADMIN_players(edict_t *ent, int client) {
    unsigned int i;
    gi.cprintf(ent, PRINT_HIGH, "Players\n");
    for (i = 0; i < maxclients->value; i++) {
        if (proxyinfo[i].inuse) {
            gi.cprintf(ent, PRINT_HIGH, "  %2i : %s\n", i, proxyinfo[i].name);
        }
    }
    gi.cprintf(ent, PRINT_HIGH, "*******************************\n");
}

void ADMIN_dumpmsec(edict_t *ent, int client) {
    unsigned int i;
    gi.cprintf(ent, PRINT_HIGH, "MSEC\n");
    for (i = 0; i < maxclients->value; i++) {
        if (proxyinfo[i].inuse) {
            gi.cprintf(ent, PRINT_HIGH, "  %2i : %-16s %d\n", i, proxyinfo[i].name, proxyinfo[i].msec_last);
        }
    }
    gi.cprintf(ent, PRINT_HIGH, "*******************************\n");
}

void ADMIN_dumpuser(edict_t *ent, int client, int user, qboolean check) {
    char *cp1;

    if (gi.argc() < 2) {
        ADMIN_players(ent, client);
        return;
    }

    if (check)
        if (!proxyinfo[user].inuse)
            return;
    //if (proxyinfo[user].inuse)
    {
        gi.cprintf(ent, PRINT_HIGH, "User Info for client %d\n", user);

        cp1 = Info_ValueForKey(proxyinfo[user].userinfo, "msg");
        gi.cprintf(ent, PRINT_HIGH, "msg          %s\n", cp1);

        cp1 = Info_ValueForKey(proxyinfo[user].userinfo, "spectator");
        gi.cprintf(ent, PRINT_HIGH, "spectator    %s\n", cp1);

        cp1 = Info_ValueForKey(proxyinfo[user].userinfo, "cl_maxfps");
        gi.cprintf(ent, PRINT_HIGH, "cl_maxfps    %s\n", cp1);

        cp1 = Info_ValueForKey(proxyinfo[user].userinfo, "gender");
        gi.cprintf(ent, PRINT_HIGH, "gender       %s\n", cp1);

        cp1 = Info_ValueForKey(proxyinfo[user].userinfo, "fov");
        gi.cprintf(ent, PRINT_HIGH, "fov          %s\n", cp1);

        cp1 = Info_ValueForKey(proxyinfo[user].userinfo, "rate");
        gi.cprintf(ent, PRINT_HIGH, "rate         %s\n", cp1);

        cp1 = Info_ValueForKey(proxyinfo[user].userinfo, "skin");
        gi.cprintf(ent, PRINT_HIGH, "skin         %s\n", cp1);

        cp1 = Info_ValueForKey(proxyinfo[user].userinfo, "hand");
        gi.cprintf(ent, PRINT_HIGH, "hand         %s\n", cp1);

        if (strlen(proxyinfo[user].gl_driver)) {
            gi.cprintf(ent, PRINT_HIGH, "gl_driver    %s\n", proxyinfo[user].gl_driver);
        }

        if (proxyinfo[client].q2a_admin & 16) {
            gi.cprintf(ent, PRINT_HIGH, "ip           %s\n", proxyinfo[user].ipaddress);
        }

        cp1 = Info_ValueForKey(proxyinfo[user].userinfo, "name");
        gi.cprintf(ent, PRINT_HIGH, "name         %s\n", cp1);

        if (proxyinfo[client].q2a_admin & 128) {
            gi.cprintf(ent, PRINT_HIGH, "full         %s\n", proxyinfo[user].userinfo);
        }
    }
}

void ADMIN_auth(edict_t *ent) {
    unsigned int i;
    for (i = 0; i < maxclients->value; i++) {
        if (proxyinfo[i].inuse) {
            stuffcmd(getEnt((i + 1)), "say I'm using $version\n");
            //if (client_check)
            //	stuffcmd(getEnt((i+1)),"nc_say\n");
        }
    }
}

void ADMIN_gfx(edict_t *ent) {
    unsigned int i;
    for (i = 0; i < maxclients->value; i++) {
        if (proxyinfo[i].inuse) {
            stuffcmd(getEnt((i + 1)), "say I'm using $gl_driver ( $vid_ref ) / $gl_mode\n");
        }
    }
}

void ADMIN_boot(edict_t *ent, int client, int user) {
    char tmptext[100];
    if (gi.argc() < 2) {
        ADMIN_players(ent, client);
        return;
    }
    if ((user >= 0) && (user < maxclients->value)) {
        if (proxyinfo[user].inuse) {
            gi.bprintf(PRINT_HIGH, "%s was kicked by %s.\n", proxyinfo[user].name, proxyinfo[client].name);
            sprintf(tmptext, "\nkick %d\n", user);
            gi.AddCommandString(tmptext);
        }
    }
}

void ADMIN_changemap(edict_t *ent, int client, char *mname) {
    char tmptext[100];
    if (gi.argc() < 2) {
        ADMIN_players(ent, client);
        return;
    }
    if (q2a_strstr(mname, "\""))
        return;
    if (q2a_strstr(mname, ";"))
        return;

    gi.bprintf(PRINT_HIGH, "%s is changing map to %s.\n", proxyinfo[client].name, mname);
    sprintf(tmptext, "\nmap %s\n", mname);
    gi.AddCommandString(tmptext);
}

int ADMIN_process_command(edict_t *ent, int client) {
    unsigned int i, done = 0;
    int send_to_client;
    edict_t *send_to_ent;
    char send_string[512];
    char abuffer[256];

    if (strlen(gi.args())) {
        sprintf(abuffer, "COMMAND - %s %s", gi.argv(0), gi.args());
        logEvent(LT_ADMINLOG, client, ent, abuffer, 0, 0.0);
        gi.dprintf("%s\n", abuffer);
    }

    if (proxyinfo[client].q2a_admin & 1) {
        //Level 1 commands
        if (strcmp(gi.argv(0), "!boot") == 0) {
            ADMIN_boot(ent, client, atoi(gi.argv(1)));
            done = 1;
        }
    }

    if (proxyinfo[client].q2a_admin & 2) {
        //Level 2 commands
        if (strcmp(gi.argv(0), "!dumpmsec") == 0) {
            ADMIN_dumpmsec(ent, client);
            done = 1;
        }
    }
    if (proxyinfo[client].q2a_admin & 4) {
        //Level 3 commands
        if (strcmp(gi.argv(0), "!changemap") == 0) {
            ADMIN_changemap(ent, client, gi.argv(1));
            done = 1;
        }
    }
    if (proxyinfo[client].q2a_admin & 8) {
        //Level 4 commands
        if (strcmp(gi.argv(0), "!dumpuser") == 0) {
            ADMIN_dumpuser(ent, client, atoi(gi.argv(1)), true);
            done = 1;
        } else if (strcmp(gi.argv(0), "!dumpuser_any") == 0) {
            ADMIN_dumpuser(ent, client, atoi(gi.argv(1)), false);
            done = 1;
        }
    }
    if (proxyinfo[client].q2a_admin & 16) {
        //Level 5 commands
        if (strcmp(gi.argv(0), "!auth") == 0) {
            ADMIN_auth(ent);
            done = 1;
            gi.cprintf(ent, PRINT_HIGH, "A new auth command has been issued.\n");
        } else if (strcmp(gi.argv(0), "!gfx") == 0) {
            ADMIN_gfx(ent);
            done = 1;
            gi.cprintf(ent, PRINT_HIGH, "Graphics command issued.\n");
        }
    }

    if (proxyinfo[client].q2a_admin & 32) {
        //Level 7 commands

        if (strcmp(gi.argv(0), "!dostuff") == 0) {
            if (gi.argc() > 2) {
                send_to_client = atoi(gi.argv(1));
                if (strcmp(gi.argv(1), "all") == 0) {
                    for (send_to_client = 0; send_to_client < maxclients->value; send_to_client++)
                        if (proxyinfo[send_to_client].inuse) {
                            strcpy(send_string, gi.argv(2));
                            if (gi.argc() > 3)
                                for (i = 3; i < gi.argc(); i++) {
                                    strcat(send_string, " ");
                                    strcat(send_string, gi.argv(i));
                                }
                            send_to_ent = getEnt((send_to_client + 1));
                            stuffcmd(send_to_ent, send_string);
                            gi.cprintf(ent, PRINT_HIGH, "Client %d (%s) has been stuffed!\n", send_to_client, proxyinfo[send_to_client].name);
                        }
                } else
                    if (proxyinfo[send_to_client].inuse) {
                    strcpy(send_string, gi.argv(2));
                    if (gi.argc() > 3)
                        for (i = 3; i < gi.argc(); i++) {
                            strcat(send_string, " ");
                            strcat(send_string, gi.argv(i));
                        }
                    send_to_ent = getEnt((send_to_client + 1));
                    stuffcmd(send_to_ent, send_string);
                    gi.cprintf(ent, PRINT_HIGH, "Client %d (%s) has been stuffed!\n", send_to_client, proxyinfo[send_to_client].name);
                }
            }
            done = 2;
        }
    }

    if (proxyinfo[client].q2a_admin & 128) {
        if ((strcmp(gi.argv(0), "!writewhois") == 0) && (whois_active)) {
            whois_write_file();
            done = 1;
            gi.cprintf(ent, PRINT_HIGH, "Whois file written.\n");
        }
    }
    return done;
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
            sprintf(temp, "%s\r\n", private_commands[i].command);
            stuffcmd(ent, temp);
        }
        proxyinfo[client].private_command_got[i] = false;
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
qboolean timers_active = false;
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
    strcpy(whois_details[WHOIS_COUNT].ip, strtok(proxyinfo[client].ipaddress, ":"));
    strcpy(whois_details[WHOIS_COUNT].dyn[0].name, proxyinfo[client].name);
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
                strcpy(whois_details[proxyinfo[client].userid].dyn[i].name, proxyinfo[client].name);
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
        strcpy(whois_details[proxyinfo[client].userid].dyn[i].name, whois_details[proxyinfo[client].userid].dyn[i + 1].name);
    }
    strcpy(whois_details[proxyinfo[client].userid].dyn[9].name, proxyinfo[client].name);
}

void whois_getid(int client, edict_t *ent) {
    //called when a client connects
    unsigned int i;
    for (i = 0; i < WHOIS_COUNT; i++) {
        if (q2a_strcmp(whois_details[i].ip, strtok(proxyinfo[client].ipaddress, ":")) == 0) {
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

    sprintf(name, "%s/q2adminwhois.txt", moddir);

    f = fopen(name, "wb");
    if (!f) {
        return;
    }

    for (i = 0; i < WHOIS_COUNT; i++) {
        if (whois_details[i].ip[0] == 0)
            continue;

        strcpy(temp, whois_details[i].ip);
        temp_len = strlen(temp);

        //convert spaces to �
        for (j = 0; j < temp_len; j++) {
            if (temp[j] == ' ')
                temp[j] = '�';
        }
        fprintf(f, "%i %s ", whois_details[i].id, temp);

        strcpy(temp, whois_details[i].seen);
        temp_len = strlen(temp);

        for (j = 0; j < temp_len; j++) {
            if (temp[j] == ' ')
                temp[j] = '�';
        }
        fprintf(f, "%s ", temp);

        for (j = 0; j < 10; j++) {
            if (whois_details[i].dyn[j].name[0]) {
                strcpy(temp, whois_details[i].dyn[j].name);
                temp_len = strlen(temp);

                for (k = 0; k < temp_len; k++) {
                    if (temp[k] == ' ')
                        temp[k] = '�';
                }
                fprintf(f, "%s ", temp);
            } else {
                fprintf(f, "� ");
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

    sprintf(name, "%s/q2adminwhois.txt", moddir);

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
            if (whois_details[WHOIS_COUNT].ip[i] == '�') {
                whois_details[WHOIS_COUNT].ip[i] = ' ';
            }
        }

        temp_len = strlen(whois_details[WHOIS_COUNT].seen);
        for (i = 0; i < temp_len; i++) {
            if (whois_details[WHOIS_COUNT].seen[i] == '�') {
                whois_details[WHOIS_COUNT].seen[i] = ' ';
            }
        }

        for (i = 0; i < 10; i++) {
            if ((whois_details[WHOIS_COUNT].dyn[i].name[0] == 255)
                    || (whois_details[WHOIS_COUNT].dyn[i].name[0] == -1)
                    || (whois_details[WHOIS_COUNT].dyn[i].name[0] == '�')) {
                whois_details[WHOIS_COUNT].dyn[i].name[0] = 0;
            } else {
                name_len = strlen(whois_details[WHOIS_COUNT].dyn[i].name);
                for (j = 0; j < name_len; j++) {
                    if (whois_details[WHOIS_COUNT].dyn[i].name[j] == '�') {
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

void reloadLoginFileRun(int startarg, edict_t *ent, int client) {
    Read_Admin_cfg();
    gi.cprintf(ent, PRINT_HIGH, "Login file reloaded.\n");
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
    strcpy(proxyinfo[client].timers[num].action, gi.argv(3));

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
//*** UPDATE END ***
