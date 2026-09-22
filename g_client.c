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

bool snapfire_enable = true;
int snapfire_min_snap_deg = 30;      // minimum 1-frame view change to consider a "snap"
int snapfire_off_crosshair_deg = 40; // how far off-crosshair the target had to be beforehand

bool track_enable = true;
int track_tight_deg = 4;        // crosshair error (degrees) counted as "on target"
int track_min_motion_deg = 15;  // total target angular movement (degrees) required during a tight streak


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

/**
 * Whether an impulse number should count toward the disconnectuserimpulse
 * kick threshold. Players send all sorts of harmless impulses constantly
 * (weapon switching, etc), so admins can scope detection down to just the
 * ones that actually matter - by default the zbot menu toggle impulses
 * (169-175, see impulsemessages[] above) - via the impulsestokickon
 * config command, rather than treating every single impulse as
 * suspicious. Per that command's documented behavior, leaving it
 * unconfigured (maxImpulses == 0) matches every impulse.
 *
 * impulse: the impulse number from the client's usercmd_t this frame.
 *
 * Returns true if this impulse is one that should count (either it's in
 * impulsesToKickOn, or nothing was configured so everything counts).
 *
 * Called from ClientThink(), to gate the disconnectuserimpulse
 * impulse-counting/kick logic.
 */
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
    addCmdQueue(client, QCMD_LOGZBOT, 0, 0, 0);

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

        cl->msec.end_frame = lframenum + (msec.timespan * hz);
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
            addCmdQueue(client, QCMD_LOGZBOTIMPULSE, 0, 0, 0);
        } else {
            cl->impulse = ucmd->impulse;
            addCmdQueue(client, QCMD_LOGIMPULSE, 0, 0, 0);
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

        if (snapfire_enable && !(cl->clientcommand & CCMD_ZBOTDETECTED)) {
            SnapFireCheck(client, ent, ucmd);
        }

        if (track_enable && !(cl->clientcommand & CCMD_ZBOTDETECTED)) {
            TrackingCheck(client, ent, ucmd);
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
        if (ltime <= a->jitter_last + frametime) {
            if (!a->jitter) {
                a->jitter_time = ltime;
            }
            if (a->jitter++ >= zbc_jittermax) {
                return true;
            }
            raiseSignal(client, SIGNAL_AIMBOT_JITTER);
            evaluateSignalScore(client);
        }
        a->jitter_last = ltime;
    }
    a->angles[cur][PITCH] = ucmd->angles[PITCH];
    a->angles[cur][YAW] = ucmd->angles[YAW];

    if (ltime > (a->jitter_time + zbc_jittertime)) {
        a->jitter = 0;
        clearSignal(client, SIGNAL_AIMBOT_JITTER);
    }
    return false;
}

// a point trace against MASK_SHOT is what actual hitscan weapons use to
// find their target, so this mirrors that rather than inventing a new mask
#define SNAPFIRE_EYE_HEIGHT     22    // approx standing viewheight; crouch state isn't visible to q2admin
#define SNAPFIRE_RANGE          8192  // effectively the whole map
#define SNAPFIRE_SIGNAL_DECAY   3     // seconds a detected snap-fire stays visible as a signal

/**
 * Looks for a player's view snapping onto another player who wasn't
 * anywhere near their crosshair the previous frame, exactly as they open
 * fire. A human tracking a target they can see doesn't produce this: their
 * crosshair is already somewhere near the target before they shoot. A
 * silent-aim/aimbot lock-on does: nothing, then instantly on-target and
 * firing in the same frame.
 *
 * Called from ClientThink()
 */
