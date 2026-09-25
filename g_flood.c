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

floodcmd_t floodcmds[FLOOD_MAXCMDS];
int maxflood_cmds = 0;

/**
 * Loads one flood-command file into floodcmds[]. The list names client
 * commands that should count towards chat flood protection on top of the
 * built-in say/say_team - so a mod's own chat-like command (a taunt, a
 * radio message, a custom messaging command) can be rate limited too,
 * instead of being a way around the limit.
 *
 * Each line is a match rule prefixed by its type, blank lines and ';'
 * comments ignored, anything else logged as a bad line:
 *   SW: - the command must start with this
 *   EX: - it must equal this exactly
 *   RE: - it must match this regular expression
 * RE patterns are upper-cased before compiling, pairing with
 * checkforfloodcmds() upper-casing the command it tests, so RE rules end
 * up case insensitive.
 *
 * floodname: path of the file to read.
 *
 * Returns true if the file was opened and read, false if it couldn't be
 * opened or the list was already full at FLOOD_MAXCMDS.
 *
 * Called from readFloodLists() below, once for each of the two locations
 * a flood file can live in.
 */
bool readFloodFile(char *floodname) {
    FILE *floodfile;
    unsigned int uptoLine = 0;

    if (maxflood_cmds >= FLOOD_MAXCMDS) {
        return false;
    }

    floodfile = fopen(floodname, "rt");
    if (!floodfile) {
        return false;
    }

    while (fgets(buffer, 256, floodfile)) {
        char *cp = buffer;
        int len;

        uptoLine++;

        // remove '\n'
        len = q2a_strlen(buffer) - 1;
        if (buffer[len] == '\n') {
            buffer[len] = 0x0;
        }

        SKIPBLANK(cp);

        if (startContains(cp, "SW:") || startContains(cp, "EX:") || startContains(cp, "RE:")) {
            // looks ok, add...
            switch (*cp) {
                case 'S':
                    floodcmds[maxflood_cmds].type = FLOOD_SW;
                    break;

                case 'E':
                    floodcmds[maxflood_cmds].type = FLOOD_EX;
                    break;

                case 'R':
                    floodcmds[maxflood_cmds].type = FLOOD_RE;
                    break;
            }

            cp += 3;
            SKIPBLANK(cp);

            len = q2a_strlen(cp) + 1;

            // zero length command
            if (!len) {
                continue;
            }

            floodcmds[maxflood_cmds].floodcmd = G_Malloc(len);
            Q_snprintf(floodcmds[maxflood_cmds].floodcmd, len, "%s", cp);

            if (floodcmds[maxflood_cmds].type == FLOOD_RE) {
                upperCase(cp);
                floodcmds[maxflood_cmds].r = re_compile(cp);
                if (!floodcmds[maxflood_cmds].r) {
                    // malformed re... skip this flood command
                    continue;
                }
            } else {
                floodcmds[maxflood_cmds].r = 0;
            }
            maxflood_cmds++;
            if (maxflood_cmds >= FLOOD_MAXCMDS) {
                break;
            }
        } else if (!(cp[0] == ';' || cp[0] == '\n' || isBlank(cp))) {
            gi.dprintf("Error loading FLOOD from line %d in file %s\n", uptoLine, floodname);
        }
    }
    fclose(floodfile);
    return true;
}

/**
 * Free up any allocated memory
 *
 * Releases the command strings readFloodFile()/floodcmdRun() allocated
 * and empties floodcmds[], so the list can be rebuilt without leaking
 * the previous load's strings. Only the string is freed; the compiled
 * regex needs no freeing because re_compile() returns a pointer into its
 * own static storage rather than an allocation.
 *
 * Takes no parameters; operates on the global floodcmds[]/maxflood_cmds.
 * Returns nothing.
 *
 * Called from readFloodLists() below before each reload, and from
 * SpawnEntities() (g_init.c) while tearing down the previous level's
 * config lists.
 */
void freeFloodLists(void) {
    while (maxflood_cmds) {
        maxflood_cmds--;
        G_Free(floodcmds[maxflood_cmds].floodcmd);
    }
}

/**
 * Rebuilds the flood-command list from disk: drops whatever was loaded
 * before, then reads the file named by the q2a_floodfile cvar from two
 * places - the server's working directory and moddir - merging both into
 * one list, so a server-wide default and a per-mod list can coexist
 * rather than one replacing the other. The per-mod half matters here
 * because which commands are chat-like depends entirely on the mod.
 *
 * Only warns (LT_INTERNALWARN) if *neither* location had a readable
 * file, since having just one of the two is normal.
 *
 * Takes no parameters; fills the global floodcmds[]. Returns nothing.
 *
 * Called from SpawnEntities() (g_init.c) on every level load, so the
 * list picks up edits at each map change, and from reloadFloodFileRun()
 * below.
 */
void readFloodLists(void) {
    bool ret;

    freeFloodLists();
    ret = readFloodFile(configfile_flood->string);
    Q_snprintf(buffer, sizeof(buffer), "%s/%s", moddir, configfile_flood->string);
    if (readFloodFile(buffer)) {
        ret = true;
    }
    if (!ret) {
        logEvent(LT_INTERNALWARN, 0, NULL, va("%s could not be found", configfile_flood->string), IW_FLOODSETUPLOAD, 0.0, true);
    }
}

/**
 * "!reloadfloodfile" - re-reads the flood-command list from disk, for
 * picking up edits without waiting for the next map change (which is
 * otherwise the only time readFloodLists() runs).
 *
 * startarg: unused, this command takes no arguments.
 * ent:      who to confirm to.
 * client:   unused here.
 *
 * Returns nothing.
 *
 * Called via the "reloadfloodfile" entry in q2aCommands[] (g_cmd.c),
 * from an in-game admin console or rcon.
 */
void reloadFloodFileRun(int startarg, edict_t *ent, int client) {
    readFloodLists();
    gi.cprintf(ent, PRINT_HIGH, "Flood commands reloaded.\n");
}

