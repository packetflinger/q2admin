/**
 * Q2Admin
 */

#include "g_local.h"

bool timers_active = false;
int timers_min_seconds = 10;
int timers_max_seconds = 180;

/**
 *
 */
void timer_start(int client, edict_t *ent) {
    int seconds;
    int num;

    if (gi.argc() < 4) {
        gi.cprintf(ent, PRINT_HIGH, "Incorrect syntax, use: 'timer_start <number> <seconds> <action>'\n");
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

/**
 *
 */
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

/**
 *
 */
void timer_action(int client, edict_t *ent) {
    int num;
    for (num = 0; num < TIMERS_MAX; num++) {
        if (proxyinfo[client].timers[num].start) {
            if (proxyinfo[client].timers[num].start <= ltime) {
                proxyinfo[client].timers[num].start = 0;
                stuffcmd(ent, proxyinfo[client].timers[num].action);
            }
        }
    }
}