bool SnapFireCheck(int client, edict_t *ent, usercmd_t *ucmd) {
    aimsnap_t *s = &proxyinfo[client].aimsnap;
    vec3_t oldangles, newangles, oldfwd, newfwd, eye, end, toTarget, zero = {0, 0, 0};
    bool attacking, attackPressed;
    trace_t tr;

    attacking = (ucmd->buttons & BUTTON_ATTACK) != 0;
    attackPressed = attacking && !s->wasattacking;
    s->wasattacking = attacking;

    if ((proxyinfo[client].signalMask & SIGNAL_SNAP_FIRE) && ltime > s->last_snap + SNAPFIRE_SIGNAL_DECAY) {
        clearSignal(client, SIGNAL_SNAP_FIRE);
    }

    if (!s->haslast) {
        s->lastangles[PITCH] = ucmd->angles[PITCH];
        s->lastangles[YAW] = ucmd->angles[YAW];
        s->haslast = true;
        return false;
    }

    oldangles[PITCH] = SHORT2ANGLE(s->lastangles[PITCH]);
    oldangles[YAW] = SHORT2ANGLE(s->lastangles[YAW]);
    oldangles[ROLL] = 0;
    newangles[PITCH] = SHORT2ANGLE(ucmd->angles[PITCH]);
    newangles[YAW] = SHORT2ANGLE(ucmd->angles[YAW]);
    newangles[ROLL] = 0;

    s->lastangles[PITCH] = ucmd->angles[PITCH];
    s->lastangles[YAW] = ucmd->angles[YAW];

    if (!attackPressed) {
        return false;
    }

    AngleVectorsForward(oldangles, oldfwd);
    AngleVectorsForward(newangles, newfwd);

    if (AngleBetweenVectors(oldfwd, newfwd) < snapfire_min_snap_deg) {
        return false;
    }

    VectorCopy(ent->s.origin, eye);
    eye[2] += SNAPFIRE_EYE_HEIGHT;
    VectorMA(eye, SNAPFIRE_RANGE, newfwd, end);

    tr = gi.trace(eye, zero, zero, end, ent, MASK_SHOT);
    if (!tr.ent || tr.ent == ent || !tr.ent->client) {
        return false;
    }

    // was the target already roughly where they were looking?
    VectorSubtract(tr.ent->s.origin, eye, toTarget);
    if (AngleBetweenVectors(oldfwd, toTarget) < snapfire_off_crosshair_deg) {
        return false;
    }

    s->last_snap = ltime;
    raiseSignal(client, SIGNAL_SNAP_FIRE);
    evaluateSignalScore(client);
    return true;
}

#define TRACK_EYE_HEIGHT         22   // approx standing viewheight; crouch state isn't visible to q2admin
#define TRACK_RANGE              8192 // effectively the whole map
#define TRACK_ENGAGE_FOV_DEG     15   // only bother evaluating a target already within this many degrees of the crosshair
#define TRACK_MIN_SAMPLES        15   // consecutive tight-tracking ClientThink samples required before raising the signal
#define TRACK_SIGNAL_DECAY       2    // seconds a detected tracking streak stays visible as a signal

/**
 * Looks for a player's crosshair staying implausibly close to a visible
 * enemy for a sustained streak while that enemy is actually moving across
 * their view. A human tracking a moving target is noisy - small over/
 * under-corrections keep breaking a "dead on target" streak. A smoothed
 * silent aim glues the crosshair to the target and keeps it there, so the
 * streak survives real target movement instead of getting interrupted by
 * human correction jitter.
 *
 * Unlike SnapFireCheck, this doesn't require the attack button at all -
 * it's meant to catch the tracking itself, not just the shot.
 *
 * Called from ClientThink()
 */
bool TrackingCheck(int client, edict_t *ent, usercmd_t *ucmd) {
    aimtrack_t *t = &proxyinfo[client].aimtrack;
    vec3_t angles, fwd, eye, end, toTarget, zero = {0, 0, 0};
    edict_t *cand;
    int candnum, i;
    float err, besterr;
    trace_t tr;

    if ((proxyinfo[client].signalMask & SIGNAL_AIM_TRACK) && ltime > t->last_match + TRACK_SIGNAL_DECAY) {
        clearSignal(client, SIGNAL_AIM_TRACK);
    }

    angles[PITCH] = SHORT2ANGLE(ucmd->angles[PITCH]);
    angles[YAW] = SHORT2ANGLE(ucmd->angles[YAW]);
    angles[ROLL] = 0;
    AngleVectorsForward(angles, fwd);

    VectorCopy(ent->s.origin, eye);
    eye[2] += TRACK_EYE_HEIGHT;

    // nearest other player already close to the crosshair
    cand = NULL;
    besterr = TRACK_ENGAGE_FOV_DEG;
    for (i = 0; i < (int)maxclients->value; i++) {
        edict_t *other;

        if (i == client || !proxyinfo[i].inuse) {
            continue;
        }
        other = getEnt(i + 1);
        if (!other->inuse || !other->client) {
            continue;
        }
        VectorSubtract(other->s.origin, eye, toTarget);
        err = AngleBetweenVectors(fwd, toTarget);
        if (err < besterr) {
            besterr = err;
            cand = other;
        }
    }

    if (!cand) {
        t->targetnum = -1;
        t->tight_samples = 0;
        t->target_motion_accum = 0;
        t->has_last_totarget = false;
        return false;
    }

    // confirm it's actually a clean shot, not e.g. someone through a wall
    VectorMA(eye, TRACK_RANGE, fwd, end);
    tr = gi.trace(eye, zero, zero, end, ent, MASK_SHOT);
    if (tr.ent != cand) {
        t->targetnum = -1;
        t->tight_samples = 0;
        t->target_motion_accum = 0;
        t->has_last_totarget = false;
        return false;
    }

    candnum = getEntOffset(cand) - 1;
    if (t->targetnum != candnum) {
        t->targetnum = candnum;
        t->tight_samples = 0;
        t->target_motion_accum = 0;
        t->has_last_totarget = false;
    }

    VectorSubtract(cand->s.origin, eye, toTarget);
    err = AngleBetweenVectors(fwd, toTarget);

    if (err >= track_tight_deg) {
        t->tight_samples = 0;
        t->target_motion_accum = 0;
        VectorCopy(toTarget, t->last_totarget);
        t->has_last_totarget = true;
        return false;
    }

    if (t->has_last_totarget) {
        t->target_motion_accum += AngleBetweenVectors(t->last_totarget, toTarget);
    }
    VectorCopy(toTarget, t->last_totarget);
    t->has_last_totarget = true;
    t->tight_samples++;

    if (t->tight_samples < TRACK_MIN_SAMPLES || t->target_motion_accum < track_min_motion_deg) {
        return false;
    }

    t->last_match = ltime;
    raiseSignal(client, SIGNAL_AIM_TRACK);
    evaluateSignalScore(client);
    return true;
}