/**
 * Tests a command against one flood rule, applying whichever of the
 * three match types that rule was loaded as: SW (prefix), EX (whole
 * string, case insensitive) or RE (regex).
 *
 * Note re_matchp() returns the offset the match was found at, so the
 * comparison against 0 means an RE rule only fires when the pattern
 * matches at the *start* of the command - effectively anchored rather
 * than a search. Also worth knowing that re_compile() builds into a
 * single static buffer it hands a pointer into, so with more than one RE
 * rule loaded they all end up matching against whichever pattern
 * compiled last.
 *
 * cp:       the command to test, already upper-cased by
 *           checkforfloodcmds() - which is what makes EX/RE case
 *           insensitive, since the stored patterns are upper-cased too.
 * floodcmd: index into floodcmds[] of the rule to test.
 *
 * Returns true if this one rule matches.
 *
 * Called from checkforfloodcmds() below, once per loaded rule.
 */
bool checkforfloodcmd(char *cp, int floodcmd) {
    int len;
    switch (floodcmds[floodcmd].type) {
        case FLOOD_SW:
            return startContains(cp, floodcmds[floodcmd].floodcmd);
        case FLOOD_EX:
            return !Q_stricmp(cp, floodcmds[floodcmd].floodcmd);
        case FLOOD_RE:
            return (re_matchp(floodcmds[floodcmd].r, cp, &len) == 0);
    }
    return false;
}

/**
 * Decides whether a client command is one that should count towards
 * chat flood protection, by testing it against every loaded rule until
 * one matches. Without this, only say/say_team would be limited and any
 * mod command that broadcasts text would be an open channel for
 * spamming the server.
 *
 * Upper-cases the candidate into the shared `buffer` first, which is
 * what makes matching case insensitive against the equally upper-cased
 * stored patterns. That means this clobbers `buffer`, so callers can't
 * be holding anything in it across the call.
 *
 * cp: the command name the client issued.
 *
 * Returns true if any rule matches, false if none do or no rules are
 * loaded.
 *
 * Called from doClientCommand() (g_cmd.c) for any command that isn't
 * already recognised as chat. A match there subjects the command to
 * checkForMute() immediately and sets the caller's flag so
 * checkForFlood() runs once the command itself has been handled.
 */
bool checkforfloodcmds(char *cp) {
    q2a_strncpy(buffer, cp, sizeof(buffer)-1);
    upperCase(buffer);
    for (unsigned int i = 0; i < maxflood_cmds; i++) {
        if (checkforfloodcmd(buffer, i)) {
            return true;
        }
    }
    return false;
}

/**
 * Decides whether a client is currently allowed to say something, across
 * all three ways q2admin can silence someone. It's the single gate every
 * chat path consults, so an admin punishment or a flood penalty applies
 * consistently no matter which command the text came in through.
 *
 * The three levels, in order of severity:
 *   permanent (CCMD_PCSILENCE) - muted until they reconnect or an admin
 *       lifts it,
 *   temporary (CCMD_CSILENCE)  - muted until chattimeout passes, which
 *       is checked and cleared here so it expires on its own,
 *   stifled (CCMD_STIFLED)     - not silenced but rate limited to one
 *       message per stifle_length, for someone who's merely talkative
 *       rather than abusive.
 *
 * client:     whose mute state to check.
 * ent:        their edict, for the "you're still muted" notice.
 * displayMsg: whether to send that notice. Callers that check the same
 *             message more than once, or that are reporting on someone
 *             else's behalf, pass false to avoid duplicate spam.
 *
 * Returns true if the message should be suppressed, false to let it
 * through. Note this isn't a pure query in the stifled case - returning
 * false there starts the next cooldown.
 *
 * Called from doClientCommand() (g_cmd.c) on the say/say_team paths and
 * on any command checkforfloodcmds() matched, and from the extended
 * say_person/say_group handlers.
 */
bool checkForMute(int client, edict_t *ent, bool displayMsg) {
    // permanently muted
    if (proxyinfo[client].clientcommand & CCMD_PCSILENCE) {
        return true;
    }

    // temp mute
    if (proxyinfo[client].clientcommand & CCMD_CSILENCE) {
        if (proxyinfo[client].chattimeout < ltime) {
            proxyinfo[client].clientcommand &= ~CCMD_CSILENCE;
        } else {
            int secleft = (int) (proxyinfo[client].chattimeout - ltime) + 1;
            if (displayMsg) {
                gi.cprintf(ent, PRINT_HIGH, "%d seconds of chat silence left.\n", secleft);
            }
            return true;
        }
    }

    // half muted (can talk once per timespan)
    if (proxyinfo[client].clientcommand & CCMD_STIFLED) {
        int sf = proxyinfo[client].stifle_frame;
        // checkForMute() is called more than once per frame per message. Simply checking for
        // stifle_frame being larger than the current frame number is insufficient since the
        // first time this is called it will work, but subsequent calls will result in the
        // mute. So stifled client chat will show up in server console but not in the actual
        // game. Effectively, this means the client is full-muted. So you have to check if
        // the stifle_frame is the exact value of the current frame + the stifle time to
        // know if it was set THIS frame and to not apply the mute until the next frame.
        if (sf > lframenum && sf != lframenum + proxyinfo[client].stifle_length) {
            if (displayMsg) {
                int secleft = FRAMES_TO_SECS(proxyinfo[client].stifle_frame - lframenum);
                gi.cprintf(ent, PRINT_HIGH, "You're stifled for %d more seconds\n", secleft);
            }
            return true;
        } else {
            proxyinfo[client].stifle_frame = lframenum + proxyinfo[client].stifle_length;
            return false;
        }
    }
    return false;
}

