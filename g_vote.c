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

votecmd_t votecmds[VOTE_MAXCMDS];
int maxvote_cmds = 0;
bool votecountnovotes = 1;
int votepasspercent = 50;
int voteminclients = 0;
static bool voteinprogress = 0;
static unsigned long votetimeout, voteremindtimeout;
char cmdvote[2048];
char cmdpassedvote[2048];
char votecaller[16];
int clientVoteTimeout = 60;
int clientRemindTimeout = 10;
int clientMaxVoteTimeout = 0;
int clientMaxVotes = 0;

/**
 * Loads one vote allow-list file into votecmds[]. Players can't propose
 * arbitrary commands - whatever a vote passes with is eventually handed
 * to gi.AddCommandString() as a real server console command, so the set
 * of proposable commands has to be an explicit allow-list rather than
 * anything a player types. This is what builds that list.
 *
 * Each line is a match rule prefixed by its type, blank lines and ';'
 * comments ignored, anything else logged as a bad line:
 *   SW: - the proposed command must start with this
 *   EX: - it must equal this exactly
 *   RE: - it must match this regular expression
 * RE patterns are upper-cased before compiling, which pairs with
 * checkVoteCommand() upper-casing the candidate, making RE rules
 * case insensitive.
 *
 * Stops once votecmds[] holds VOTE_MAXCMDS rules, whether that limit is
 * hit part way through this file or was already reached before it was
 * opened.
 *
 * votename: path of the file to read.
 *
 * Returns true if the file was opened and read, false if it couldn't be
 * opened or there was no room left for more rules.
 *
 * Called from readVoteLists() below, once for each of the two locations
 * a vote file can live in.
 */
bool readVoteFile(char *votename) {
    FILE *votefile;
    unsigned int uptoLine = 0;

    if (maxvote_cmds >= VOTE_MAXCMDS) {
        return false;
    }

    votefile = fopen(votename, "rt");
    if (!votefile) {
        return false;
    }

    while (fgets(buffer, 256, votefile)) {
        char *cp = buffer;
        int len;

        // remove '\n'
        len = q2a_strlen(buffer) - 1;
        if (buffer[len] == '\n') {
            buffer[len] = 0x0;
        }
        SKIPBLANK(cp);
        uptoLine++;
        if (startContains(cp, "SW:") || startContains(cp, "EX:") || startContains(cp, "RE:")) {
            // looks ok, add...
            switch (*cp) {
                case 'S':
                    votecmds[maxvote_cmds].type = VOTE_SW;
                    break;
                case 'E':
                    votecmds[maxvote_cmds].type = VOTE_EX;
                    break;
                case 'R':
                    votecmds[maxvote_cmds].type = VOTE_RE;
                    break;
            }
            cp += 3;
            SKIPBLANK(cp);
            len = q2a_strlen(cp) + 1;

            // zero length command
            if (!len) {
                gi.dprintf("Error loading VOTE from line %d in file %s\n", uptoLine, votename);
                continue;
            }
            votecmds[maxvote_cmds].votecmd = G_Malloc(len);
            q2a_strcpy(votecmds[maxvote_cmds].votecmd, cp);
            if (votecmds[maxvote_cmds].type == VOTE_RE) {
                upperCase(cp);
                votecmds[maxvote_cmds].r = re_compile(cp);
                if (!votecmds[maxvote_cmds].r) {
                    // malformed re... skip this vote command
                    gi.dprintf("Error loading VOTE from line %d in file %s\n", uptoLine, votename);
                    continue;
                }
            } else {
                votecmds[maxvote_cmds].r = 0;
            }
            maxvote_cmds++;
            if (maxvote_cmds >= VOTE_MAXCMDS) {
                break;
            }
        } else if (!(cp[0] == ';' || cp[0] == '\n' || isBlank(cp))) {
            gi.dprintf("Error loading VOTE from line %d in file %s\n", uptoLine, votename);
        }
    }
    fclose(votefile);
    return true;
}