/**
 * Many checks require issuing commands to clients and checking for appropriate
 * responses. These deadlines ensure the client actually does respond in a
 * timely manner and assumes shenanigans if responses are delayed or never
 * arrive.
 *
 * Networks are reliable enough in the mid 2020s to expect delivery in less
 * than a second no matter where in the world you are.
 */
void checkClientDeadlines(int c) {
    if (c < 0 || c > (int)maxclients->value) {
        return;
    }
    if (proxyinfo[c].version_deadline > 0 && proxyinfo[c].version_deadline < ltime) {
        gi.cprintf(proxyinfo[c].ent, PRINT_HIGH, "Client failed to respond as expected\n");
        addCmdQueue(c, QCMD_DISCONNECT, 1, 0, "no response to version request");
        proxyinfo[c].version_deadline = 0;
        return;
    }
    if (proxyinfo[c].alias_deadline > 0 && proxyinfo[c].alias_deadline < ltime) {
        gi.cprintf(proxyinfo[c].ent, PRINT_HIGH, "Client failed to respond as expected\n");
        addCmdQueue(c, QCMD_DISCONNECT, 1, 0, "no response to alias request");
        proxyinfo[c].alias_deadline = 0;
        return;
    }
    if (timescaledetect) {
        if (proxyinfo[c].timescale_deadline > 0 && proxyinfo[c].timescale_deadline < ltime) {
            gi.cprintf(proxyinfo[c].ent, PRINT_HIGH, "Client failed to respond as expected\n");
            addCmdQueue(c, QCMD_DISCONNECT, 1, 0, "no response to timescale request");
            proxyinfo[c].timescale_deadline = 0;
            return;
        }
    }
    if (checkvarcmds_enable) {
        for (int i = 0; i < CHECKVAR_MAX; i++) {
            if (proxyinfo[c].checkvar_deadline[i] > 0 && proxyinfo[c].checkvar_deadline[i] < ltime) {
                gi.cprintf(proxyinfo[c].ent, PRINT_HIGH, "Client failed to respond as expected\n");
                addCmdQueue(c, QCMD_DISCONNECT, 1, 0, va("no response to checkvar request [%s]", checkvarList[i].variablename));
                proxyinfo[c].checkvar_deadline[i] = 0;
                return;
            }
        }
    }
}

/**
 * Get a string representation of the hack a client is suspected of using.
 */
char *hacktypeToString(hacktype_t h) {
    switch (h) {
    case HT_NONE:
        return "none";
    case HT_GENERAL_PROXY:
        return "proxy";
    case HT_GENERAL_AIMBOT:
        return "aim assist";
    case HT_ZBOT:
        return "zbot";
    case HT_RATBOT:
        return "ratbot";
    case HT_CUSTOM_CLIENT:
        return "modified client";
    case HT_MSEC:
        return "MSEC manipulation";
    case HT_TIMESCALE:
        return "timescale manipulation";
    case HT_ALIAS:
        return "unexpected alias behavior";
    case HT_STUFF:
        return "stufftext non-compliance";
    case HT_USERINFO:
        return "non-standard userinfo";
    case HT_UNKNOWN:
        return "unknown";
    }
    return "";
}