/**
 * Counts a client's messages against the flood limit and punishes them
 * for crossing it. This is the actual flood detector - checkForMute()
 * only enforces a penalty that already exists, this is what decides one
 * is warranted.
 *
 * Counts messages within a rolling window (chatFloodProtectSec): once
 * the window lapses the count resets and any raised flood signal clears,
 * otherwise each message increments it until chatFloodProtectNum is
 * exceeded. A client with their own settings (see
 * clientchatfloodprotectRun()) is measured against those, everyone else
 * against the server-wide ones.
 *
 * What crossing the limit costs is driven by chatFloodProtectSilence,
 * which encodes three different punishments in one number: 0 kicks them
 * outright, negative mutes them permanently, and positive mutes them for
 * that many seconds. Either mute also raises SIGNAL_CHATFLOOD and
 * evaluates the client's signal score, so repeat flooding contributes
 * towards removal through the normal scoring system rather than being
 * judged only in isolation.
 *
 * client: the client whose message to count.
 *
 * Returns true if this message tripped the limit and a penalty was
 * applied, false if they're still within it.
 *
 * Called from the say/say_team handlers (g_cmd.c) and, after the command
 * has been processed, for any command checkforfloodcmds() matched.
 */
bool checkForFlood(int client) {
    struct chatflood_s *fi;

    if (!proxyinfo[client].floodinfo.chatFloodProtect) {
        if (!floodinfo.chatFloodProtect) {
            return false;
        }
        fi = &floodinfo;
    } else {
        fi = &proxyinfo[client].floodinfo;
    }
    if (proxyinfo[client].chattimeout < ltime) {
        proxyinfo[client].chattimeout = ltime + fi->chatFloodProtectSec;
        proxyinfo[client].chatcount = 0;
        clearSignal(client, SIGNAL_CHATFLOOD);
    } else {
        if (proxyinfo[client].chatcount >= fi->chatFloodProtectNum) {
            Q_snprintf(buffer, sizeof(buffer), chatFloodProtectMsg, proxyinfo[client].name);
            gi.bprintf(PRINT_HIGH, "%s\n", buffer);

            raiseSignal(client, SIGNAL_CHATFLOOD);

            if (fi->chatFloodProtectSilence == 0) {
                addCmdQueue(client, QCMD_DISCONNECT, 0, 0, chatFloodProtectMsg);
            } else if (fi->chatFloodProtectSilence < 0) {
                proxyinfo[client].clientcommand |= CCMD_PCSILENCE;
                evaluateSignalScore(client);
            } else {
                proxyinfo[client].chattimeout = ltime + fi->chatFloodProtectSilence;
                proxyinfo[client].clientcommand |= CCMD_CSILENCE;
                evaluateSignalScore(client);
            }
            return true;
        }
        proxyinfo[client].chatcount++;
    }
    return false;
}

/**
 * Parses the namechangefloodprotect setting from a config file: three
 * whitespace separated numbers, "num sec silence", limiting how often a
 * player may rename. Rapid name changing is a nuisance tactic - it
 * spams everyone's console and makes a player hard to identify or
 * report - so it gets its own limit separate from chat.
 *
 * The feature only switches on if all three values are present, so a
 * half-written setting leaves it off rather than half configured.
 *
 * arg: the raw value string - count, window in seconds, and the penalty
 *      (which follows the same 0/negative/positive convention as
 *      checkForFlood()'s silence).
 *
 * Returns nothing; updates the module's nameChangeFloodProtect* globals.
 *
 * Called from readCfgFile() (g_cmd.c) as the initfunc for the
 * "namechangefloodprotect" entry in q2aCommands[], at config load.
 */
void nameChangeFloodProtectInit(char *arg) {
    nameChangeFloodProtect = false;

    if (*arg) {
        nameChangeFloodProtectNum = q2a_atoi(arg);
        while (*arg && *arg != ' ') {
            arg++;
        }
        SKIPBLANK(arg);
        if (*arg) {
            nameChangeFloodProtectSec = q2a_atoi(arg);
            while (*arg && *arg != ' ') {
                arg++;
            }
            SKIPBLANK(arg);
            if (*arg) {
                nameChangeFloodProtectSilence = q2a_atoi(arg);
                nameChangeFloodProtect = true;
            }
        }
    }
}

/**
 * "!namechangefloodprotect [num sec silence]" - the console counterpart
 * to nameChangeFloodProtectInit(), for adjusting the name-change limit
 * live rather than editing the config and reloading. With no arguments
 * it just reports the current setting.
 *
 * startarg: index of the first of the three numbers in the command's
 *           arguments. All three are required to enable it; a single
 *           argument turns the feature off, which is how it's disabled
 *           from the console.
 * ent:      who to report the resulting setting to.
 * client:   unused here.
 *
 * Returns nothing; always prints the setting as it now stands, so the
 * admin sees the effect whether or not anything changed.
 *
 * Called via the "namechangefloodprotect" entry in q2aCommands[]
 * (g_cmd.c).
 */
void nameChangeFloodProtectRun(int startarg, edict_t *ent, int client) {
    if (gi.argc() > startarg + 2) {
        nameChangeFloodProtectNum = q2a_atoi(gi.argv(startarg));
        nameChangeFloodProtectSec = q2a_atoi(gi.argv(startarg + 1));
        nameChangeFloodProtectSilence = q2a_atoi(gi.argv(startarg + 2));
        nameChangeFloodProtect = true;
    } else if (gi.argc() > startarg) {
        nameChangeFloodProtect = false;
    }
    if (nameChangeFloodProtect) {
        gi.cprintf(ent, PRINT_HIGH, "namechangefloodprotect %d %d %d\n", nameChangeFloodProtectNum, nameChangeFloodProtectSec, nameChangeFloodProtectSilence);
    } else {
        gi.cprintf(ent, PRINT_HIGH, "namechangefloodprotect disabled\n");
    }
}