/**
 * Releases the command strings readVoteFile()/votecmdRun() allocated for
 * each rule and empties votecmds[], so the list can be rebuilt from
 * scratch without leaking the previous load's strings.
 *
 * Only the votecmd string is freed; the compiled regex needs no freeing
 * because re_compile() hands back a pointer into its own static storage
 * rather than an allocation (see checkForVoteCmd() for what that costs).
 *
 * Takes no parameters; operates on the global votecmds[]/maxvote_cmds.
 *
 * Called from readVoteLists() below before each reload, and from
 * SpawnEntities() (g_init.c) as part of tearing down the previous
 * level's config lists.
 */
void freeVoteLists(void) {
    while (maxvote_cmds) {
        maxvote_cmds--;
        G_Free(votecmds[maxvote_cmds].votecmd);
    }
}

/**
 * Rebuilds the vote allow-list from disk: drops whatever was loaded
 * before, then reads the file named by the q2a_votefile cvar from two
 * places - the server's working directory and moddir - merging both into
 * one list. That lets a server-wide default live in one spot and a
 * per-mod list add to it, rather than one having to replace the other.
 *
 * Only warns (LT_INTERNALWARN) if *neither* location had a readable
 * file, since having just one of the two is the normal case.
 *
 * Takes no parameters; fills the global votecmds[].
 *
 * Called from SpawnEntities() (g_init.c) on every level load, so the
 * list picks up edits at each map change, and from reloadVoteFileRun()
 * below.
 */
void readVoteLists(void) {
    bool ret;

    freeVoteLists();
    ret = readVoteFile(configfile_vote->string);
    Q_snprintf(buffer, sizeof(buffer), "%s/%s", moddir, configfile_vote->string);
    if (readVoteFile(buffer)) {
        ret = true;
    }
    if (!ret) {
        logEvent(LT_INTERNALWARN, 0, NULL, va("%s could not be found", configfile_vote->string), IW_VOTESETUPLOAD, 0.0, true);
    }
}

/**
 * "!reloadvotefile" - re-reads the vote allow-list from disk, for
 * picking up edits without waiting for the next map change (which is
 * otherwise the only time readVoteLists() runs).
 *
 * Note the confirmation text below says "Disbled entities reloaded",
 * which is wrong on both counts - it's copy-paste from the disable-list
 * equivalent, and misspelled.
 *
 * startarg: unused, this command takes no arguments.
 * ent:      who to confirm to.
 * client:   unused here.
 *
 * Called via the "reloadvotefile" entry in q2aCommands[] (g_cmd.c), from
 * an in-game admin console or rcon.
 */
void reloadVoteFileRun(int startarg, edict_t *ent, int client) {
    readVoteLists();
    gi.cprintf(ent, PRINT_HIGH, "Disbled entities reloaded.\n");
}

/**
 * Tests a proposed command against one allow-list rule, applying
 * whichever of the three match types that rule was loaded as: SW
 * (prefix), EX (whole string, case insensitive) or RE (regex).
 *
 * Two things about the RE case are worth knowing. re_matchp() returns
 * the offset the match was found at, so comparing it to 0 means an RE
 * rule only passes when the pattern matches at the *start* of the
 * proposed command - it's effectively anchored, not a search. And
 * re_compile() builds into a single static buffer that it returns a
 * pointer into, so every stored re_t in the process aliases the same
 * storage: with more than one RE rule loaded, they all end up matching
 * against whichever pattern was compiled last.
 *
 * cp:      the proposed command, already upper-cased by
 *          checkVoteCommand() - that's what makes EX/RE case
 *          insensitive, since the stored patterns are upper-cased too.
 * votecmd: index into votecmds[] of the rule to test.
 *
 * Returns true if this one rule matches.
 *
 * Called from checkVoteCommand() below, once per loaded rule.
 */
bool checkForVoteCmd(char *cp, int votecmd) {
    int len;
    switch (votecmds[votecmd].type) {
        case VOTE_SW:
            return startContains(cp, votecmds[votecmd].votecmd);
        case VOTE_EX:
            return !Q_stricmp(cp, votecmds[votecmd].votecmd);
        case VOTE_RE:
            return re_matchp(votecmds[votecmd].r, cp, &len) == 0;
    }
    return false;
}

