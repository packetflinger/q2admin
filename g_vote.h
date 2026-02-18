/**
 * Q2Admin
 * Voting stuff
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

bool checkforvotecmd(char *cp, int votecmd);
void checkOnVoting(void);
bool checkVoteCommand(char *votecmd);
void displayNextVote(edict_t *ent, int client, long floodcmd);
void displayVote(void);
void freeVoteLists(void);
void listvotesRun(int startarg, edict_t *ent, int client);
bool ReadVoteFile(char *votename);
void readVoteLists(void);
void reloadVoteFileRun(int startarg, edict_t *ent, int client);
void run_vote(edict_t *ent, int client);
void votecmdRun(int startarg, edict_t *ent, int client);
void voteDelRun(int startarg, edict_t *ent, int client);