/**
 * Parses the skinchangefloodprotect setting from a config file, in the
 * same "num sec silence" form as nameChangeFloodProtectInit(), limiting
 * how often a player may change skin. Rapid skin switching is both
 * visually disruptive and makes every other client reload models, so
 * it's worth limiting for the same reasons as name changes.
 *
 * As with the name-change version, all three values must be present for
 * the feature to switch on.
 *
 * arg: the raw value string - count, window in seconds, and penalty.
 *
 * Returns nothing; updates the module's skinChangeFloodProtect* globals.
 *
 * Called from readCfgFile() (g_cmd.c) as the initfunc for the
 * "skinchangefloodprotect" entry in q2aCommands[], at config load.
 */
void skinChangeFloodProtectInit(char *arg) {
    skinChangeFloodProtect = false;

    if (*arg) {
        skinChangeFloodProtectNum = q2a_atoi(arg);
        while (*arg && *arg != ' ') {
            arg++;
        }
        SKIPBLANK(arg);
        if (*arg) {
            skinChangeFloodProtectSec = q2a_atoi(arg);
            while (*arg && *arg != ' ') {
                arg++;
            }
            SKIPBLANK(arg);
            if (*arg) {
                skinChangeFloodProtectSilence = q2a_atoi(arg);
                skinChangeFloodProtect = true;
            }
        }
    }
}

/**
 * "!skinchangefloodprotect [num sec silence]" - the console counterpart
 * to skinChangeFloodProtectInit(), for adjusting the skin-change limit
 * live. With no arguments it just reports the current setting.
 *
 * startarg: index of the first of the three numbers in the command's
 *           arguments. All three are required to enable it; a single
 *           argument turns the feature off.
 * ent:      who to report the resulting setting to.
 * client:   unused here.
 *
 * Returns nothing; always prints the setting as it now stands.
 *
 * Called via the "skinchangefloodprotect" entry in q2aCommands[]
 * (g_cmd.c).
 */
void skinChangeFloodProtectRun(int startarg, edict_t *ent, int client) {
    if (gi.argc() > startarg + 2) {
        skinChangeFloodProtectNum = q2a_atoi(gi.argv(startarg));
        skinChangeFloodProtectSec = q2a_atoi(gi.argv(startarg + 1));
        skinChangeFloodProtectSilence = q2a_atoi(gi.argv(startarg + 2));
        skinChangeFloodProtect = true;
    } else if (gi.argc() > startarg) {
        skinChangeFloodProtect = false;
    }
    if (skinChangeFloodProtect) {
        gi.cprintf(ent, PRINT_HIGH, "skinchangefloodprotect %d %d %d\n", skinChangeFloodProtectNum, skinChangeFloodProtectSec, skinChangeFloodProtectSilence);
    } else {
        gi.cprintf(ent, PRINT_HIGH, "skinchangefloodprotect disabled\n");
    }
}

/**
 * Parses the chatfloodprotect setting from a config file, in the same
 * "num sec silence" form as the name and skin versions - this one being
 * the server-wide default checkForFlood() measures everyone against
 * unless they have a per-player override.
 *
 * Unlike the other two this additionally requires the count and window
 * to be non-zero before enabling, since a limit of zero messages or a
 * zero-length window would silence everyone outright.
 *
 * arg: the raw value string - count, window in seconds, and penalty
 *      (0 kicks, negative mutes permanently, positive mutes for that
 *      many seconds - see checkForFlood()).
 *
 * Returns nothing; updates the global floodinfo.
 *
 * Called from readCfgFile() (g_cmd.c) as the initfunc for the
 * "chatfloodprotect" entry in q2aCommands[], at config load.
 */
void chatFloodProtectInit(char *arg) {
    floodinfo.chatFloodProtect = false;

    if (*arg) {
        floodinfo.chatFloodProtectNum = q2a_atoi(arg);
        while (*arg && *arg != ' ') {
            arg++;
        }
        SKIPBLANK(arg);
        if (*arg) {
            floodinfo.chatFloodProtectSec = q2a_atoi(arg);
            while (*arg && *arg != ' ') {
                arg++;
            }
            SKIPBLANK(arg);
            if (*arg) {
                floodinfo.chatFloodProtectSilence = q2a_atoi(arg);
                if (floodinfo.chatFloodProtectNum && floodinfo.chatFloodProtectSec) {
                    floodinfo.chatFloodProtect = true;
                }
            }
        }
    }
}

/**
 * "!chatfloodprotect [num sec silence]" - the console counterpart to
 * chatFloodProtectInit(), for tuning the server-wide chat limit live,
 * which is useful when a server is being actively spammed and the
 * current limit is clearly too loose. With no arguments it reports the
 * current setting.
 *
 * startarg: index of the first of the three numbers in the command's
 *           arguments. All three are required, and as in the config
 *           version a zero count or window leaves it disabled; a single
 *           argument turns the feature off.
 * ent:      who to report the resulting setting to.
 * client:   unused here.
 *
 * Returns nothing; always prints the setting as it now stands.
 *
 * Called via the "chatfloodprotect" entry in q2aCommands[] (g_cmd.c).
 */
void chatFloodProtectRun(int startarg, edict_t *ent, int client) {
    if (gi.argc() > startarg + 2) {
        floodinfo.chatFloodProtectNum = q2a_atoi(gi.argv(startarg));
        floodinfo.chatFloodProtectSec = q2a_atoi(gi.argv(startarg + 1));
        floodinfo.chatFloodProtectSilence = q2a_atoi(gi.argv(startarg + 2));
        if (floodinfo.chatFloodProtectNum && floodinfo.chatFloodProtectSec) {
            floodinfo.chatFloodProtect = true;
        } else {
            floodinfo.chatFloodProtect = false;
        }
    } else if (gi.argc() > startarg) {
        floodinfo.chatFloodProtect = false;
    }
    if (floodinfo.chatFloodProtect) {
        gi.cprintf(ent, PRINT_HIGH, "chatfloodprotect %d %d %d\n", floodinfo.chatFloodProtectNum, floodinfo.chatFloodProtectSec, floodinfo.chatFloodProtectSilence);
    } else {
        gi.cprintf(ent, PRINT_HIGH, "chatfloodprotect disabled\n");
    }
}

