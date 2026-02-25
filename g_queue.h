/**
 * Q2Admin
 * Client queue related stuff
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
