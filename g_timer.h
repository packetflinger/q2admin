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

extern bool timers_active;
extern int timers_min_seconds;
extern int timers_max_seconds;

void timerAction(int client, edict_t *ent);
void timerStart(int client, edict_t *ent);
void timerStop(int client, edict_t *ent);