/**
 * "!mute <player> <seconds|PERM>" - silences a player by hand, for when
 * an admin judges someone abusive rather than waiting for the automatic
 * flood limits to catch them. Passing 0 seconds unmutes, so one command
 * covers applying and lifting a mute.
 *
 * A timed mute sets CCMD_CSILENCE with a deadline checkForMute() expires
 * on its own; PERM sets CCMD_PCSILENCE, which lasts until it's lifted or
 * they reconnect. Applying a timed mute clears any permanent one, so
 * downgrading works without unmuting first.
 *
 * Announces to the whole server, not just the admin - the point being
 * that everyone can see the silence was deliberate, and the target is
 * told why they can no longer talk.
 *
 * startarg: unused; arguments are parsed from the raw argument string so
 *           quoted player names survive.
 * ent:      the admin, for replies and the usage line. NULL when issued
 *           from the server console, which is why the confirmations to
 *           ent are conditional.
 * client:   the admin's client index, used to resolve the target name.
 *
 * Returns nothing; prints the usage line if the target or duration can't
 * be parsed.
 *
 * Called via the "mute" entry in q2aCommands[] (g_cmd.c).
 *
 * Note the PERM branch doesn't check that a player was actually matched,
 * so "!mute <unknown> PERM" acts on whatever index the failed lookup
 * left behind.
 */
void muteRun(int startarg, edict_t *ent, int client) {
    char *text;
    edict_t *enti;
    int clienti;
    int seconds;

    // skip the first part (!mute)
    text = getArgs();
    if (!ent) {
        while (*text != ' ') {
            text++;
        }
    }

    SKIPBLANK(text);

    enti = getClientFromArg(client, ent, &clienti, text, &text);

    // make sure the text doesn't overflow the internal buffer...
    if (enti && isdigit(*text) && (seconds = q2a_atoi(text)) >= 0) {
        if (seconds) {
            gi.cprintf(NULL, PRINT_HIGH, "%s has been muted for %d seconds.\n", proxyinfo[clienti].name, seconds);

            if (ent) {
                gi.cprintf(ent, PRINT_HIGH, "%s has been muted for %d seconds.\n", proxyinfo[clienti].name, seconds);
            }

            gi.cprintf(enti, PRINT_HIGH, "You have been muted for %d seconds.\n", seconds);

            proxyinfo[clienti].chattimeout = ltime + seconds;
            proxyinfo[clienti].clientcommand &= ~CCMD_PCSILENCE;
            proxyinfo[clienti].clientcommand |= CCMD_CSILENCE;
        } else if (proxyinfo[clienti].clientcommand & (CCMD_CSILENCE | CCMD_PCSILENCE)) {
            gi.cprintf(NULL, PRINT_HIGH, "%s has been unmuted.\n", proxyinfo[clienti].name);
            if (ent) {
                gi.cprintf(ent, PRINT_HIGH, "%s has been unmuted.\n", proxyinfo[clienti].name);
            }
            gi.cprintf(enti, PRINT_HIGH, "You have been unmuted.\n");
            proxyinfo[clienti].clientcommand &= ~(CCMD_CSILENCE | CCMD_PCSILENCE);
        }
    } else if (Q_stricmp(text, "PERM") == 0) {
        gi.cprintf(NULL, PRINT_HIGH, "%s has been muted.\n", proxyinfo[clienti].name);
        if (ent) {
            gi.cprintf(ent, PRINT_HIGH, "%s has been muted.\n", proxyinfo[clienti].name);
        }
        gi.cprintf(enti, PRINT_HIGH, "You have been muted.\n");
        proxyinfo[clienti].clientcommand |= CCMD_PCSILENCE;
    } else {
        gi.cprintf(ent, PRINT_HIGH, "[sv] !mute %s <time(seconds)/PERM>\n", PLAYERSPEC);
    }
}

/**
 * "!stifle <player> <seconds>" - rate limits a player's chat to one
 * message every <seconds> rather than silencing them outright. The
 * middle ground between leaving someone alone and muting them: a player
 * who's talking too much but not abusively can still participate, just
 * not flood.
 *
 * Passing 0 seconds unstifles, so as with !mute one command both applies
 * and lifts. stifle_frame is set to 0 so the very next message is
 * allowed through and starts the cooldown, rather than the player being
 * silently blocked until a first interval elapses.
 *
 * startarg: unused; arguments are parsed from the raw argument string.
 * ent:      the admin, for replies and the usage line. NULL from the
 *           server console.
 * client:   the admin's client index, used to resolve the target name.
 *
 * Returns nothing; prints the usage line if the target or duration can't
 * be parsed.
 *
 * Called via the "stifle" entry in q2aCommands[] (g_cmd.c).
 */
void stifleRun(int startarg, edict_t *ent, int client) {
    char *text;
    edict_t *enti;
    int clienti;
    int seconds;

    // skip the first part (!stifle)
    text = getArgs();
    if (!ent) {
        while (*text != ' ') {
            text++;
        }
    }

    SKIPBLANK(text);

    enti = getClientFromArg(client, ent, &clienti, text, &text);

    // make sure the text doesn't overflow the internal buffer...
    if (enti && isdigit(*text) && (seconds = q2a_atoi(text)) >= 0) {
        if (seconds) {
            gi.cprintf(NULL, PRINT_HIGH, "%s is now stifled for %d seconds.\n", proxyinfo[clienti].name, seconds);
            if (ent) {
                gi.cprintf(ent, PRINT_HIGH, "%s is now stifled for %d seconds.\n", proxyinfo[clienti].name, seconds);
            }
            gi.cprintf(enti, PRINT_HIGH, "You are now stifled for %d seconds.\n", seconds);
            proxyinfo[clienti].clientcommand |= CCMD_STIFLED;
            proxyinfo[clienti].stifle_frame = 0; // next print will trigger
            proxyinfo[clienti].stifle_length = SECS_TO_FRAMES(seconds);
        } else if (proxyinfo[clienti].clientcommand & CCMD_STIFLED) {
            gi.cprintf(NULL, PRINT_HIGH, "%s has been unstifled.\n", proxyinfo[clienti].name);
            if (ent) {
                gi.cprintf(ent, PRINT_HIGH, "%s has been unstifled.\n", proxyinfo[clienti].name);
            }
            gi.cprintf(enti, PRINT_HIGH, "You have been unstifled.\n");
            proxyinfo[clienti].clientcommand &= ~CCMD_STIFLED;
        }
    } else {
        gi.cprintf(ent, PRINT_HIGH, "[sv] !stifle %s <seconds>\n", PLAYERSPEC);
    }
}