/**
 * Decides whether a proposed command is something players are allowed to
 * vote on, by testing it against every loaded rule until one matches.
 * This is the gate that keeps a vote from being a route to running
 * arbitrary server console commands - runVote() only starts a vote if
 * this says yes.
 *
 * Upper-cases the candidate into the shared `buffer` first, which is
 * what makes matching case insensitive against the equally upper-cased
 * stored patterns. Note that means this clobbers `buffer`, so callers
 * can't be relying on it across this call.
 *
 * cp: the command a player proposed.
 *
 * Returns true if any rule matches, false if nothing does (or if no
 * rules are loaded at all, which effectively disables proposing).
 *
 * Called from runVote() below, on the propose path.
 */
bool checkVoteCommand(char *cp) {
    unsigned int i;

    q2a_strncpy(buffer, cp, sizeof(buffer)-1);
    upperCase(buffer);
    for (i = 0; i < maxvote_cmds; i++) {
        if (checkForVoteCmd(buffer, i)) {
            return true;
        }
    }
    return false;
}

/**
 * "!listvotes" - shows the loaded vote allow-list, so an admin can see
 * what players are actually able to propose.
 *
 * Prints the header itself and then queues QCMD_DISPVOTE, which walks
 * the rules one per frame via displayNextVote() rather than dumping
 * potentially VOTE_MAXCMDS lines into the client in a single burst.
 *
 * startarg: unused, this command takes no arguments.
 * ent:      who to print the header to.
 * client:   whose command queue the listing is paced through.
 *
 * Called via the "listvotes" entry in q2aCommands[] (g_cmd.c).
 */
void listvotesRun(int startarg, edict_t *ent, int client) {
    addCmdQueue(client, QCMD_DISPVOTE, 0, 0, 0);
    gi.cprintf(ent, PRINT_HIGH, "Start Vote Command List:\n");
}

/**
 * Prints one allow-list rule - its 1-based number, its type prefix and
 * its pattern, in the same "SW:/EX:/RE:" form the vote file uses, so
 * what's shown can be matched up against (or pasted into) that file.
 *
 * Re-queues itself for the next index until the list runs out, then
 * prints the closing line. That pacing is the whole reason this is split
 * out of listvotesRun(): one rule per queued-command tick keeps a long
 * list from flooding the client at once, the same approach
 * displayNextBan() and friends use.
 *
 * ent:     who to print to.
 * client:  their client index, used to queue the next tick.
 * votecmd: 0-based index into votecmds[] of the rule to print; past the
 *          end means stop and print the footer.
 *
 * Called from G_RunFrame()'s QCMD_DISPVOTE handling (g_main.c), first
 * queued by listvotesRun() and then kept going by this function.
 */
void displayNextVote(edict_t *ent, int client, long votecmd) {
    if (votecmd < maxvote_cmds) {
        switch (votecmds[votecmd].type) {
            case VOTE_SW:
                gi.cprintf(ent, PRINT_HIGH, "%4ld SW:\"%s\"\n", votecmd + 1, votecmds[votecmd].votecmd);
                break;
            case VOTE_EX:
                gi.cprintf(ent, PRINT_HIGH, "%4ld EX:\"%s\"\n", votecmd + 1, votecmds[votecmd].votecmd);
                break;
            case VOTE_RE:
                gi.cprintf(ent, PRINT_HIGH, "%4ld RE:\"%s\"\n", votecmd + 1, votecmds[votecmd].votecmd);
                break;
        }
        votecmd++;
        addCmdQueue(client, QCMD_DISPVOTE, 0, votecmd, 0);
    } else {
        gi.cprintf(ent, PRINT_HIGH, "End Vote Command List\n");
    }
}

/**
 * "!votecmd [SW/EX/RE] "command"" - appends one rule to the vote
 * allow-list at runtime, for trying a rule out or reacting to something
 * mid-session without editing the vote file and reloading.
 *
 * The addition is memory-only: the next readVoteLists() (any map change,
 * or !reloadvotefile) rebuilds from disk and drops it, so anything meant
 * to stick has to go in the file.
 *
 * Rejects an unrecognised type keyword, a blank pattern, a list that's
 * already at VOTE_MAXCMDS, and a regex that won't compile - in the regex
 * case freeing the command string it had already allocated before
 * bailing. As with the file loader, the pattern is upper-cased before
 * compiling so matching stays case insensitive.
 *
 * startarg: index of the type keyword in the command's arguments; the
 *           pattern is expected at startarg + 1.
 * ent:      who to report success or the usage line to.
 * client:   unused here.
 *
 * Called via the "votecmd" entry in q2aCommands[] (g_cmd.c).
 */
