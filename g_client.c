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

bool zbc_enable = true;
bool timescaledetect = true;
bool swap_attack_use = false;
bool dopversion = true;


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

bool checkImpulse(byte impulse) {
    unsigned int i;

    if (!maxImpulses) {
        return true;
    }

    for (i = 0; i < maxImpulses; i++) {
        if (impulsesToKickOn[i] == impulse) {
            return true;
        }
    }

    return false;
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

/**
 * Called for each update packet from client to server. The frequency of each
 * run depends on the client's cl_maxfps CVAR; it will be called 1000/cl_maxfps
 * times each second. Pmove is called from the forward game library's
 * ClientThink() function.
 *
 * Currently q2admin will just pass the pmove call along to the forward game
 * library to process.
 *
 * Flow:
 *   server -> (q2a) ClientThink()
 *   (q2a) ClientThink() -> (mod) ClientThink()
 *   (mod) ClientThink() -> (q2a) Pmove_internal
 *   (q2a) Pmove_internal -> (server) PF_Pmove
 */
void Pmove_internal(pmove_t *pmove) {
    if (runmode == 0) {
        gi.Pmove(pmove);
        return;
    }
    gi.Pmove(pmove);
}

/**
 * Called for each client frame. This will called once per cl_maxfps value per
 * second. The msec value in the usercmd_t arg should be approximately
 * 1000/cl_maxfps. For an fps of 120, that equals roughly 8-9ms.
 *
 * The ucmd arg is movement/state data sent from the player's client.
 */
void ClientThink(edict_t *ent, usercmd_t *ucmd) {
    int client;
    char *msg = 0;
    proxyinfo_t *cl;

    profile_init_2(1);
    profile_init_2(2);

    if (!dllloaded) {
        return;
    }

    if (runmode == 0) {
        ge_mod->ClientThink(ent, ucmd);
        G_MergeEdicts();
        return;
    }

    client = getEntOffset(ent) - 1;
    cl = &proxyinfo[client];

    profile_start(1);

    cl->frames_count++;

    if (cl->freeze.frozen) {
        if (cl->freeze.thaw > 0 && cl->freeze.thaw < ltime) {
            q2a_memset(&cl->freeze, 0, sizeof(freeze_t));
        } else {
            ucmd->msec = 0;
        }
    }

    if (lframenum > cl->msec.end_frame) {
        if (cl->show_fps) {
            if (cl->msec.total == 500) {
                gi.cprintf(ent, PRINT_HIGH, "%3.2f fps\n", (float) cl->frames_count * 2);
            }
        }

        if (cl->msec.total > msec.max_allowed) {
            if (msec.max_violations) {
                cl->msec.violations++;
                if (cl->msec.violations >= msec.max_violations) {
                    if (msec.action != MVA_NOTHING) {
                        gi.bprintf(PRINT_HIGH, "Excessive msec consumption from %s\n", cl->name);
                        Q_snprintf(buffer, sizeof(buffer), "exceeded msec limit %d/%d in %d secs", cl->msec.total, msec.max_allowed, msec.timespan);
                        addCmdQueue(client, QCMD_DISCONNECT, 1, 0, buffer);
                    }
                }
            } else {
                // let things stabilize after joining for a few seconds
                if (cl->enteredgame + 5 < ltime) {
                    cl->speedfreeze = ltime + 3;
                }
            }
        }
        if (cl->msec.total < msec.min_required) {
            cl->msec.violations++;
            if (cl->msec.violations >= msec.max_violations) {
                if (msec.action != MVA_NOTHING) {
                    gi.bprintf(PRINT_HIGH, "msec underflow from %s\n", cl->name);
                    Q_snprintf(buffer, sizeof(buffer), "something is fishy, didn't meet msec requirement - %d/%d in %d secs", cl->msec.total, msec.min_required, msec.timespan);
                    addCmdQueue(client, QCMD_DISCONNECT, 1, 0, buffer);
                }
            }
        }

        cl->msec.end_frame = lframenum + (msec.timespan * HZ);
        cl->msec.previous = cl->msec.total;
        cl->msec.total = 0;
        cl->frames_count = 0;
    }

    cl->msec.total += ucmd->msec;

    if (cl->speedfreeze) {
        if (cl->speedfreeze > ltime) {
            ucmd->msec = 0;
        } else {
            if (speedbot_check_type & 2) {
                gi.bprintf(PRINT_HIGH, "%s has been frozen for exceeding the speed limit.\n", cl->name);
            }
            cl->speedfreeze = 0;
        }

    }

    if (ucmd->impulse) {
        if (client >= maxclients->value) return;

        if (displayimpulses) {
            if (ucmd->impulse >= 169 && ucmd->impulse <= 175) {
                msg = impulsemessages[ucmd->impulse - 169];
                gi.bprintf(PRINT_HIGH, "%s generated an impulse %s\n", cl->name, msg);
            } else {
                msg = "generated an impulse";
                gi.bprintf(PRINT_HIGH, "%s generated an impulse %d\n", cl->name, ucmd->impulse);
            }
        }

        if (ucmd->impulse >= 169 && ucmd->impulse <= 175) {
            cl->impulse = ucmd->impulse;
            addCmdQueue(client, QCMD_LOGTOFILE2, 0, 0, 0);
        } else {
            cl->impulse = ucmd->impulse;
            addCmdQueue(client, QCMD_LOGTOFILE3, 0, 0, 0);
        }

        if (disconnectuserimpulse && checkImpulse(ucmd->impulse)) {
            cl->impulsesgenerated++;

            if (cl->impulsesgenerated >= maximpulses) {
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

    if (!(cl->clientcommand & BANCHECK)) {
        if (zbc_enable && !(cl->clientcommand & CCMD_ZBOTDETECTED)) {
            if (AimbotCheck(client, ucmd)) {
                cl->clientcommand |= (CCMD_ZBOTDETECTED | CCMD_ZPROXYCHECK2);
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

/**
 * Send private_command[1-4] and inverted_command[1-4] to a players
 */
void stuff_private_commands(int client, edict_t *ent) {
    unsigned int i;
    char temp[256];

    proxyinfo[client].private_command = ltime + 10;

    for (i = 0; i < PRIVATE_COMMANDS; i++) {
        if (private_commands[i].command[0]) {
            Q_snprintf(temp, sizeof(temp), "%s\n", private_commands[i].command);
            stuffcmd(ent, temp);
        }
        proxyinfo[client].private_command_got[i] = false;
    }
}

/**
 * Check if a player is using some kind of aim assist. Checks client angles
 * for a large jump between ClientThinks.
 *
 * Originally ZbotCheck v1.01 by Matt "WhiteFang" Ayres (matt@lithium.com)
 *
 * Called from ClientThink()
 */
bool AimbotCheck(int client, usercmd_t *ucmd) {
    int prev, cur;
    aimbot_t *a = &proxyinfo[client].aim_assist;

    prev = a->toggle;
    a->toggle ^= 1;     // was 0 now 1, was 1 now 0
    cur = a->toggle;

    if (ucmd->angles[PITCH] == a->angles[cur][PITCH] &&
            ucmd->angles[YAW] == a->angles[cur][YAW] &&
            ucmd->angles[PITCH] != a->angles[prev][PITCH] &&
            ucmd->angles[YAW] != a->angles[prev][YAW] &&
            abs(ucmd->angles[PITCH] - a->angles[prev][PITCH]) +
            abs(ucmd->angles[YAW] - a->angles[prev][YAW]) >= zbc_jittermove) {
        if (ltime <= a->jitter_last + FRAMETIME) {
            if (!a->jitter) {
                a->jitter_time = ltime;
            }
            if (a->jitter++ >= zbc_jittermax) {
                return true;
            }
        }
        a->jitter_last = ltime;
    }
    a->angles[cur][PITCH] = ucmd->angles[PITCH];
    a->angles[cur][YAW] = ucmd->angles[YAW];

    if (ltime > (a->jitter_time + zbc_jittertime)) {
        a->jitter = 0;
    }
    return false;
}