/**
 * "!unstifle <player>" - lifts a stifle. Does the same job as "!stifle
 * <player> 0" but reads more obviously as an undo, and additionally
 * resets the cooldown bookkeeping (stifle_frame and stifle_length) so no
 * stale timing is left behind if they're stifled again later.
 *
 * startarg: unused; the player is parsed from the raw argument string.
 * ent:      the admin, for replies and the usage line. NULL from the
 *           server console.
 * client:   the admin's client index, used to resolve the target name.
 *
 * Returns nothing. Silently does nothing if the player wasn't stifled,
 * so it's safe to use speculatively; prints the usage line if no player
 * matched.
 *
 * Called via the "unstifle" entry in q2aCommands[] (g_cmd.c).
 */
void unstifleRun(int startarg, edict_t *ent, int client) {
    char *text;
    edict_t *enti;
    int clienti;

    // skip the first part (!unstifle)
    text = getArgs();
    if (!ent) {
        while (*text != ' ') {
            text++;
        }
    }

    SKIPBLANK(text);

    enti = getClientFromArg(client, ent, &clienti, text, &text);

    if (enti) {
        if (proxyinfo[clienti].clientcommand & CCMD_STIFLED) {
            gi.cprintf(NULL, PRINT_HIGH, "%s has been unstifled.\n", proxyinfo[clienti].name);
            if (ent) {
                gi.cprintf(ent, PRINT_HIGH, "%s has been unstifled.\n", proxyinfo[clienti].name);
            }
            gi.cprintf(enti, PRINT_HIGH, "You have been unstifled.\n");
            proxyinfo[clienti].clientcommand &= ~CCMD_STIFLED;
            proxyinfo[clienti].stifle_frame = 0;
            proxyinfo[clienti].stifle_length = 0;
        }
    } else {
        gi.cprintf(ent, PRINT_HIGH, "[sv] !unstifle %s\n", PLAYERSPEC);
    }
}

/**
 * Set custom chat flood protection rules for a particular player.
 *
 * Format: sv !clientchatfloodprotection [CL id | name] chatnum secs silence
 *
 * Overrides the server-wide chat limit for one player, which lets an
 * admin tighten the rules on someone pushing them without making the
 * server stricter for everyone - or loosen them for someone who
 * legitimately talks a lot. checkForFlood() prefers these settings over
 * the global ones whenever they're set.
 *
 * The override lives in the player's proxyinfo, so it lasts only as long
 * as their connection; it isn't remembered if they reconnect.
 *
 * Three forms, by what follows the player: three numbers sets the
 * override, any other text disables it and returns them to the global
 * settings, and nothing at all just reports what they're currently on.
 *
 * startarg: unused; arguments are parsed from the raw argument string.
 * ent:      the admin, for replies and the usage line.
 * client:   the admin's client index, used to resolve the target name.
 *
 * Returns nothing; prints the usage line if nothing matched. As with the
 * global version a zero count or window is rejected rather than
 * silencing the player entirely.
 *
 * Called via the "clientchatfloodprotect" entry in q2aCommands[]
 * (g_cmd.c).
 */
void clientchatfloodprotectRun(int startarg, edict_t *ent, int client) {
    char *text;
    edict_t *enti;
    int clienti;

    // skip the first part (!clientchatfloodprotect)
    text = getArgs();
    while (*text != ' ') {
        text++;
    }
    SKIPBLANK(text);

    enti = getClientFromArg(client, ent, &clienti, text, &text);

    if (enti && isdigit(*text)) {
        int chatFloodProtectNum;
        int chatFloodProtectSec;
        int chatFloodProtectSilence;

        chatFloodProtectNum = q2a_atoi(text);
        while (isdigit(*text)) {
            text++;
        }
        SKIPBLANK(text);

        chatFloodProtectSec = q2a_atoi(text);
        while (isdigit(*text)) {
            text++;
        }
        SKIPBLANK(text);

        chatFloodProtectSilence = q2a_atoi(text);
        while (isdigit(*text)) {
            text++;
        }
        SKIPBLANK(text);

        if (chatFloodProtectNum && chatFloodProtectSec) {
            proxyinfo[clienti].floodinfo.chatFloodProtect = true;
            proxyinfo[clienti].floodinfo.chatFloodProtectNum = chatFloodProtectNum;
            proxyinfo[clienti].floodinfo.chatFloodProtectSec = chatFloodProtectSec;
            proxyinfo[clienti].floodinfo.chatFloodProtectSilence = chatFloodProtectSilence;

            gi.cprintf(ent, PRINT_HIGH, "%s clientchatfloodprotect %d %d %d\n", proxyinfo[clienti].name, proxyinfo[clienti].floodinfo.chatFloodProtectNum, proxyinfo[clienti].floodinfo.chatFloodProtectSec, proxyinfo[clienti].floodinfo.chatFloodProtectSilence);
            return;
        }
    } else if (enti && *text) {
        proxyinfo[clienti].floodinfo.chatFloodProtect = false;
        gi.cprintf(ent, PRINT_HIGH, "%s clientchatfloodprotect disabled\n", proxyinfo[clienti].name);
        return;
    } else if (enti) {
        if (proxyinfo[clienti].floodinfo.chatFloodProtect) {
            gi.cprintf(ent, PRINT_HIGH, "%s clientchatfloodprotect %d %d %d\n", proxyinfo[clienti].name, proxyinfo[clienti].floodinfo.chatFloodProtectNum, proxyinfo[clienti].floodinfo.chatFloodProtectSec, proxyinfo[clienti].floodinfo.chatFloodProtectSilence);
        } else {
            gi.cprintf(ent, PRINT_HIGH, "%s clientchatfloodprotect disabled\n", proxyinfo[clienti].name);
        }
        return;
    }
    gi.cprintf(ent, PRINT_HIGH, "[sv] !clientchatfloodprotect %s <[xxx(num) xxx(sec) xxx(silence) / disable]>\n", PLAYERSPEC);
}

