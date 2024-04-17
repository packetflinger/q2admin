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

void timer_action(int client, edict_t *ent);
void timer_start(int client, edict_t *ent);
void timer_stop(int client, edict_t *ent);
