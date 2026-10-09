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

spawncmd_t spawncmds[SPAWN_MAXCMDS];
int maxspawn_cmds = 0;
int entity_classname_offset = (sizeof (struct edict_s)) + 20; // default byte offset to the classname variable.

/**
 * Loads one entity-disable file into spawncmds[]. The list names entity
 * classnames that should never spawn, which lets an admin strip things
 * out of stock maps without editing the .bsp - removing a weapon or
 * powerup a server doesn't want, or an entity a mod handles badly.
 *
 * Each line is a match rule prefixed by its type, blank lines and ';'
 * comments ignored, anything else logged as a bad line:
 *   SW: - the classname must start with this
 *   EX: - it must equal this exactly
 *   RE: - it must match this regular expression
 * RE patterns are upper-cased before compiling, pairing with
 * checkDisabledEntities() upper-casing the classname it tests, so RE
 * rules end up case insensitive.
 *
 * Any rule that would match worldspawn is defused rather than kept -
 * overwritten with the harmless literal "removed" - because worldspawn
 * carries the map itself and removing it would take the level with it.
 *
 * spawnname:    path of the file to read.
 * onelevelflag: stored on each rule this file contributes. True marks
 *               rules that came from a per-map file and are only meant
 *               to last the current level (see freeOneLevelSpawnLists()).
 *
 * Returns true if the file was opened and read, false if it couldn't be
 * opened or the list was already full at SPAWN_MAXCMDS.
 *
 * Called from readSpawnLists() below for the two server-wide locations,
 * and directly from SpawnEntities() (g_init.c) for the optional per-map
 * q2adminmaps/<mapname>.q2aspawn file, which is where onelevelflag is
 * passed true.
 */
bool readSpawnFile(char *spawnname, bool onelevelflag) {
    FILE *spawnfile;
    unsigned int uptoLine = 0;

    if (maxspawn_cmds >= SPAWN_MAXCMDS) {
        return false;
    }

    spawnfile = fopen(spawnname, "rt");
    if (!spawnfile) {
        return false;
    }

    while (fgets(buffer, 256, spawnfile)) {
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
                    spawncmds[maxspawn_cmds].type = SPAWN_SW;
                    break;

                case 'E':
                    spawncmds[maxspawn_cmds].type = SPAWN_EX;
                    break;

                case 'R':
                    spawncmds[maxspawn_cmds].type = SPAWN_RE;
                    break;
            }

            spawncmds[maxspawn_cmds].onelevelflag = onelevelflag;

            cp += 3;
            SKIPBLANK(cp);

            len = q2a_strlen(cp) + 1;

            // zero length command
            if (!len) {
                gi.dprintf("Error loading SPAWN from line %d in file %s\n", uptoLine, spawnname);
                continue;
            }

            spawncmds[maxspawn_cmds].spawncmd = G_Malloc(len);
            q2a_strcpy(spawncmds[maxspawn_cmds].spawncmd, cp);

            if (spawncmds[maxspawn_cmds].type == SPAWN_RE) {
                upperCase(cp);
                spawncmds[maxspawn_cmds].r = re_compile(cp);
                if (!spawncmds[maxspawn_cmds].r) {
                    // malformed re... skip this spawn command
                    gi.dprintf("Error loading SPAWN from line %d in file %s\n", uptoLine, spawnname);
                    continue;
                }

                // don't allow disabling worldspawn. Bad things will happen
                int len;
                if (re_matchp(spawncmds[maxspawn_cmds].r, "WORLDSPAWN", &len) == 0){
                    q2a_strcpy(spawncmds[maxspawn_cmds].spawncmd, "removed");
                    spawncmds[maxspawn_cmds].r = 0;
                    spawncmds[maxspawn_cmds].type = SPAWN_EX;
                    continue;
                }
            } else {
                // Don't allow disabling worldspawn. Bad things will happen
                if (startContains(spawncmds[maxspawn_cmds].spawncmd, "world")) {
                    q2a_strcpy(spawncmds[maxspawn_cmds].spawncmd, "removed");
                    continue;
                }
                spawncmds[maxspawn_cmds].r = 0;
            }

            maxspawn_cmds++;

            if (maxspawn_cmds >= SPAWN_MAXCMDS) {
                break;
            }
        } else if (!(cp[0] == ';' || cp[0] == '\n' || isBlank(cp))) {
            gi.dprintf("Error loading SPAWN from line %d in file %s\n", uptoLine, spawnname);
        }
    }
    fclose(spawnfile);
    return true;
}

