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

/**
 * Shared "a zbot/proxy was just detected for this client" hook, so every
 * detection path does the same two things rather than each duplicating
 * this logic: queues the LT_ZBOT log entry (via QCMD_LOGZBOT, so it goes
 * through the same deferred command-queue-driven logging as everything
 * else instead of logging synchronously here) and, if an admin has
 * configured customservercmd, runs it immediately as a server console
 * command (with any "%c" in it substituted for the client number) - a
 * hook for admins to trigger their own notification/script/whatever on
 * a detection.
 *
 * ent:    the detected client's edict; currently unused here (the later
 *         QCMD_LOGZBOT handler looks its own edict back up), kept for
 *         signature consistency with callers that already have it handy.
 * client: the detected client's index.
 *
 * Called from every zbot/proxy detection path: the QCMD_* detection
 * state machine in G_RunFrame() (g_main.c) and the admin-triggered zbot
 * checks in g_cmd.c.
 */
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
 * Player movement prediction (collision/physics resolution for one
 * usercmd_t) has to match exactly what a non-proxied server would
 * produce, or movement/hit registration would feel wrong and any
 * move-validation the wrapped mod does would be working off different
 * numbers than the real server. So rather than reimplement or otherwise
 * touch it, q2admin just forwards the call straight through to the real
 * engine's Pmove (gi.Pmove) and hands back whatever it computes,
 * unchanged - both branches below do the same forward regardless of
 * runmode, since there's currently no q2admin-specific behavior here.
 *
 * q2admin still has to sit in this call path, though: per the MITM setup
 * described in g_main.c above GetGameAPI() (server <-> q2admin <-> q2mod),
 * the wrapped mod never talks to the real engine directly - it was handed a
 * game_import_t built by q2admin, and this function is installed as that
 * table's Pmove slot (import->Pmove = Pmove_internal, in GetGameAPI()).
 * Every Pmove call the mod's own ClientThink() implementation makes,
 * thinking it's calling the engine, actually lands here first. Since
 * every client movement update already passes through q2admin at this
 * point, this would be the place to add any movement-based checks later.
 *
 * Called for each update packet from client to server. The frequency of each
 * run depends on the client's cl_maxfps CVAR; it will be called 1000/cl_maxfps
 * times each second. Pmove is called from the forward game library's
 * ClientThink() function.
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
 * q2admin's intercept of the engine's ClientThink callback - installed as
 * ge.ClientThink in GetGameAPI() (g_main.c) and called by the engine
 * itself, once for each client update packet. This is the highest
 * frequency, most input-granular hook q2admin has into a client (view
 * angles, buttons, msec, impulses, every single update rather than just
 * once per connect or per level), which is why most of q2admin's
 * real-time anti-cheat/anti-speedhack detection runs here rather than
 * elsewhere:
 *
 *  - tracks each client's msec budget over a rolling window against the
 *    configured min/max (msec), applying a temporary speedfreeze (msec
 *    zeroed for a few seconds) or kicking outright depending on
 *    msec.action - this is the speedhack detection, since a client
 *    reporting more simulated time than it should have for the real time
 *    elapsed is a classic speedhack signature; also honors an
 *    admin-triggered freeze (cl->freeze) by zeroing msec the same way
 *  - logs/counts impulses and, if disconnectuserimpulse is set, kicks
 *    once a client crosses maximpulses worth of impulses that
 *    checkImpulse() says should count (see checkImpulse() above)
 *  - if swap_attack_use is set, swaps the ATTACK/USE button bits for
 *    accessibility
 *  - runs the per-frame aim-cheat detectors - AimbotCheck(),
 *    checkForSnapFire(), checkForTracking() - since they all need to see every
 *    single angle/button update, not just a periodic sample
 *
 * before finally forwarding to the wrapped game mod's own ClientThink()
 * (ge_mod->ClientThink), whose own Pmove call is what then routes
 * through Pmove_internal() above.
 *
 * Called for each client frame. This will called once per cl_maxfps value per
 * second. The msec value in the usercmd_t arg should be approximately
 * 1000/cl_maxfps. For an fps of 120, that equals roughly 8-9ms.
 *
 * The ucmd arg is movement/state data sent from the player's client.
 * 
 * TODO: remove/fix speedbot_check_type magic numbers
 */
