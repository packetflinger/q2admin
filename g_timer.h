/**
 * Q2Admin
 * Timer stuff
 */

#pragma once

#define TIMERS_MAX      4

typedef struct {
    char action[256];
    int start;
} timers_t;

extern qboolean timers_active;
extern int timers_min_seconds;
extern int timers_max_seconds;

void timer_action(int client, edict_t *ent);
void timer_start(int client, edict_t *ent);
void timer_stop(int client, edict_t *ent);