/**
 * Releases the classname strings readSpawnFile()/spawncmdRun()
 * allocated and empties spawncmds[], so the list can be rebuilt from
 * scratch without leaking the previous load's strings.
 *
 * Only the string is freed; the compiled regex needs no freeing because
 * re_compile() returns a pointer into its own static storage rather than
 * an allocation.
 *
 * Takes no parameters; operates on the global spawncmds[]/maxspawn_cmds.
 * Returns nothing.
 *
 * Called from readSpawnLists() below before each reload.
 */
void freeSpawnLists(void) {
    while (maxspawn_cmds) {
        maxspawn_cmds--;
        G_Free(spawncmds[maxspawn_cmds].spawncmd);
    }
}

/**
 * Drops just the rules flagged onelevelflag - the ones a per-map
 * q2adminmaps/<mapname>.q2aspawn file contributed - leaving the
 * server-wide rules in place, so a map specific tweak doesn't outlive
 * the map it was written for. Closes the gap after each removal to keep
 * spawncmds[] dense, since everything else iterates 0..maxspawn_cmds.
 *
 * Takes no parameters; operates on the global spawncmds[]/maxspawn_cmds.
 * Returns nothing.
 *
 * Currently unreachable - nothing anywhere calls it. In practice per-map
 * rules are cleared anyway, because SpawnEntities() (g_init.c) calls
 * readSpawnLists() (which wipes the whole list via freeSpawnLists())
 * immediately before loading the new map's file, so the previous map's
 * entries never survive. That makes this a leftover of an approach that
 * cleared them separately.
 */
void freeOneLevelSpawnLists(void) {
    int spawn = 0;

    while (spawn < maxspawn_cmds) {
        if (spawncmds[spawn].onelevelflag) {
            G_Free(spawncmds[spawn].spawncmd);

            if (spawn + 1 < maxspawn_cmds) {
                q2a_memmove((spawncmds + spawn), (spawncmds + spawn + 1), sizeof (spawncmd_t) * (maxspawn_cmds - spawn));
            }

            maxspawn_cmds--;
        } else {
            spawn++;
        }
    }
}

/**
 * Rebuilds the entity-disable list from disk: drops whatever was loaded
 * before, then reads the file named by the q2a_spawnfile cvar from two
 * places - the server's working directory and moddir - merging both into
 * one list, so a server-wide default and a per-mod list can coexist
 * rather than one replacing the other. Both are loaded with
 * onelevelflag false, since these persist across maps.
 *
 * Only warns (LT_INTERNALWARN) if *neither* location had a readable
 * file, since having just one of the two is normal.
 *
 * Takes no parameters; fills the global spawncmds[]. Returns nothing -
 * a missing file isn't fatal, it just means nothing gets disabled.
 *
 * Called from SpawnEntities() (g_init.c) at the start of every level
 * load, before the per-map file is layered on top, and from
 * reloadSpawnFileRun() below.
 */
void readSpawnLists(void) {
    bool ret;

    freeSpawnLists();
    ret = readSpawnFile(configfile_spawn->string, false);
    Q_snprintf(buffer, sizeof(buffer), "%s/%s", moddir, configfile_spawn->string);
    if (readSpawnFile(buffer, false)) {
        ret = true;
    }
    if (!ret) {
        // gi.dprintf("WARNING: %s could not be found\n", configfile_spawn->string);
        logEvent(LT_INTERNALWARN, 0, NULL, va("%s could not be found", configfile_spawn->string), IW_SPAWNSETUPLOAD, 0.0, true);
    }
}

/**
 * "!reloadspawnfile" - re-reads the entity-disable list from disk, for
 * picking up edits without waiting for the next map change.
 *
 * Note it only refreshes the list itself. Entities are filtered as a map
 * loads, so nothing changes for the map already running - the reloaded
 * rules take effect on the next one.
 *
 * startarg: unused, this command takes no arguments.
 * ent:      who to confirm to.
 * client:   unused here.
 *
 * Returns nothing.
 *
 * Called via the "reloadspawnfile" entry in q2aCommands[] (g_cmd.c),
 * from an in-game admin console or rcon.
 */
void reloadSpawnFileRun(int startarg, edict_t *ent, int client) {
    readSpawnLists();
    gi.cprintf(ent, PRINT_HIGH, "Disbled entities reloaded.\n");
}