/**
 * "!listfloods" - shows the loaded flood-command rules, so an admin can
 * see which commands are actually being counted towards the chat limit.
 *
 * Prints the header itself and then queues QCMD_DISPFLOOD, which walks
 * the rules one per frame via displayNextFlood() rather than dumping a
 * list of up to FLOOD_MAXCMDS entries into the client at once.
 *
 * startarg: unused, this command takes no arguments.
 * ent:      who to print the header to.
 * client:   whose command queue the listing is paced through.
 *
 * Returns nothing.
 *
 * Called via the "listfloods" entry in q2aCommands[] (g_cmd.c).
 */
void listfloodsRun(int startarg, edict_t *ent, int client) {
    addCmdQueue(client, QCMD_DISPFLOOD, 0, 0, 0);
    gi.cprintf(ent, PRINT_HIGH, "Start flood commands List:\n");
}

/**
 * Prints one flood rule - its 1-based number, type prefix and pattern,
 * in the same "SW:/EX:/RE:" form the flood file uses, so what's shown
 * can be matched against (or pasted into) that file.
 *
 * Re-queues itself for the next index until the list runs out, then
 * prints the closing line. That pacing is why this is split out of
 * listfloodsRun(): one rule per queued-command tick keeps a long list
 * from flooding the client, the same approach displayNextBan() and
 * friends use.
 *
 * ent:      who to print to.
 * client:   their client index, used to queue the next tick.
 * floodcmd: 0-based index into floodcmds[] of the rule to print; past
 *           the end means stop and print the footer.
 *
 * Returns nothing.
 *
 * Called from G_RunFrame()'s QCMD_DISPFLOOD handling (g_main.c), first
 * queued by listfloodsRun() and then kept going by this function.
 */
void displayNextFlood(edict_t *ent, int client, long floodcmd) {
    if (floodcmd < maxflood_cmds) {
        switch (floodcmds[floodcmd].type) {
            case FLOOD_SW:
                gi.cprintf(ent, PRINT_HIGH, "%4ld SW:\"%s\"\n", floodcmd + 1, floodcmds[floodcmd].floodcmd);
                break;
            case FLOOD_EX:
                gi.cprintf(ent, PRINT_HIGH, "%4ld EX:\"%s\"\n", floodcmd + 1, floodcmds[floodcmd].floodcmd);
                break;
            case FLOOD_RE:
                gi.cprintf(ent, PRINT_HIGH, "%4ld RE:\"%s\"\n", floodcmd + 1, floodcmds[floodcmd].floodcmd);
                break;
        }
        floodcmd++;
        addCmdQueue(client, QCMD_DISPFLOOD, 0, floodcmd, 0);
    } else {
        gi.cprintf(ent, PRINT_HIGH, "End flood commands List\n");
    }
}

/**
 * "chat_stats <player>" - dumps a player's chat tracking figures, for
 * deciding whether someone is a chatbot rather than just talkative.
 *
 * None of these numbers mean much alone, which is why they're shown
 * together: a high chat rate is just as likely to be a chatty regular,
 * and low movement on its own is someone idling in spectator. It's the
 * combination - lots of words against almost no distance covered - that
 * separates a parked bot from a player, and the stored recent messages
 * are what let an admin confirm it by eye before acting.
 *
 * Words and distance are the decaying accumulators (see CHATPEST_HALFLIFE),
 * so they describe recent behaviour rather than the whole session, and
 * both will read lower than a raw running total.
 *
 * startarg: unused; the player is parsed from the raw argument string so
 *           quoted names survive.
 * ent:      who to print to. NULL when run from the server console, which
 *           is the only place this is registered.
 * client:   the invoking client index, used to resolve the target name.
 *
 * Returns nothing; prints the usage line if no player matched.
 *
 * Called via the "chat_stats" entry in q2aCommands[] (g_cmd.c).
 */
void chatStatsRun(int startarg, edict_t *ent, int client) {
    char *text;
    edict_t *enti;
    int clienti;
    chatstats_t *chat;
    float miles;

    text = getArgs();
    if (!ent) {
        while (*text != ' ') {
            text++;
        }
    }
    SKIPBLANK(text);
    enti = getClientFromArg(client, ent, &clienti, text, &text);

    if (!enti) {
        gi.cprintf(ent, PRINT_HIGH, "[sv] chat_stats %s\n", PLAYERSPEC);
        return;
    }

    chat = &proxyinfo[clienti].chat_stats;
    miles = proxyinfo[clienti].distance_moved / WORLDUNITSPERMILE;

    cprintf_internal(ent, PRINT_HIGH, "Chat stats for %s:\n", proxyinfo[clienti].name);
    cprintf_internal(ent, PRINT_HIGH, "  words:        %.1f (decaying, %.0fs half-life)\n",
            chat->words, (float) CHATPEST_HALFLIFE);
    cprintf_internal(ent, PRINT_HIGH, "  distance:     %.0f units (%.3f miles)\n",
            proxyinfo[clienti].distance_moved, miles);
    cprintf_internal(ent, PRINT_HIGH, "  words/mile:   %.1f\n", proxyinfo[clienti].words_per_mile);
    cprintf_internal(ent, PRINT_HIGH, "  chars/sec:    %.2f\n", chat->chatrate);
    cprintf_internal(ent, PRINT_HIGH, "  total chars:  %d\n", chat->printchars);

    // The stored messages are a circular buffer, so walk back from the
    // most recent rather than printing the array in slot order - the
    // newest few are what an admin actually wants to read.
    for (int i = 0; i < MSG_SAVE_COUNT; i++) {
        int idx = (chat->last_index - 1 - i + (MSG_SAVE_COUNT * 2)) % MSG_SAVE_COUNT;

        if (chat->last[idx][0]) {
            cprintf_internal(ent, PRINT_HIGH, "  recent[%d]:    %s", i + 1, chat->last[idx]);
        }
    }
}