void votecmdRun(int startarg, edict_t *ent, int client) {
    char *cmd;
    int len;

    if (maxvote_cmds >= VOTE_MAXCMDS) {
        gi.cprintf(ent, PRINT_HIGH, "Sorry, maximum number of vote commands has been reached.\n");
        return;
    }

    if (gi.argc() <= startarg + 1) {
        gi.cprintf(ent, PRINT_HIGH, VOTECMD);
        return;
    }

    cmd = gi.argv(startarg);

    if (Q_stricmp(cmd, "SW") == 0) {
        votecmds[maxvote_cmds].type = VOTE_SW;
    } else if (Q_stricmp(cmd, "EX") == 0) {
        votecmds[maxvote_cmds].type = VOTE_EX;
    } else if (Q_stricmp(cmd, "RE") == 0) {
        votecmds[maxvote_cmds].type = VOTE_RE;
    } else {
        gi.cprintf(ent, PRINT_HIGH, VOTECMD);
        return;
    }

    cmd = gi.argv(startarg + 1);
    if (isBlank(cmd)) {
        gi.cprintf(ent, PRINT_HIGH, VOTECMD);
        return;
    }

    len = q2a_strlen(cmd) + 20;
    votecmds[maxvote_cmds].votecmd = G_Malloc(len);
    processString(votecmds[maxvote_cmds].votecmd, cmd, len - 1, 0);

    if (votecmds[maxvote_cmds].type == VOTE_RE) {
        upperCase(cmd);
        votecmds[maxvote_cmds].r = re_compile(cmd);
        if (!votecmds[maxvote_cmds].r) {
            G_Free(votecmds[maxvote_cmds].votecmd);

            // malformed re...
            gi.cprintf(ent, PRINT_HIGH, "Regular expression couldn't compile!\n");
            return;
        }
    } else {
        votecmds[maxvote_cmds].r = 0;
    }

    switch (votecmds[maxvote_cmds].type) {
        case VOTE_SW:
            gi.cprintf(ent, PRINT_HIGH, "%4d SW:\"%s\" added\n", maxvote_cmds + 1, votecmds[maxvote_cmds].votecmd);
            break;
        case VOTE_EX:
            gi.cprintf(ent, PRINT_HIGH, "%4d EX:\"%s\" added\n", maxvote_cmds + 1, votecmds[maxvote_cmds].votecmd);
            break;
        case VOTE_RE:
            gi.cprintf(ent, PRINT_HIGH, "%4d RE:\"%s\" added\n", maxvote_cmds + 1, votecmds[maxvote_cmds].votecmd);
            break;
    }
    maxvote_cmds++;
}

/**
 * "!votedel <votenum>" - removes one rule from the vote allow-list by
 * the number !listvotes displays, the counterpart to !votecmd for
 * withdrawing something at runtime.
 *
 * Frees that rule's command string and closes the gap by shifting the
 * remaining rules down, which keeps votecmds[] dense (everything else
 * here iterates 0..maxvote_cmds) at the cost of renumbering every rule
 * after the deleted one - so numbers from an earlier !listvotes are
 * stale afterwards.
 *
 * Like !votecmd this only edits the in-memory list; the next
 * readVoteLists() restores whatever the file says.
 *
 * startarg: index of the rule number in the command's arguments.
 * ent:      who to report success or the usage line to.
 * client:   unused here.
 *
 * Called via the "votedel" entry in q2aCommands[] (g_cmd.c).
 */
void voteDelRun(int startarg, edict_t *ent, int client) {
    int vote;

    if (gi.argc() <= startarg) {
        gi.cprintf(ent, PRINT_HIGH, VOTEDELCMD);
        return;
    }
    vote = q2a_atoi(gi.argv(startarg));
    if (vote < 1 || vote > maxvote_cmds) {
        gi.cprintf(ent, PRINT_HIGH, VOTEDELCMD);
        return;
    }
    vote--;
    G_Free(votecmds[vote].votecmd);
    if (vote + 1 < maxvote_cmds) {
        q2a_memmove((votecmds + vote), (votecmds + vote + 1), sizeof (votecmd_t) * (maxvote_cmds - vote));
    }
    maxvote_cmds--;
    gi.cprintf(ent, PRINT_HIGH, "Vote command deleted\n");
}