/**
 * Tests a classname against one disable rule, applying whichever of the
 * three match types that rule was loaded as: SW (prefix), EX (whole
 * string, case insensitive) or RE (regex).
 *
 * Note re_matchp() returns the offset the match was found at, so the
 * comparison against 0 means an RE rule only fires when the pattern
 * matches at the *start* of the classname - effectively anchored rather
 * than a search. Also worth knowing that re_compile() builds into a
 * single static buffer it hands a pointer into, so with more than one RE
 * rule loaded they all end up matching against whichever pattern
 * compiled last.
 *
 * cp:       the classname to test, already upper-cased by
 *           checkDisabledEntities() - which is what makes EX/RE case
 *           insensitive, since the stored patterns are upper-cased too.
 * spawncmd: index into spawncmds[] of the rule to test.
 *
 * Returns true if this one rule matches.
 *
 * Called from checkDisabledEntities() below, once per loaded rule.
 */
bool checkForSpawnCmd(char *cp, int spawncmd) {
    int len;
    switch (spawncmds[spawncmd].type) {
        case SPAWN_SW:
            return startContains(cp, spawncmds[spawncmd].spawncmd);
        case SPAWN_EX:
            return !Q_stricmp(cp, spawncmds[spawncmd].spawncmd);
        case SPAWN_RE:
            return (re_matchp(spawncmds[spawncmd].r, cp, &len) == 0);
    }
    return false;
}

/**
 * Decides whether an entity classname is one the admin has disabled, by
 * testing it against every loaded rule until one matches. This is the
 * question both filtering paths ask - the entity string rewrite at map
 * load and, when enabled, the live linkentity hook.
 *
 * Upper-cases the classname into the shared `buffer` first, which is
 * what makes matching case insensitive against the equally upper-cased
 * stored patterns. That means this clobbers `buffer`, so callers can't
 * be holding anything in it across the call.
 *
 * cp: the entity classname to check.
 *
 * Returns true if any rule matches, i.e. this entity should not spawn.
 * False if nothing matches or no rules are loaded.
 *
 * Called from SpawnEntities() (g_init.c) while walking the map's entity
 * string, and from linkentity_internal() below.
 */
bool checkDisabledEntities(char *cp) {
    unsigned int i;

    q2a_strncpy(buffer, cp, sizeof(buffer)-1);
    upperCase(buffer);
    for (i = 0; i < maxspawn_cmds; i++) {
        if (checkForSpawnCmd(buffer, i)) {
            return true;
        }
    }
    return false;
}

/**
 * "!listspawns" - shows the loaded entity-disable rules, so an admin can
 * see what's actually being stripped from maps.
 *
 * Prints the header itself and then queues QCMD_DISPSPAWN, which walks
 * the rules one per frame via displayNextSpawn() rather than dumping the
 * whole list into the client at once.
 *
 * startarg: unused, this command takes no arguments.
 * ent:      who to print the header to.
 * client:   whose command queue the listing is paced through.
 *
 * Returns nothing.
 *
 * Called via the "listspawns" entry in q2aCommands[] (g_cmd.c).
 */
void listspawnsRun(int startarg, edict_t *ent, int client) {
    addCmdQueue(client, QCMD_DISPSPAWN, 0, 0, 0);
    gi.cprintf(ent, PRINT_HIGH, "Start disbled-entities List:\n");
}

/**
 * Prints one disable rule - its 1-based number, type prefix and pattern,
 * in the same "SW:/EX:/RE:" form the spawn file uses, so what's shown
 * can be matched against (or pasted into) that file.
 *
 * Re-queues itself for the next index until the list runs out, then
 * prints the closing line. That pacing is why this is split out of
 * listspawnsRun(): one rule per queued-command tick keeps a long list
 * from flooding the client, the same approach displayNextBan() and
 * friends use.
 *
 * ent:      who to print to.
 * client:   their client index, used to queue the next tick.
 * spawncmd: 0-based index into spawncmds[] of the rule to print; past
 *           the end means stop and print the footer.
 *
 * Returns nothing.
 *
 * Called from G_RunFrame()'s QCMD_DISPSPAWN handling (g_main.c), first
 * queued by listspawnsRun() and then kept going by this function.
 */
void displayNextSpawn(edict_t *ent, int client, long spawncmd) {
    if (spawncmd < maxspawn_cmds) {
        switch (spawncmds[spawncmd].type) {
            case SPAWN_SW:
                gi.cprintf(ent, PRINT_HIGH, "%4ld SW:\"%s\"\n", spawncmd + 1, spawncmds[spawncmd].spawncmd);
                break;
            case SPAWN_EX:
                gi.cprintf(ent, PRINT_HIGH, "%4ld EX:\"%s\"\n", spawncmd + 1, spawncmds[spawncmd].spawncmd);
                break;
            case SPAWN_RE:
                gi.cprintf(ent, PRINT_HIGH, "%4ld RE:\"%s\"\n", spawncmd + 1, spawncmds[spawncmd].spawncmd);
                break;
        }
        spawncmd++;
        addCmdQueue(client, QCMD_DISPSPAWN, 0, spawncmd, 0);
    } else {
        gi.cprintf(ent, PRINT_HIGH, "End disbled-entities List\n");
    }
}

