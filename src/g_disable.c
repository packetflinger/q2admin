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

disablecmd_t disablecmds[DISABLE_MAXCMDS];
int maxdisable_cmds = 0;
bool disablecmds_enable = false;

/**
 * Loads one disabled-command file into disablecmds[]. The list names
 * client commands players aren't allowed to run at all - a blunt way to
 * shut off a mod command that's exploitable, broken, or simply unwanted
 * on this server, without needing the mod itself changed.
 *
 * Each line is a match rule prefixed by its type, blank lines and ';'
 * comments ignored, anything else logged as a bad line:
 *   SW: - the command line must start with this
 *   EX: - it must equal this exactly
 *   RE: - it must match this regular expression
 * RE patterns are upper-cased before compiling, pairing with
 * checkDisabledCommand() upper-casing what it tests, so RE rules end up
 * case insensitive.
 *
 * disablename: path of the file to read.
 *
 * Returns true if the file was opened and read, false if it couldn't be
 * opened or the list was already full at DISABLE_MAXCMDS.
 *
 * Called from readDisableLists() below, once for each of the two
 * locations a disable file can live in.
 */
bool readDisableFile(char *disablename) {
    FILE *disablefile;
    unsigned int uptoLine = 0;

    if (maxdisable_cmds >= DISABLE_MAXCMDS) {
        return false;
    }

    disablefile = fopen(disablename, "rt");
    if (!disablefile) {
        return false;
    }

    while (fgets(buffer, 256, disablefile)) {
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
                    disablecmds[maxdisable_cmds].type = DISABLE_SW;
                    break;
                case 'E':
                    disablecmds[maxdisable_cmds].type = DISABLE_EX;
                    break;
                case 'R':
                    disablecmds[maxdisable_cmds].type = DISABLE_RE;
                    break;
            }

            cp += 3;
            SKIPBLANK(cp);

            len = q2a_strlen(cp) + 1;

            // zero length command
            if (!len) {
                gi.dprintf("Error loading DISABLE from line %d in file %s\n", uptoLine, disablename);
                continue;
            }

            disablecmds[maxdisable_cmds].disablecmd = G_Malloc(len);
            Q_snprintf(disablecmds[maxdisable_cmds].disablecmd, len, "%s", cp);

            if (disablecmds[maxdisable_cmds].type == DISABLE_RE) {
                upperCase(cp);
                disablecmds[maxdisable_cmds].r = re_compile(cp);
                if (!disablecmds[maxdisable_cmds].r) {
                    // malformed re... skip this disable command
                    gi.dprintf("Error loading DISABLE from line %d in file %s\n", uptoLine, disablename);
                    continue;
                }
            } else {
                disablecmds[maxdisable_cmds].r = 0;
            }

            maxdisable_cmds++;
            if (maxdisable_cmds >= DISABLE_MAXCMDS) {
                break;
            }
        } else if (!(cp[0] == ';' || cp[0] == '\n' || isBlank(cp))) {
            gi.dprintf("Error loading DISABLE from line %d in file %s\n", uptoLine, disablename);
        }
    }
    fclose(disablefile);
    return true;
}

/**
 * Releases the command strings readDisableFile()/disablecmdRun()
 * allocated and empties disablecmds[], so the list can be rebuilt
 * without leaking the previous load's strings. Only the string is freed;
 * the compiled regex needs no freeing because re_compile() returns a
 * pointer into its own static storage rather than an allocation.
 *
 * Takes no parameters; operates on the global
 * disablecmds[]/maxdisable_cmds. Returns nothing.
 *
 * Called from readDisableLists() below before each reload, and from
 * SpawnEntities() (g_init.c) while tearing down the previous level's
 * config lists.
 */
void freeDisableLists(void) {
    while (maxdisable_cmds) {
        maxdisable_cmds--;
        G_Free(disablecmds[maxdisable_cmds].disablecmd);
    }
}

/**
 * Rebuilds the disabled-command list from disk: drops whatever was
 * loaded before, then reads the file named by the q2a_disablefile cvar
 * from two places - the server's working directory and moddir - merging
 * both into one list, so a server-wide default and a per-mod list can
 * coexist rather than one replacing the other. The per-mod half matters
 * here because which commands exist at all depends on the mod.
 *
 * Only warns (LT_INTERNALWARN) if *neither* location had a readable
 * file, since having just one of the two is normal.
 *
 * Takes no parameters; fills the global disablecmds[]. Returns nothing.
 *
 * Called from SpawnEntities() (g_init.c) on every level load, so the
 * list picks up edits at each map change, and from
 * reloadDisableFileRun() below.
 */