/**
 * Center-prints the running tally of the vote in progress to everyone,
 * tailored per player: anyone who hasn't voted yet also gets told how to
 * (the console syntax), while those who have just see the numbers.
 *
 * This is how a vote stays visible to people who weren't looking at the
 * exact frame it was proposed - it's shown when the vote opens and again
 * on each reminder tick.
 *
 * Takes no parameters; reads the current vote's globals (cmdvote,
 * votecaller) and each client's CCMD_VOTED/CCMD_VOTEYES flags.
 *
 * Called from runVote() below when a vote is first proposed, and from
 * checkOnVoting() every clientRemindTimeout seconds while it runs.
 * runVote() also has a near-copy of this inline for the "what's the
 * status" case, differing in that it prints only to the asking player
 * and omits who proposed the vote.
 */
void displayVote(void) {
    int client;
    unsigned int maxclientsused = 0, voteyes = 0, voteno = 0, novote = 0;

    // count votes
    for (client = 0; client < maxclients->value; client++) {
        if (proxyinfo[client].inuse) {
            maxclientsused++;
            if (proxyinfo[client].clientcommand & CCMD_VOTED) {
                if (proxyinfo[client].clientcommand & CCMD_VOTEYES) {
                    voteyes++;
                } else {
                    voteno++;
                }
            } else {
                novote++;
            }
        }
    }

    for (client = 0; client < maxclients->value; client++) {
        if (proxyinfo[client].inuse) {
            if (proxyinfo[client].clientcommand & CCMD_VOTED) {
                // just display the stat's
                gi.centerprintf(getEnt((client + 1)), "Vote Summary So Far:\n"
                        "Proposed Vote: %s by %s\n"
                        "Voted Yes: %d    Voted No: %d\n"
                        "Haven't Voted Yet: %d\n", cmdvote, votecaller, voteyes, voteno, novote);
            } else {
                // format the remember and the stat's
                gi.centerprintf(getEnt((client + 1)), "You haven't voted yet. To vote at the \n"
                        "console type '%s yes' or '%s no'\n"
                        "\n"
                        "Vote Summary So Far:\n"
                        "Proposed Vote: %s by %s\n"
                        "Voted Yes: %d    Voted No: %d\n"
                        "Haven't Voted Yet: %d\n", clientVoteCommand, clientVoteCommand,
                        cmdvote, votecaller, voteyes, voteno, novote);
            }
        }
    }
}

/**
 * The player-facing vote command (whatever clientvotecommand is set to,
 * "vote" by default) - one entry point doing three jobs depending on
 * what follows it:
 *   no argument  - show the current tally, or how to propose one if no
 *                  vote is running,
 *   yes / no     - cast a vote, if one is running,
 *   anything else- propose that as a new vote.
 *
 * The propose path is the guarded one, since a passing vote ends up at
 * gi.AddCommandString() as a real console command. Two things stand
 * between a player and that: any proposal containing ';' is rejected
 * outright, because it would otherwise let a player chain extra commands
 * onto an allowed one, and what's left still has to match the
 * checkVoteCommand() allow-list.
 *
 * Past that it enforces the anti-nuisance limits: voteminclients (enough
 * people present to be worth voting), a fixed 45 second settling period
 * after a map change, and the voteclientmaxvotes/voteclientmaxvotetimeout
 * budget - with votescast == -1 as the sentinel for "this player is done
 * proposing until the next level", which is how a zero timeout is made
 * to mean per-level rather than per-window.
 *
 * A successful proposal opens the vote, records the command and who
 * called it, counts the proposer as a yes, and shows everyone the tally.
 *
 * ent:    the player running the command, and who replies go to.
 * client: their client index, for their vote flags and vote budget.
 *
 * Called from doClientCommand() (g_cmd.c) when a player types the
 * configured vote command, gated on vote_enable.
 */