/**
 * "!spawncmd [SW/EX/RE] "classname"" - appends one rule to the
 * entity-disable list at runtime, for trying a rule out without editing
 * the spawn file and reloading.
 *
 * The addition is memory-only: the next readSpawnLists() (any map
 * change, or !reloadspawnfile) rebuilds from disk and drops it, so
 * anything meant to stick has to go in the file. And since entities are
 * filtered as a map loads, a rule added now first takes effect on the
 * next map.
 *
 * Rejects an unrecognised type keyword, a blank pattern, a list already
 * at SPAWN_MAXCMDS, and a regex that won't compile - freeing the string
 * it had already allocated in that last case. Note it does *not* repeat
 * readSpawnFile()'s worldspawn guard, so a rule added this way can match
 * worldspawn.
 *
 * startarg: index of the type keyword in the command's arguments; the
 *           classname pattern is expected at startarg + 1.
 * ent:      who to report success or the usage line to.
 * client:   unused here.
 *
 * Returns nothing; success or refusal is reported to ent.
 *
 * Called via the "spawncmd" entry in q2aCommands[] (g_cmd.c).
 */
void spawncmdRun(int startarg, edict_t *ent, int client) {
    char *cmd;
    int len;

    if (maxspawn_cmds >= SPAWN_MAXCMDS) {
        gi.cprintf(ent, PRINT_HIGH, "Sorry, maximum number of disbled-entitie commands has been reached.\n");
        return;
    }

    if (gi.argc() <= startarg + 1) {
        gi.cprintf(ent, PRINT_HIGH, SPAWNCMD);
        return;
    }

    cmd = gi.argv(startarg);
    if (Q_stricmp(cmd, "SW") == 0) {
        spawncmds[maxspawn_cmds].type = SPAWN_SW;
    } else if (Q_stricmp(cmd, "EX") == 0) {
        spawncmds[maxspawn_cmds].type = SPAWN_EX;
    } else if (Q_stricmp(cmd, "RE") == 0) {
        spawncmds[maxspawn_cmds].type = SPAWN_RE;
    } else {
        gi.cprintf(ent, PRINT_HIGH, SPAWNCMD);
        return;
    }

    spawncmds[maxspawn_cmds].onelevelflag = false;
    cmd = gi.argv(startarg + 1);

    if (isBlank(cmd)) {
        gi.cprintf(ent, PRINT_HIGH, SPAWNCMD);
        return;
    }

    len = q2a_strlen(cmd) + 20;

    spawncmds[maxspawn_cmds].spawncmd = G_Malloc(len);
    processString(spawncmds[maxspawn_cmds].spawncmd, cmd, len - 1, 0);

    if (spawncmds[maxspawn_cmds].type == SPAWN_RE) {
        upperCase(cmd);
        spawncmds[maxspawn_cmds].r = re_compile(cmd);
        if (!spawncmds[maxspawn_cmds].r) {
            G_Free(spawncmds[maxspawn_cmds].spawncmd);

            // malformed re...
            gi.cprintf(ent, PRINT_HIGH, "Regular expression couldn't compile!\n");
            return;
        }
    } else {
        spawncmds[maxspawn_cmds].r = 0;
    }

    switch (spawncmds[maxspawn_cmds].type) {
        case SPAWN_SW:
            gi.cprintf(ent, PRINT_HIGH, "%4d SW:\"%s\" added\n", maxspawn_cmds + 1, spawncmds[maxspawn_cmds].spawncmd);
            break;
        case SPAWN_EX:
            gi.cprintf(ent, PRINT_HIGH, "%4d EX:\"%s\" added\n", maxspawn_cmds + 1, spawncmds[maxspawn_cmds].spawncmd);
            break;
        case SPAWN_RE:
            gi.cprintf(ent, PRINT_HIGH, "%4d RE:\"%s\" added\n", maxspawn_cmds + 1, spawncmds[maxspawn_cmds].spawncmd);
            break;
    }
    maxspawn_cmds++;
}

