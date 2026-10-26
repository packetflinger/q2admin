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

#define VOTEFILE        "q2a_vote.cfg"
#define VOTECMD         "[sv] !votecmd [SW/EX/RE] \"command\"\n"
#define VOTEDELCMD      "[sv] !votedel votenum\n"
#define VOTE_MAXCMDS    1024

#define DEFAULTVOTECOMMAND  "vote"

#define VOTE_SW  0
#define VOTE_EX  1
#define VOTE_RE  2

typedef struct {
    char *votecmd;
    byte type;
    re_t r;
} votecmd_t;

bool checkForVoteCmd(char *cp, int votecmd);
void checkOnVoting(void);
bool checkVoteCommand(char *votecmd);
void displayNextVote(edict_t *ent, int client, long floodcmd);
void displayVote(void);
void freeVoteLists(void);
void listvotesRun(int startarg, edict_t *ent, int client);
bool readVoteFile(char *votename);
void readVoteLists(void);
void reloadVoteFileRun(int startarg, edict_t *ent, int client);
void runVote(edict_t *ent, int client);
void votecmdRun(int startarg, edict_t *ent, int client);
void voteDelRun(int startarg, edict_t *ent, int client);
