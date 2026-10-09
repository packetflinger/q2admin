/**
 * Q2Admin
 */

#include "g_local.h"

bool timers_active = false;
int timers_min_seconds = 10;
int timers_max_seconds = 180;

/**
 * "timer_start <number> <seconds> <action>" - arms one of a player's
 * countdown timers. When it expires, timerAction() stuffs <action> into
 * that same player's console.
 *
 * This is a convenience for players rather than an admin tool: the
 * classic use is a respawn reminder ("quad's up in 60 seconds"), which
 * otherwise needs a client-side script. Because everything it can do
 * happens in the caller's own console, it's safe to leave open to any
 * player rather than gating it behind an admin level.
 *
 * Refuses anything outside the server's configured bounds: a duration
 * outside timers_min_seconds..timers_max_seconds, a slot number outside
 * 1..TIMERS_MAX-1, or a second call while newCommandAllowed()'s cooldown
 * is still running - which is what stops a player re-arming timers every
 * frame. (The refusal message says 5 seconds; that cooldown is actually
 * 3.) Re-using a slot number silently overwrites whatever was in it.
 *
 * client: the calling player's index, whose timer slots are used.
 * ent:    their edict, for replies.
 *
 * The action is taken from a single argument, so anything with spaces
 * has to be quoted at the console, and it's stored verbatim - no newline
 * is appended, so it needs to end with one to actually run rather than
 * just landing in the console input line.
 *
 * Called from doClientCommand() (g_cmd.c) on "timer_start", gated on
 * timers_active.
 */
void timerStart(int client, edict_t *ent) {
    int seconds;
    int num;

    if (gi.argc() < 4) {
        gi.cprintf(ent, PRINT_HIGH, "Incorrect syntax, use: 'timer_start <number> <seconds> <action>'\n");
        return;
    }
    if (!newCommandAllowed(client)) {
        gi.cprintf(ent, PRINT_HIGH, "Please wait 5 seconds\n");
        return; //wait 5 secs before starting the timer again
    }
    num = q2a_atoi(gi.argv(1));
    seconds = q2a_atoi(gi.argv(2));
    if ((seconds < timers_min_seconds) || (seconds > timers_max_seconds)) {
        gi.cprintf(ent, PRINT_HIGH, "Timer seconds falls outside acceptable range of %i to %i.\n", timers_min_seconds, timers_max_seconds);
        return;
    }
    if ((num < 1) || (num >= TIMERS_MAX)) {
        gi.cprintf(ent, PRINT_HIGH, "Invalid timer number\n");
        return;
    }
    proxyinfo[client].timers[num].start = ltime + seconds;
    q2a_strncpy(proxyinfo[client].timers[num].action, gi.argv(3), sizeof(proxyinfo[client].timers[num].action)-1);
}

/**
 * "timer_stop <number>" - cancels one of the caller's timers before it
 * fires, by zeroing its start time (0 being the "unset" marker
 * timerAction() skips over).
 *
 * The counterpart to timerStart(), for when a reminder is no longer
 * wanted - without it a player's only option would be waiting for the
 * action to fire.
 *
 * client: the calling player's index, whose timer slot is cleared.
 * ent:    their edict, for replies.
 *
 * Accepts the same 1..TIMERS_MAX-1 slot numbers as timerStart(), and
 * clearing a slot that was never armed is harmless. Unlike timerStart()
 * this isn't rate limited, since cancelling costs nothing.
 *
 * Called from doClientCommand() (g_cmd.c) on "timer_stop", gated on
 * timers_active.
 */
void timerStop(int client, edict_t *ent) {
    int num;

    if (gi.argc() < 2) {
        gi.cprintf(ent, PRINT_HIGH, "Invalid Timer\n");
        return;
    }
    num = q2a_atoi(gi.argv(1));
    if ((num < 1) || (num >= TIMERS_MAX)) {
        gi.cprintf(ent, PRINT_HIGH, "Invalid timer number\n");
        return;
    }
    proxyinfo[client].timers[num].start = 0;
}

/**
 * Fires any of a player's timers that have come due, stuffing the stored
 * action into their console and clearing the slot so it only runs once.
 * This is the half that actually makes a timer go off - timerStart()
 * only records when and what.
 *
 * Has to be driven from the frame loop rather than scheduled, because a
 * timer is just a target ltime sitting in proxyinfo with nothing
 * watching it; each call is the check for "has that moment passed yet".
 *
 * client: the player whose slots to check.
 * ent:    their edict, which the action is stuffed to.
 *
 * Walks all TIMERS_MAX slots including slot 0, which timerStart()'s
 * validation makes unreachable - so of the four slots only 1-3 can ever
 * be used. It's also called for the -1 "global" proxyinfo slot that the
 * caller's loop starts on, which likewise has no way to get a timer set,
 * so nothing fires there.
 *
 * Timers live in proxyinfo, which ClientConnect() wipes wholesale, so a
 * pending timer can't carry over to whoever next occupies that client
 * slot. It does survive a level change for a player who stays connected.
 *
 * Called from G_RunFrame() (g_main.c) for each client it processes that
 * frame, gated on timers_active.
 */
void timerAction(int client, edict_t *ent) {
    for (int num = 0; num < TIMERS_MAX; num++) {
        if (proxyinfo[client].timers[num].start) {
            if (proxyinfo[client].timers[num].start <= ltime) {
                proxyinfo[client].timers[num].start = 0;
                stuffPlayer(ent, proxyinfo[client].timers[num].action);
            }
        }
    }
}