void readDisableLists(void) {
    bool ret;

    freeDisableLists();
    ret = readDisableFile(configfile_disable->string);
    Q_snprintf(buffer, sizeof(buffer), "%s/%s", moddir, configfile_disable->string);
    if (readDisableFile(buffer)) {
        ret = true;
    }
    if (!ret) {
        logEvent(LT_INTERNALWARN, 0, NULL, va("%s could not be found", configfile_disable->string), IW_DISABLESETUPLOAD, 0.0, true);
    }
}

/**
 * "!reloaddisablefile" - re-reads the disabled-command list from disk,
 * for picking up edits without waiting for the next map change (which is
 * otherwise the only time readDisableLists() runs). Unlike the entity
 * disabler, changes here take effect immediately, since commands are
 * checked as they're issued rather than at map load.
 *
 * startarg: unused, this command takes no arguments.
 * ent:      who to confirm to.
 * client:   unused here.
 *
 * Returns nothing.
 *
 * Called via the "reloaddisablefile" entry in q2aCommands[] (g_cmd.c),
 * from an in-game admin console or rcon.
 */
void reloadDisableFileRun(int startarg, edict_t *ent, int client) {
    readDisableLists();
    gi.cprintf(ent, PRINT_HIGH, "Disabled commands reloaded.\n");
}

/**
 * Tests a command line against one disable rule, applying whichever of
 * the three match types that rule was loaded as: SW (prefix), EX (whole
 * string, case insensitive) or RE (regex).
 *
 * SW is the useful one here, since what's being matched is the command
 * plus its arguments: a prefix rule can block a command outright
 * ("give") or only a particular use of it ("give all"), depending how
 * much of the line it covers.
 *
 * Note re_matchp() returns the offset the match was found at, so the
 * comparison against 0 means an RE rule only fires when the pattern
 * matches at the *start* of the line - effectively anchored rather than
 * a search. Also worth knowing that re_compile() builds into a single
 * static buffer it hands a pointer into, so with more than one RE rule
 * loaded they all end up matching against whichever pattern compiled
 * last.
 *
 * cp:         the command line to test, already upper-cased by
 *             checkDisabledCommand() - which is what makes EX/RE case
 *             insensitive, since the stored patterns are upper-cased
 *             too.
 * disablecmd: index into disablecmds[] of the rule to test.
 *
 * Returns true if this one rule matches.
 *
 * Called from checkDisabledCommand() below, once per loaded rule.
 */
bool checkForDisableCmd(char *cp, int disablecmd) {
    int len;
    switch (disablecmds[disablecmd].type) {
        case DISABLE_SW:
            return startContains(cp, disablecmds[disablecmd].disablecmd);
        case DISABLE_EX:
            return !Q_stricmp(cp, disablecmds[disablecmd].disablecmd);
        case DISABLE_RE:
            return (re_matchp(disablecmds[disablecmd].r, cp, &len) == 0);
    }
    return false;
}

/**
 * Decides whether a client command should be blocked, by testing it
 * against every loaded rule until one matches. This is the gate that
 * makes the whole feature work - a match means the command is refused
 * before the wrapped game mod ever sees it, which is what lets a server
 * shut off a mod command it can't otherwise change.
 *
 * Worth being clear about what gets matched: the caller passes the
 * client's whole command line, command name and arguments joined by a
 * space, not just the command name. That's what allows rules targeting a
 * specific use of a command rather than the command as a whole.
 *
 * Upper-cases the candidate into the shared `buffer` first, which is
 * what makes matching case insensitive against the equally upper-cased
 * stored patterns. That means this clobbers `buffer`, so callers can't
 * be holding anything in it across the call.
 *
 * cmd: the full command line the client issued.
 *
 * Returns true if any rule matches and the command should be refused,
 * false if none do or no rules are loaded.
 *
 * Called from doClientCommand() (g_cmd.c), gated on disablecmds_enable.
 * A match there logs the attempt (LT_DISABLECMD), announces it to the
 * console with the player's name, and drops the command.
 */