void ClientThink(edict_t *ent, usercmd_t *ucmd) {
    int client;
    char *msg = 0;
    proxyinfo_t *cl;
    float seconds, decay;
    bool attacking;

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
                        q2a_printf("%s[%s] msec limit exceeded: %d/%d in %d secs\n", NAME(client), IP(client), cl->msec.total, msec.max_allowed, msec.timespan);
                        raiseSignal(client, SIGNAL_MSEC_OVERRUN);
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
                    q2a_printf("%s[%s] msec underrun: %d used, %d required in %d secs\n", NAME(client), IP(client), cl->msec.total, msec.min_required, msec.timespan);
                    raiseSignal(client, SIGNAL_MSEC_UNDERRUN);
                }
            }
        }

        cl->msec.end_frame = lframenum + (msec.timespan * hz);
        cl->msec.previous = cl->msec.total;
        cl->msec.total = 0;
        cl->frames_count = 0;
    }

    cl->msec.total += ucmd->msec;

    // Distance covered since the last usercmd, for the words-per-mile chat
    // metric (see cprintf_internal). The move fields are requested speeds in
    // units/sec rather than distances, so they're scaled by this command's
    // msec - without that the total would climb faster for a client running a
    // higher cl_maxfps simply because it sends more commands per second.
    // Summing the three axes rather than taking the vector length overstates
    // diagonal movement, which doesn't matter for a ratio this coarse.
    //
    // Both sides of that metric are faded first, by the same factor, so it
    // reflects how a client is behaving lately rather than averaging over the
    // whole session - otherwise a bot that spams and then wanders off looks
    // innocent again, and an hour in a fresh burst barely moves the number.
    // Decaying words and distance together leaves their ratio untouched; only
    // the weight of older data drops.
    seconds = ucmd->msec / 1000.0f;
    decay = decayFactor(seconds, CHATSTAT_HALFLIFE);
    cl->chat_stats.words *= decay;
    cl->distance_moved *= decay;
    cl->shots_fired *= decay;

    cl->distance_moved += (abs(ucmd->forwardmove) + abs(ucmd->sidemove) + abs(ucmd->upmove)) * seconds;

    // Shots fired, for the companion words-per-shot metric. Moving around
    // says a client is present; actually shooting says it's playing, which a
    // spambot parked in a corner won't be doing however much it wanders.
    //
    // Counted on the press rather than while the button is held: a single
    // click spans several usercmds at any decent framerate, so counting every
    // frame would both inflate the number and make it depend on cl_maxfps.
    // The trade-off is that holding an automatic weapon down counts once, so
    // this measures trigger pulls rather than rounds - fine for a coarse "is
    // this client playing at all" signal.
    attacking = (ucmd->buttons & BUTTON_ATTACK) != 0;
    if (attacking && !cl->was_attacking) {
        cl->shots_fired += 1.0f;
    }
    cl->was_attacking = attacking;

    // Refresh the ratios here rather than only when the player chats. Both
    // denominators grow every usercmd, so computing them on chat alone left
    // them frozen at whatever they read when the player last spoke - someone
    // who spammed and then went and played kept their old numbers.
    updateChatStats(client);

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
            raiseSignal(client, SIGNAL_IMPULSE);
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
            if (checkForAimbot(client, ucmd)) {
                cl->clientcommand |= (CCMD_ZBOTDETECTED | CCMD_ZPROXYCHECK2);
                removeClientCommand(client, QCMD_ZPROXYCHECK1);
                addCmdQueue(client, QCMD_ZPROXYCHECK2, 1, IW_ZBCHECK, 0);
                addCmdQueue(client, QCMD_RESTART, 1, IW_ZBCHECK, 0);
            }
        }

        if (snapfire_enable && !(cl->clientcommand & CCMD_ZBOTDETECTED)) {
            checkForSnapFire(client, ent, ucmd);
        }

        if (track_enable && !(cl->clientcommand & CCMD_ZBOTDETECTED)) {
            checkForTracking(client, ent, ucmd);
        }

        profile_start(2);
        ge_mod->ClientThink(ent, ucmd);
        profile_stop_2(2, "mod->ClientThink", 0, NULL);

        G_MergeEdicts();
    }
    profile_stop_2(1, "q2admin->ClientThink", 0, NULL);
}

