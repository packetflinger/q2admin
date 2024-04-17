/**
 * Q2Admin
 * Client queue related stuff
 */

#pragma once

void addCmdQueue(int client, byte command, float timeout, unsigned long data, char *str);
qboolean getCommandFromQueue(int client, byte *command, unsigned long *data, char **str);
void removeClientCommand(int client, byte command);
void removeClientCommands(int client);