bool checkDisabledCommand(char *cmd) {
    q2a_strncpy(buffer, cmd, sizeof(buffer)-1);
    upperCase(buffer);
    for (unsigned int i = 0; i < maxdisable_cmds; i++) {
        if (checkForDisableCmd(buffer, i)) {
            return true;
        }
    }
    return false;
}

/**
 * "!listdisables" - shows the loaded disabled-command rules, so an admin
 * can see which commands players are currently blocked from running.
 *
 * Prints the header itself and then queues QCMD_DISPDISABLE, which walks
 * the rules one per frame via displayNextDisable() rather than dumping
 * the whole list into the client at once.
 *
 * startarg: unused, this command takes no arguments.
 * ent:      who to print the header to.
 * client:   whose command queue the listing is paced through.
 *
 * Returns nothing.
 *
 * Called via the "listdisables" entry in q2aCommands[] (g_cmd.c).
 *
 * Note the header text says "disbled-entities", copy-pasted from the
 * entity disabler in g_spawn.c - this command lists disabled *commands*.
 */
void listdisablesRun(int startarg, edict_t *ent, int client) {
    addCmdQueue(client, QCMD_DISPDISABLE, 0, 0, 0);
    gi.cprintf(ent, PRINT_HIGH, "Start disbled-entities List:\n");
}

/**
 * Prints one disable rule - its 1-based number, type prefix and pattern,
 * in the same "SW:/EX:/RE:" form the disable file uses, so what's shown
 * can be matched against (or pasted into) that file.
 *
 * Re-queues itself for the next index until the list runs out, then
 * prints the closing line. That pacing is why this is split out of
 * listdisablesRun(): one rule per queued-command tick keeps a long list
 * from flooding the client, the same approach displayNextBan() and
 * friends use.
 *
 * ent:        who to print to.
 * client:     their client index, used to queue the next tick.
 * disablecmd: 0-based index into disablecmds[] of the rule to print;
 *             past the end means stop and print the footer.
 *
 * Returns nothing.
 *
 * Called from G_RunFrame()'s QCMD_DISPDISABLE handling (g_main.c), first
 * queued by listdisablesRun() and then kept going by this function.
 *
 * As with the header, the closing line says "disbled-entities" - the
 * same copy-paste from g_spawn.c.
 */
void displayNextDisable(edict_t *ent, int client, long disablecmd) {
    if (disablecmd < maxdisable_cmds) {
        switch (disablecmds[disablecmd].type) {
            case DISABLE_SW:
                gi.cprintf(ent, PRINT_HIGH, "%4ld SW:\"%s\"\n", disablecmd + 1, disablecmds[disablecmd].disablecmd);
                break;
            case DISABLE_EX:
                gi.cprintf(ent, PRINT_HIGH, "%4ld EX:\"%s\"\n", disablecmd + 1, disablecmds[disablecmd].disablecmd);
                break;
            case DISABLE_RE:
                gi.cprintf(ent, PRINT_HIGH, "%4ld RE:\"%s\"\n", disablecmd + 1, disablecmds[disablecmd].disablecmd);
                break;
        }
        disablecmd++;
        addCmdQueue(client, QCMD_DISPDISABLE, 0, disablecmd, 0);
    } else {
        gi.cprintf(ent, PRINT_HIGH, "End disbled-entities List\n");
    }
}

/**
 * "!disablecmd [SW/EX/RE] "command"" - appends one rule to the
 * disabled-command list at runtime, for shutting off a command
 * immediately without editing the disable file and reloading. That
 * immediacy is the point: if a command turns out to be exploitable
 * mid-game, this closes it off for everyone right away.
 *
 * The addition is memory-only: the next readDisableLists() (any map
 * change, or !reloaddisablefile) rebuilds from disk and drops it, so
 * anything meant to stick has to go in the file.
 *
 * Rejects an unrecognised type keyword, a blank pattern, a list already
 * at DISABLE_MAXCMDS, and a regex that won't compile - freeing the
 * string it had already allocated in that last case.
 *
 * startarg: index of the type keyword in the command's arguments; the
 *           command pattern is expected at startarg + 1.
 * ent:      who to report success or the usage line to.
 * client:   unused here.
 *
 * Returns nothing; success or refusal is reported to ent.
 *
 * Called via the "disablecmd" entry in q2aCommands[] (g_cmd.c).
 */