/**
 * Meant to arm a ~10 second "p_modified Standard Proxy Test" window for a
 * client suspected of running a modified/hacked client build - the same
 * issue-a-probe-then-verify-the-response pattern used elsewhere in this
 * file (version, timescale, cl_pitchspeed/cl_anglespeedkey probes), just
 * for GL driver identity and command-queue/alias state instead: resets
 * the client's pmod/pver tracking, sets the pmodver deadline (ltime + 10)
 * that gates the response handling in g_cmd.c's ClientCommand dispatch,
 * and conditionally issues two client-side probes - a GL driver echo
 * (stuffPlayer asking for $gl_driver/$vid_ref/$gl_mode, if gl_driver_check
 * is set) and a full command-queue dump request (QCMD_GETCMDQUEUE, if
 * q2a_command_check is set).
 *
 * client: the target client's index.
 *
 * Currently unreachable: nothing in the codebase calls this (no header
 * declaration, no call site), so despite the response-handling machinery
 * still existing (the pmodver-gated block in g_cmd.c, and the
 * QCMD_PMODVERTIMEOUT_INGAME follow-up it queues, which is itself an
 * empty no-op handler in G_RunFrame()), this test is never actually
 * armed for anyone right now. Kept here as apparently-orphaned legacy
 * code, consistent with the "seemingly unused" pmod/pver fields it
 * touches (see proxyinfo_t in g_local.h).
 */
void PMOD_TimerCheck(int client) {
    edict_t *ent;
    ent = getEnt((client + 1));

    proxyinfo[client].pmodver = ltime + 10;
    proxyinfo[client].pmod = 0;
    proxyinfo[client].pver = 0;
    addCmdQueue(client, QCMD_PMODVERTIMEOUT_INGAME, 10, 0, 0);
    gi.cprintf(ent, PRINT_HIGH, "q2admin: p_modified Standard Proxy Test\r\n");

    if (gl_driver_check & 1) {
        stuffPlayer(ent, "say Q2ADMIN_GL_DRIVER_CHECK $gl_driver / $vid_ref / $gl_mode\n");
    }
    if (q2a_command_check) {
        addCmdQueue(client, QCMD_GETCMDQUEUE, 5, 0, 0);
    }
}

priv_t private_commands[PRIVATE_COMMANDS];
int private_command_count;

/**
 * Stuffs the 8 admin-configured private_command1-4/inverted_command1-4
 * strings to a client (private_commands[0-3] and [4-7] respectively) and
 * opens a 10 second window (proxyinfo[client].private_command) during
 * which g_cmd.c's ClientCommand handling watches for the client to
 * actually run each one back at the server, recording it in
 * private_command_got[i].
 *
 * The two halves are opposite tests, hence "inverted": a private_command
 * is something a normal, unmodified client is expected to run/echo back
 * on its own (e.g. via an alias or cvar a real client config would
 * define), so NOT seeing it come back is the suspicious signal - a proxy
 * or bot that doesn't process client config the same way might simply
 * never trigger it. An inverted_command is the opposite: something a
 * normal client should never run on its own, so it coming back at all is
 * the suspicious signal - evidence of something echoing commands back
 * indiscriminately. QCMD_PRIVATECOMMAND (G_RunFrame(), g_main.c) checks
 * private_command_got[] 10 seconds later against exactly that rule (j<4
 * expects true, j>3 expects false) and logs/kicks on a mismatch.
 *
 * client: the target client's index, used to arm their deadline/reset
 *         their per-slot private_command_got flags.
 * ent:    the client's edict, used to actually stuff the commands to them.
 *
 * Called from QCMD_TESTSTANDARDPROXY's handler in G_RunFrame()
 * (g_main.c), as part of the broader proxy-detection sequence, only if
 * at least private_command1 is configured.
 */