void runVote(edict_t *ent, int client) {
    char *votecmd;

    if (gi.argc() <= 1) {
        // menu driven interface...
        if (voteinprogress) {
            int clienti;
            int maxclientsused = 0, voteyes = 0, voteno = 0, novote = 0;

            // count votes
            for (clienti = 0; clienti < maxclients->value; clienti++) {
                if (proxyinfo[clienti].inuse) {
                    maxclientsused++;

                    if (proxyinfo[clienti].clientcommand & CCMD_VOTED) {
                        if (proxyinfo[clienti].clientcommand & CCMD_VOTEYES) {
                            voteyes++;
                        } else {
                            voteno++;
                        }
                    } else {
                        novote++;
                    }
                }
            }

            if (proxyinfo[client].clientcommand & CCMD_VOTED) {
                // just display the stat's
                gi.centerprintf(ent, "Vote Summary So Far:\n"
                        "Proposed Vote: %s\n"
                        "Voted Yes: %d    Voted No: %d\n"
                        "Haven't Voted Yet: %d\n", cmdvote, voteyes, voteno, novote);
            } else {
                // format the remember and the stat's
                gi.centerprintf(ent, "You haven't voted yet. To vote at the \n"
                        "console type '%s yes' or '%s no'\n"
                        "\n"
                        "Vote Summary So Far:\n"
                        "Proposed Vote: %s\n"
                        "Voted Yes: %d    Voted No: %d\n"
                        "Haven't Voted Yet: %d\n", clientVoteCommand, clientVoteCommand,
                        cmdvote, voteyes, voteno, novote);
            }
        } else {
            gi.centerprintf(ent, "To propose a vote type in \n%s <cmd>\n at the console.\n", clientVoteCommand);
        }
        return;
    }
    votecmd = gi.args();
    if (q2a_strchr(votecmd, ';')) {
        gi.cprintf(ent, PRINT_HIGH, "Invalid vote command!\n");
        return;
    }
    SKIPBLANK(votecmd);
    if (startContains(votecmd, "YES")) {
        if (voteinprogress) {
            proxyinfo[client].clientcommand |= (CCMD_VOTEYES | CCMD_VOTED);
            gi.cprintf(ent, PRINT_HIGH, "You have voted: YES\n");
        } else {
            gi.cprintf(ent, PRINT_HIGH, "There is no vote in progress!\n");
        }
        return;
    } else if (startContains(votecmd, "NO")) {
        if (voteinprogress) {
            proxyinfo[client].clientcommand |= CCMD_VOTED;
            proxyinfo[client].clientcommand &= ~CCMD_VOTEYES;
            gi.cprintf(ent, PRINT_HIGH, "You have voted: NO\n");
        } else {
            gi.cprintf(ent, PRINT_HIGH, "There is no vote in progress!\n");
        }
        return;
    }
    if (voteinprogress) {
        gi.cprintf(ent, PRINT_HIGH, "There is already a vote in progress!\n");
        return;
    }
    if (checkVoteCommand(votecmd)) {
        if (voteminclients) {
            int client;
            int maxclientsingame = 0;

            // count number of clients
            for (client = 0; client < maxclients->value; client++) {
                if (proxyinfo[client].inuse) {
                    maxclientsingame++;
                }
            }
            if (voteminclients > maxclientsingame) {
                gi.cprintf(ent, PRINT_HIGH, "Not enough people to vote.\n");
                return;
            }
        }

        // check if allowed to vote at this time...
        if (clientMaxVotes) {
            if (ltime <= 45) {
                gi.cprintf(ent, PRINT_HIGH, "Recent map change - too soon to vote (please wait).\n");
                return;
            }
            if (proxyinfo[client].votescast == -1) {
                // not allowed to vote again..
                gi.cprintf(ent, PRINT_HIGH, "You can't propose any more votes until the next level.\n");
                return;
            }
            // started counting votes?
            if (proxyinfo[client].votetimeout > ltime || clientMaxVoteTimeout == 0) {
                // exceeded maximum votes allowed?
                if (proxyinfo[client].votescast >= clientMaxVotes) {
                    int secleft = (int) (proxyinfo[client].votetimeout - ltime) + 1;

                    gi.cprintf(ent, PRINT_HIGH, "You can't propose any more votes for %d seconds.\n", secleft);
                    return;
                } else {
                    proxyinfo[client].votescast++;

                    // if they proposed the last vote for the level?
                    if (clientMaxVoteTimeout == 0 && proxyinfo[client].votescast >= clientMaxVotes) {
                        proxyinfo[client].votescast = -1;
                    }
                }
            } else {
                // first vote for the timeout period.
                proxyinfo[client].votescast = 1;
                proxyinfo[client].votetimeout = ltime + clientMaxVoteTimeout;
            }
        }
        voteinprogress = 1;
        votetimeout = ltime + clientVoteTimeout;
        voteremindtimeout = ltime + clientRemindTimeout;
        proxyinfo[client].clientcommand |= (CCMD_VOTEYES | CCMD_VOTED);
        q2a_strncpy(cmdvote, votecmd, sizeof(cmdvote)-1);
        q2a_strcat(cmdvote, "\n");
        q2a_strncpy(votecaller, proxyinfo[client].name, sizeof(votecaller)-1);
        q2a_strcat(votecaller, "\n");
        displayVote();
    } else {
        gi.cprintf(ent, PRINT_HIGH, "Invalid vote command specified.\n");
    }
}

