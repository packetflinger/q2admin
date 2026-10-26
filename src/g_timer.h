/**
 * Copyright (C) The Q2Admin contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <https://www.gnu.org/licenses/>.
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