void disablecmdRun(int startarg, edict_t *ent, int client) {
    char *cmd;
    int len;

    if (maxdisable_cmds >= DISABLE_MAXCMDS) {
        gi.cprintf(ent, PRINT_HIGH, "Sorry, maximum number of disbled-entitie commands has been reached.\n");
        return;
    }
    if (gi.argc() <= startarg + 1) {
        gi.cprintf(ent, PRINT_HIGH, DISABLECMD);
        return;
    }

    cmd = gi.argv(startarg);
    if (Q_stricmp(cmd, "SW") == 0) {
        disablecmds[maxdisable_cmds].type = DISABLE_SW;
    } else if (Q_stricmp(cmd, "EX") == 0) {
        disablecmds[maxdisable_cmds].type = DISABLE_EX;
    } else if (Q_stricmp(cmd, "RE") == 0) {
        disablecmds[maxdisable_cmds].type = DISABLE_RE;
    } else {
        gi.cprintf(ent, PRINT_HIGH, DISABLECMD);
        return;
    }

    cmd = gi.argv(startarg + 1);
    if (isBlank(cmd)) {
        gi.cprintf(ent, PRINT_HIGH, DISABLECMD);
        return;
    }

    len = q2a_strlen(cmd) + 20;
    disablecmds[maxdisable_cmds].disablecmd = G_Malloc(len);
    processString(disablecmds[maxdisable_cmds].disablecmd, cmd, len - 1, 0);

    if (disablecmds[maxdisable_cmds].type == DISABLE_RE) {
        upperCase(cmd);
        disablecmds[maxdisable_cmds].r = re_compile(cmd);
        if (!disablecmds[maxdisable_cmds].r) {
            G_Free(disablecmds[maxdisable_cmds].disablecmd);

            // malformed re...
            gi.cprintf(ent, PRINT_HIGH, "Regular expression couldn't compile!\n");
            return;
        }
    } else {
        disablecmds[maxdisable_cmds].r = 0;
    }

    switch (disablecmds[maxdisable_cmds].type) {
        case DISABLE_SW:
            gi.cprintf(ent, PRINT_HIGH, "%4d SW:\"%s\" added\n", maxdisable_cmds + 1, disablecmds[maxdisable_cmds].disablecmd);
            break;
        case DISABLE_EX:
            gi.cprintf(ent, PRINT_HIGH, "%4d EX:\"%s\" added\n", maxdisable_cmds + 1, disablecmds[maxdisable_cmds].disablecmd);
            break;
        case DISABLE_RE:
            gi.cprintf(ent, PRINT_HIGH, "%4d RE:\"%s\" added\n", maxdisable_cmds + 1, disablecmds[maxdisable_cmds].disablecmd);
            break;
    }
    maxdisable_cmds++;
}

/**
 * "!disabledel <disablenum>" - removes one rule from the
 * disabled-command list by the number !listdisables shows, the
 * counterpart to !disablecmd for re-enabling a command at runtime.
 *
 * Frees that rule's string and shifts the remaining rules down to close
 * the gap, which keeps disablecmds[] dense at the cost of renumbering
 * everything after it - so numbers from an earlier !listdisables go
 * stale.
 *
 * Like !disablecmd this only edits the in-memory list; the next
 * readDisableLists() restores whatever the file says.
 *
 * startarg: index of the rule number in the command's arguments.
 * ent:      who to report success or the usage line to.
 * client:   unused here.
 *
 * Returns nothing; success or refusal is reported to ent.
 *
 * Called via the "disabledel" entry in q2aCommands[] (g_cmd.c).
 */
void disableDelRun(int startarg, edict_t *ent, int client) {
    int disable;

    if (gi.argc() <= startarg) {
        gi.cprintf(ent, PRINT_HIGH, DISABLEDELCMD);
        return;
    }
    disable = q2a_atoi(gi.argv(startarg));
    if (disable < 1 || disable > maxdisable_cmds) {
        gi.cprintf(ent, PRINT_HIGH, DISABLEDELCMD);
        return;
    }
    disable--;
    G_Free(disablecmds[disable].disablecmd);
    if (disable + 1 < maxdisable_cmds) {
        q2a_memmove((disablecmds + disable), (disablecmds + disable + 1), sizeof (disablecmd_t) * (maxdisable_cmds - disable));
    }
    maxdisable_cmds--;
    gi.cprintf(ent, PRINT_HIGH, "Disbled command deleted\n");
}