/**
 * Drives a vote in progress to its conclusion - the piece that makes
 * voting time-based rather than something a player has to close out.
 * Does nothing unless a vote is open.
 *
 * Ends the vote when it times out (clientVoteTimeout) or as soon as
 * every connected player has voted, whichever comes first, so a decided
 * vote doesn't sit there running down its clock. Otherwise it re-shows
 * the tally every clientRemindTimeout seconds via displayVote().
 *
 * On close it tallies, clears everyone's vote flags ready for the next
 * one, and compares the yes share against votepasspercent. What counts
 * as the denominator is the votecountnovotes setting: when set,
 * abstainers are counted against the vote (the share is over everyone
 * present), when clear they're dropped from the denominator entirely so
 * only actual voters decide it.
 *
 * A passing command isn't run inline - it's copied to cmdpassedvote and
 * queued as QCMD_RUNVOTECMD on a 5 second delay, which is what gives
 * players time to read the result before a map change or whatever else
 * the command does takes effect.
 *
 * Takes no parameters; works on the module's vote state globals.
 *
 * Called from G_RunFrame() (g_main.c) every server frame.
 */
void checkOnVoting(void) {
    int client;
    unsigned int maxclientsused = 0, voteyes = 0, voteno = 0, novote = 0;
    double percent;
    char printstr[100];

    if (voteinprogress) {
        // count votes and run vote command if successful
        for (client = 0; client < maxclients->value; client++) {
            if (proxyinfo[client].inuse) {
                if (!(proxyinfo[client].clientcommand & CCMD_VOTED)) {
                    break;
                }
            }
        }
        if (votetimeout < ltime || client >= maxclients->value) {
            voteinprogress = 0;
            // count votes and run vote command if successful
            for (client = 0; client < maxclients->value; client++) {
                if (proxyinfo[client].inuse) {
                    maxclientsused++;
                    if (proxyinfo[client].clientcommand & CCMD_VOTED) {
                        if (proxyinfo[client].clientcommand & CCMD_VOTEYES) {
                            voteyes++;
                        } else {
                            voteno++;
                        }
                    } else {
                        novote++;
                    }
                }
                proxyinfo[client].clientcommand &= ~(CCMD_VOTEYES | CCMD_VOTED);
            }
            percent = ((double) voteyes / ((double) maxclientsused - ((double) votecountnovotes ? 0.0 : novote)));
            if (percent >= ((double) votepasspercent / 100)) {
                q2a_strcpy(printstr, "Vote PASSED!");
                q2a_strncpy(cmdpassedvote, cmdvote, sizeof(cmdpassedvote)-1);
                addCmdQueue(-1, QCMD_RUNVOTECMD, 5, 0, 0);
            } else {
                q2a_strcpy(printstr, "Vote FAILED!");
            }
            for (client = 0; client < maxclients->value; client++) {
                if (proxyinfo[client].inuse) {
                    gi.centerprintf(getEnt((client + 1)), "%s\n"
                            "\n"
                            "Vote Summary:\n"
                            "Proposed Vote: %s\n"
                            "Voted Yes: %d    Voted No: %d\n"
                            "Didn't Vote: %d\n", printstr, cmdvote, voteyes, voteno, novote);
                }
            }
        } else if (voteremindtimeout < ltime) {
            voteremindtimeout = ltime + clientRemindTimeout;
            displayVote();
        }
    }
}
