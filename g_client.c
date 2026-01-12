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

int zbc_jittermax = 4;
int zbc_jittertime = 10;
int zbc_jittermove = 500;

qboolean zbc_enable = qtrue;
qboolean timescaledetect = qtrue;
qboolean swap_attack_use = qfalse;
qboolean dopversion = qtrue;


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

qboolean checkImpulse(byte impulse) {
    unsigned int i;

    if (!maxImpulses) {
        return qtrue;
    }

    for (i = 0; i < maxImpulses; i++) {
        if (impulsesToKickOn[i] == impulse) {
            return qtrue;
        }
    }

    return qfalse;
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
    if (runmode == 0) {
        gi.Pmove(pmove);
        return;
    }

    gi.Pmove(pmove);
}

void ClientThink(edict_t *ent, usercmd_t *ucmd) {
    int client;
    char *msg = 0;
    profile_init_2(1);
    profile_init_2(2);

    if (!dllloaded) return;

    if (runmode == 0) {
        ge_mod->ClientThink(ent, ucmd);
        G_MergeEdicts();
        return;
    }

    client = getEntOffset(ent);
    client -= 1;

    profile_start(1);

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

        profile_start(2);
        ge_mod->ClientThink(ent, ucmd);
        profile_stop_2(2, "mod->ClientThink", 0, NULL);

        G_MergeEdicts();
    }

    profile_stop_2(1, "q2admin->ClientThink", 0, NULL);
}

// unused
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

/**
 * ZbotCheck v1.01 for Quake 2 by Matt "WhiteFang" Ayres (matt@lithium.com)
 *
 * This is provided for mod authors to implement Zbot detection, nothing more.
 * The code has so far proven to be reliable at detecting Zbot auto-aim clients
 * (cheaters).  However, no guarantees of any kind are made.  This is provided
 * as-is.  You must be familiar with Quake 2 mod coding to make use of this.
 *
 * Called from ClientThink()
 */
qboolean zbc_ZbotCheck(int client, usercmd_t *ucmd) {
    int tog0, tog1;

    tog0 = proxyinfo[client].zbc_tog;
    proxyinfo[client].zbc_tog ^= 1;
    tog1 = proxyinfo[client].zbc_tog;

    if (ucmd->angles[0] == proxyinfo[client].zbc_angles[tog1][0] &&
            ucmd->angles[1] == proxyinfo[client].zbc_angles[tog1][1] &&
            ucmd->angles[0] != proxyinfo[client].zbc_angles[tog0][0] &&
            ucmd->angles[1] != proxyinfo[client].zbc_angles[tog0][1] &&
            abs(ucmd->angles[0] - proxyinfo[client].zbc_angles[tog0][0]) +
            abs(ucmd->angles[1] - proxyinfo[client].zbc_angles[tog0][1]) >= zbc_jittermove) {
        if (ltime <= proxyinfo[client].zbc_jitter_last + 0.1) {
            if (!proxyinfo[client].zbc_jitter) {
                proxyinfo[client].zbc_jitter_time = ltime;
            }
            if (proxyinfo[client].zbc_jitter++ >= zbc_jittermax) {
                return qtrue;
            }
        }
        proxyinfo[client].zbc_jitter_last = ltime;
    }
    proxyinfo[client].zbc_angles[tog1][0] = ucmd->angles[0];
    proxyinfo[client].zbc_angles[tog1][1] = ucmd->angles[1];

    if (ltime > (proxyinfo[client].zbc_jitter_time + zbc_jittertime)) {
        proxyinfo[client].zbc_jitter = 0;
    }
    return qfalse;
}
