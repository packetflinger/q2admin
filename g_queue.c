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

/**
 * Add an operation (command) to the queue for a particular client. The queue
 * is processed one item per frame run. The only exception is the QCMD_DISCONNECT
 * command, that is handled immediately, not waiting for the next runframe()
 * and regardless of how many items are in the queue ahead of it.
 *
 * Args:
 *   client -  The proxyinfo index of the client this applies to
 *   command - What operation should be done. These are the QCMD_* values
 *   timeout - Seconds from now when this operation is considered expired. This
 *             is a float in seconds in the future relative to the q2admin's
 *             `ltime` var. The timeout decides whether the command will be
 *             run, not how long it takes for the result.
 *   data -    A long usually an array index for something.
 *   str -     A string relevant to the operations. For example when kicking
 *             a client, this would be the message displayed regarding the kick
 */
void addCmdQueue(int client, byte command, float timeout, unsigned long data, char *str) {
    char tmptext[128];

    proxyinfo[client].cmdQueue[proxyinfo[client].maxCmds].command = command;
    proxyinfo[client].cmdQueue[proxyinfo[client].maxCmds].timeout = ltime + timeout;
    proxyinfo[client].cmdQueue[proxyinfo[client].maxCmds].data = data;
    proxyinfo[client].cmdQueue[proxyinfo[client].maxCmds].str = str;
    proxyinfo[client].maxCmds++;

    if (command == QCMD_DISCONNECT) {
        gi.cprintf(NULL, PRINT_HIGH, "%s is being disconnected: %s.\n", NAME(client), str);
    }

    if (proxyinfo[client].maxCmds >= ALLOWED_MAXCMDS_SAFETY) {
        proxyinfo[client].clientcommand |= CCMD_KICKED;
        gi.bprintf(PRINT_HIGH, "%s tried to flood the server.\n", NAME(client));
        Q_snprintf(tmptext, sizeof(tmptext), "kick %d\n", client);
        gi.AddCommandString(tmptext);
    }
}

/**
 * Pop the next command off the client's queue. If the timeout is past the
 * current time, the command is skipped. Expired commands eventually get
 * shifted out of the array.
 */
bool getCommandFromQueue(int client, byte *command, unsigned long *data, char **str) {
    unsigned int i;

    for (i = 0; i < proxyinfo[client].maxCmds; i++) {
        if (proxyinfo[client].cmdQueue[i].timeout < ltime) {
            *command = proxyinfo[client].cmdQueue[i].command;
            *data = proxyinfo[client].cmdQueue[i].data;

            if (str) {
                *str = proxyinfo[client].cmdQueue[i].str;
            }

            // remove command
            proxyinfo[client].maxCmds--;

            // the queue has multiple commands, shift them over
            if (i < proxyinfo[client].maxCmds) {
                q2a_memmove(
                    proxyinfo[client].cmdQueue + i,
                    proxyinfo[client].cmdQueue + i + 1,
                    (proxyinfo[client].maxCmds - i) * sizeof(cmd_queue_t)
                );
            }
            return true;
        }
    }
    return false;
}

/**
 * Delete a command from the client's queue
 */
void removeClientCommand(int client, byte command) {
    unsigned int i = 0;

    while (i < proxyinfo[client].maxCmds) {
        if (proxyinfo[client].cmdQueue[i].command == command) {
            proxyinfo[client].maxCmds--;
            if (i < proxyinfo[client].maxCmds) {
                q2a_memmove(
                    proxyinfo[client].cmdQueue + i,
                    proxyinfo[client].cmdQueue + i + 1,
                    (proxyinfo[client].maxCmds - i) * sizeof(cmd_queue_t)
                );
            }
        } else {
            i++;
        }
    }
}

/**
 * Delete all commands from the queue.
 */
void removeClientCommands(int client) {
    proxyinfo[client].maxCmds = 0;
}