/**
 * "!spawndel <spawnnum>" - removes one rule from the entity-disable list
 * by the number !listspawns shows, the counterpart to !spawncmd for
 * withdrawing something at runtime.
 *
 * Frees that rule's string and shifts the remaining rules down to close
 * the gap, which keeps spawncmds[] dense at the cost of renumbering
 * everything after it - so numbers from an earlier !listspawns go stale.
 *
 * Like !spawncmd this only edits the in-memory list; the next
 * readSpawnLists() restores whatever the file says.
 *
 * startarg: index of the rule number in the command's arguments.
 * ent:      who to report success or the usage line to.
 * client:   unused here.
 *
 * Returns nothing; success or refusal is reported to ent.
 *
 * Called via the "spawndel" entry in q2aCommands[] (g_cmd.c).
 */
void spawnDelRun(int startarg, edict_t *ent, int client) {
    int spawn;

    if (gi.argc() <= startarg) {
        gi.cprintf(ent, PRINT_HIGH, SPAWNDELCMD);
        return;
    }

    spawn = q2a_atoi(gi.argv(startarg));

    if (spawn < 1 || spawn > maxspawn_cmds) {
        gi.cprintf(ent, PRINT_HIGH, SPAWNDELCMD);
        return;
    }

    spawn--;

    G_Free(spawncmds[spawn].spawncmd);

    if (spawn + 1 < maxspawn_cmds) {
        q2a_memmove((spawncmds + spawn), (spawncmds + spawn + 1), sizeof (spawncmd_t) * (maxspawn_cmds - spawn));
    }

    maxspawn_cmds--;
    gi.cprintf(ent, PRINT_HIGH, "Disbled-entities command deleted\n");
}

/**
 * q2admin's interception of the engine's linkentity - the call a game
 * mod makes to put an entity into the world. Like Pmove_internal(), this
 * runs in the mod-to-engine direction: the mod was handed a
 * game_import_t built by q2admin, and this is installed as that table's
 * linkentity slot in GetGameAPI() (g_main.c), so every entity the mod
 * tries to place passes through here first.
 *
 * That makes it a second, later chance to suppress a disabled entity.
 * The main path rewrites classnames in the map's entity string before
 * the mod ever sees them (SpawnEntities(), g_init.c); this one catches
 * entities the mod creates itself at runtime, which that pass can't
 * see. When a classname matches a disable rule the entity is dropped
 * rather than linked - classname pointer nulled and inuse cleared - so
 * it never enters the world. Otherwise it's logged and passed straight
 * through to the real engine.
 *
 * The awkward part is finding the classname at all. It lives in the
 * mod's private extension of edict_t, past the shared prefix q2admin can
 * see, so there's no portable way to reach it - instead it's read from a
 * raw byte offset that has to be configured per mod via
 * entity_classname_offset. A wrong offset means dereferencing whatever
 * happens to sit there, which is why this whole path is off by default
 * behind spawnentities_internal_enable and why the config warns it can
 * crash the server.
 *
 * ent: the entity the mod is trying to link.
 *
 * Returns nothing.
 *
 * Called by the wrapped game mod itself, via the import table, whenever
 * it links an entity.
 */
void linkentity_internal(edict_t *ent) {
    if (spawnentities_internal_enable && spawnentities_enable) {
        if (checkDisabledEntities(*((char **) ((unsigned long) ent + entity_classname_offset)))) {
            char **classnameptr = ((char **) ((unsigned long) ent + entity_classname_offset));
            *classnameptr = NULL;
            ent->inuse = false;
            return;
        }
    }
    logEvent(LT_ENTITYCREATE, 0, NULL, *((char **) ((unsigned long) ent + entity_classname_offset)), 0, 0.0, false);
    gi.linkentity(ent);
}

/**
 * q2admin's interception of the engine's unlinkentity, the counterpart
 * to linkentity_internal() above and likewise installed into the import
 * table handed to the wrapped mod (GetGameAPI(), g_main.c), so it's
 * called by the mod rather than the engine.
 *
 * Unlike the link side this filters nothing - an entity being removed
 * needs no policy applied. It exists purely to log the removal
 * (LT_ENTITYDELETE) before forwarding, giving the entity create/delete
 * log a matching pair of events for tracing what a mod is doing with
 * entities.
 *
 * It reads the classname through the same configured
 * entity_classname_offset hack described above, with the same risk if
 * that offset is wrong for the mod in use.
 *
 * ent: the entity the mod is removing from the world.
 *
 * Returns nothing.
 *
 * Called by the wrapped game mod itself, via the import table, whenever
 * it unlinks an entity.
 */
void unlinkentity_internal(edict_t *ent) {
    logEvent(LT_ENTITYDELETE, 0, NULL, *((char **) ((unsigned long) ent + entity_classname_offset)), 0, 0.0, false);
    gi.unlinkentity(ent);
}