/**
 * "!floodcmd [SW/EX/RE] "command"" - appends one rule to the
 * flood-command list at runtime, for bringing a command under the chat
 * limit without editing the flood file and reloading. Useful when a
 * particular mod command turns out to be a spam vector mid-session.
 *
 * The addition is memory-only: the next readFloodLists() (any map
 * change, or !reloadfloodfile) rebuilds from disk and drops it, so
 * anything meant to stick has to go in the file.
 *
 * Rejects an unrecognised type keyword, a blank pattern, a list already
 * at FLOOD_MAXCMDS, and a regex that won't compile - freeing the string
 * it had already allocated in that last case.
 *
 * startarg: index of the type keyword in the command's arguments; the
 *           command pattern is expected at startarg + 1.
 * ent:      who to report success or the usage line to.
 * client:   unused here.
 *
 * Returns nothing; success or refusal is reported to ent.
 *
 * Called via the "floodcmd" entry in q2aCommands[] (g_cmd.c).
 */
void floodcmdRun(int startarg, edict_t *ent, int client) {
    char *cmd;
    int len;

    if (maxflood_cmds >= FLOOD_MAXCMDS) {
        gi.cprintf(ent, PRINT_HIGH, "Sorry, maximum number of flood commands has been reached.\n");
        return;
    }

    if (gi.argc() <= startarg + 1) {
        gi.cprintf(ent, PRINT_HIGH, FLOODCMD);
        return;
    }

    cmd = gi.argv(startarg);

    if (Q_stricmp(cmd, "SW") == 0) {
        floodcmds[maxflood_cmds].type = FLOOD_SW;
    } else if (Q_stricmp(cmd, "EX") == 0) {
        floodcmds[maxflood_cmds].type = FLOOD_EX;
    } else if (Q_stricmp(cmd, "RE") == 0) {
        floodcmds[maxflood_cmds].type = FLOOD_RE;
    } else {
        gi.cprintf(ent, PRINT_HIGH, FLOODCMD);
        return;
    }

    cmd = gi.argv(startarg + 1);

    if (isBlank(cmd)) {
        gi.cprintf(ent, PRINT_HIGH, FLOODCMD);
        return;
    }

    len = q2a_strlen(cmd) + 20;

    floodcmds[maxflood_cmds].floodcmd = G_Malloc(len);
    processString(floodcmds[maxflood_cmds].floodcmd, cmd, len - 1, 0);

    if (floodcmds[maxflood_cmds].type == FLOOD_RE) {
        upperCase(cmd);
        floodcmds[maxflood_cmds].r = re_compile(cmd);
        if (!floodcmds[maxflood_cmds].r) {
            G_Free(floodcmds[maxflood_cmds].floodcmd);

            // malformed re...
            gi.cprintf(ent, PRINT_HIGH, "Regular expression couldn't compile!\n");
            return;
        }
    } else {
        floodcmds[maxflood_cmds].r = 0;
    }

    switch (floodcmds[maxflood_cmds].type) {
        case FLOOD_SW:
            gi.cprintf(ent, PRINT_HIGH, "%4d SW:\"%s\" added\n", maxflood_cmds + 1, floodcmds[maxflood_cmds].floodcmd);
            break;
        case FLOOD_EX:
            gi.cprintf(ent, PRINT_HIGH, "%4d EX:\"%s\" added\n", maxflood_cmds + 1, floodcmds[maxflood_cmds].floodcmd);
            break;
        case FLOOD_RE:
            gi.cprintf(ent, PRINT_HIGH, "%4d RE:\"%s\" added\n", maxflood_cmds + 1, floodcmds[maxflood_cmds].floodcmd);
            break;
    }
    maxflood_cmds++;
}

/**
 * "!flooddel <floodnum>" - removes one rule from the flood-command list
 * by the number !listfloods shows, the counterpart to !floodcmd for
 * withdrawing something at runtime.
 *
 * Frees that rule's string and shifts the remaining rules down to close
 * the gap, which keeps floodcmds[] dense at the cost of renumbering
 * everything after it - so numbers from an earlier !listfloods go stale.
 *
 * Like !floodcmd this only edits the in-memory list; the next
 * readFloodLists() restores whatever the file says.
 *
 * startarg: index of the rule number in the command's arguments.
 * ent:      who to report success or the usage line to.
 * client:   unused here.
 *
 * Returns nothing; success or refusal is reported to ent.
 *
 * Called via the "flooddel" entry in q2aCommands[] (g_cmd.c).
 */
void floodDelRun(int startarg, edict_t *ent, int client) {
    int flood;

    if (gi.argc() <= startarg) {
        gi.cprintf(ent, PRINT_HIGH, FLOODDELCMD);
        return;
    }

    flood = q2a_atoi(gi.argv(startarg));
    if (flood < 1 || flood > maxflood_cmds) {
        gi.cprintf(ent, PRINT_HIGH, FLOODDELCMD);
        return;
    }

    flood--;
    G_Free(floodcmds[flood].floodcmd);
    if (flood + 1 < maxflood_cmds) {
        q2a_memmove((floodcmds + flood), (floodcmds + flood + 1), sizeof (floodcmd_t) * (maxflood_cmds - flood));
    }
    maxflood_cmds--;
    gi.cprintf(ent, PRINT_HIGH, "flood command deleted\n");
}