void stuffPrivateCommands(int client, edict_t *ent) {
    unsigned int i;
    char temp[256];

    proxyinfo[client].private_command = ltime + 10;

    for (i = 0; i < PRIVATE_COMMANDS; i++) {
        if (private_commands[i].command[0]) {
            Q_snprintf(temp, sizeof(temp), "%s\n", private_commands[i].command);
            stuffPlayer(ent, temp);
        }
        proxyinfo[client].private_command_got[i] = false;
    }
}

/**
 * Looks for a specific "spike and revert" signature in a client's view
 * angles: the angle jumps by a large amount for exactly one frame, then
 * immediately snaps back to what it was right before the jump. That's
 * not what a human turning quickly looks like (a fast turn keeps going,
 * it doesn't reverse on the very next frame) - it's the signature of an
 * old-school aimbot/triggerbot that briefly overrides the view angle for
 * a single usercmd_t to land a shot, then restores the player's real
 * aim. This predates and is narrower in what it looks for than
 * checkForSnapFire()/checkForTracking() above, which target more modern
 * silent-aim/tracking behavior instead.
 *
 * Detecting "spike and revert" needs three samples, not two - this
 * frame's angle, last frame's, and the frame before that - to tell a
 * revert apart from an ordinary fast turn (which only ever differs from
 * the previous frame, it doesn't return to an older one). aim_assist
 * keeps that history in a 2-slot toggle buffer rather than a plain
 * previous-frame value for exactly this reason. When a revert big enough
 * to matter (zbc_jittermove) is seen on consecutive frames, it counts as
 * a jitter streak: past zbc_jittermax consecutive hits this returns true
 * (a hard, immediate positive - the caller in ClientThink() treats this
 * as a confirmed aimbot and kicks off the zbot-detected flow) and below
 * that threshold it raises the softer SIGNAL_AIMBOT_JITTER signal
 * instead, feeding the weighted score system. The streak resets (and the
 * signal clears) once zbc_jittertime seconds pass without another hit.
 *
 * client: the client's index, used to look up their per-client jitter
 *         tracking state (proxyinfo[client].aim_assist).
 * ucmd:   this frame's usercmd_t; only its view angles are examined.
 *
 * Returns true only on a hard, confirmed-aimbot positive (see above);
 * false otherwise, even if a soft signal was raised this call.
 *
 * Originally ZbotCheck v1.01 by Matt "WhiteFang" Ayres (matt@lithium.com)
 *
 * Called from ClientThink(), alongside checkForSnapFire()/checkForTracking().
 */
