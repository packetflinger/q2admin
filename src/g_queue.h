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

typedef struct {
    byte command;
    float timeout;      // more of a "delay", time to wait before processing
    unsigned long data;
    char *str;
} cmd_queue_t;

void addCmdQueue(int client, byte command, float timeout, unsigned long data, char *str);
bool getCommandFromQueue(int client, byte *command, unsigned long *data, char **str);
void removeClientCommand(int client, byte command);
void removeClientCommands(int client);