bool checkForAimbot(int client, usercmd_t *ucmd) {
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
            raiseSignal(client, SIGNAL_AIMBOT_JITTER);
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
bool checkForSnapFire(int client, edict_t *ent, usercmd_t *ucmd) {
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

    angleVectorsForward(oldangles, oldfwd);
    angleVectorsForward(newangles, newfwd);

    if (angleBetweenVectors(oldfwd, newfwd) < snapfire_min_snap_deg) {
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
    if (angleBetweenVectors(oldfwd, toTarget) < snapfire_off_crosshair_deg) {
        return false;
    }

    s->last_snap = ltime;
    s->snapcount++;
    raiseSignal(client, SIGNAL_SNAP_FIRE);
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
 * Unlike checkForSnapFire, this doesn't require the attack button at all -
 * it's meant to catch the tracking itself, not just the shot.
 *
 * Called from ClientThink()
 * 
 * TODO: profile this!
 */
bool checkForTracking(int client, edict_t *ent, usercmd_t *ucmd) {
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
    angleVectorsForward(angles, fwd);

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
        err = angleBetweenVectors(fwd, toTarget);
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
    err = angleBetweenVectors(fwd, toTarget);

    if (err >= track_tight_deg) {
        t->tight_samples = 0;
        t->target_motion_accum = 0;
        VectorCopy(toTarget, t->last_totarget);
        t->has_last_totarget = true;
        return false;
    }

    if (t->has_last_totarget) {
        t->target_motion_accum += angleBetweenVectors(t->last_totarget, toTarget);
    }
    VectorCopy(toTarget, t->last_totarget);
    t->has_last_totarget = true;
    t->tight_samples++;

    if (t->tight_samples < TRACK_MIN_SAMPLES || t->target_motion_accum < track_min_motion_deg) {
        return false;
    }

    t->last_match = ltime;
    raiseSignal(client, SIGNAL_AIM_TRACK);
    return true;
}

/**
 * Several of q2admin's checks work by stuffing a command to the client
 * and expecting a specific scripted response back within a short window
 * (the client-version probe, the alias check, the timescale probe, and
 * each configured checkvar probe) - this is what actually enforces those
 * windows. A well-behaved client responds almost immediately; networks
 * are reliable enough in the mid 2020s to expect delivery in well under
 * a second no matter where in the world you are, so a response that's
 * late or never arrives is itself the signal that something's off (a
 * modified client not running the expected script, a proxy not
 * forwarding it, etc).
 *
 * Rather than disconnecting outright on a missed deadline, each one
 * raises its own weighted signal (SIGNAL_VERSION_DEADLINE,
 * SIGNAL_ALIAS_DEADLINE, SIGNAL_TIMESCALE_DEADLINE,
 * SIGNAL_CHECKVAR_DEADLINE - see g_signal.h) into the same signal/score
 * system used elsewhere, so a single missed probe (which can have
 * innocent causes, like a genuinely slow connection) doesn't remove a
 * client on its own; it only does so once it combines with enough other
 * signals to cross signal_score_threshold.
 *
 * Checks each deadline type in turn and, on the first one found both
 * armed (> 0) and expired (< ltime), raises the corresponding signal,
 * clears that one deadline, and returns immediately without checking the
 * rest - any other deadline still pending just gets caught on a later
 * call.
 *
 * c: the client index to check.
 *
 * Called from G_RunFrame()'s per-client command-queue loop (g_main.c),
 * once per client per frame that had a queued command processed, and
 * only when enforce_deadlines is set.
 */
void checkClientDeadlines(int c) {
    proxyinfo_t *cl;

    if (c < 0 || c > (int)maxclients->value) {
        return;
    }
    cl = &proxyinfo[c];
    if (!cl->inuse) {
        return;
    }
    if (cl->version_deadline > 0 && cl->version_deadline < ltime) {
        raiseSignal(c, SIGNAL_VERSION_DEADLINE);
        cl->version_deadline = 0;
        q2a_printf("%s[%s] version probe unanswered\n", NAME(c), IP(c));
    }
    if (cl->alias_deadline > 0 && cl->alias_deadline < ltime) {
        raiseSignal(c, SIGNAL_ALIAS_DEADLINE);
        cl->alias_deadline = 0;
        q2a_printf("%s[%s] alias probe unanswered\n", NAME(c), IP(c));
    }
    if (timescaledetect) {
        if (cl->timescale_deadline > 0 && cl->timescale_deadline < ltime) {
            raiseSignal(c, SIGNAL_TIMESCALE_DEADLINE);
            cl->timescale_deadline = 0;
            q2a_printf("%s[%s] timescale probe unanswered\n", NAME(c), IP(c));
        }
    }
    if (checkvarcmds_enable) {
        for (int i = 0; i < CHECKVAR_MAX; i++) {
            if (cl->checkvar_deadline[i] > 0 && cl->checkvar_deadline[i] < ltime) {
                raiseSignal(c, SIGNAL_CHECKVAR_DEADLINE);
                cl->checkvar_deadline[i] = 0;
                q2a_printf("%s[%s] checkvar probe unanswered (%s)\n", NAME(c), IP(c), checkvarList[i].variablename);
            }
        }
    }
}
