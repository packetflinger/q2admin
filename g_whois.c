/**
 * Q2Admin
 * whois tracking functions
 */

#include "g_local.h"

user_details_t *whois_details;
int WHOIS_COUNT = 0;
int whois_active = 0;

/**
 * The "whois <name|id>" player command - looks up the alias history
 * q2admin has been quietly accumulating for a player, so anyone can see
 * what other names a given player has connected under. Records are keyed
 * by IP (see whoisGetID()), which is what lets a player who renames, or
 * who left and came back, still be tied back to their earlier names.
 *
 * Resolves the argument in three passes, stopping at the first hit:
 *   1. as a client slot number, if it parses as one in range (so
 *      "whois 3" works straight off the !players / adm_players list),
 *   2. as an exact name match against currently connected players
 *      (despite the inline comment below claiming partial matching, this
 *      is a plain strcmp),
 *   3. as an exact match against any of the 10 remembered aliases of
 *      every stored record - this pass is what finds players who aren't
 *      connected right now.
 *
 * A target carrying ADMIN_LEVEL7 is refused at passes 1 and 2 ("Unable
 * to fetch info"). That's the one and only thing that level bit does
 * anywhere in q2admin (it's marked "???" in g_admin.h): it's not a
 * command grant, it's a privacy flag that keeps an admin's own identity
 * history from being looked up. Note it's only checked against connected
 * players, so pass 3 can still surface those names from the stored list.
 *
 * client: the invoking player's index. Not used for the lookup itself -
 *         it's passed to whoisDumpDetails(), which decides from their
 *         admin level whether IPs are included in the output.
 * ent:    who to print the results to.
 *
 * Called from doClientCommand() (g_cmd.c) when a player types "whois",
 * gated on whois_active. Unlike the "!" commands this needs no admin
 * level - any player can run it.
 */
void whois(int client, edict_t *ent) {
    char a1[256];
    unsigned int i;
    int temp;

    if (gi.argc() < 2) {
        gi.cprintf(ent, PRINT_HIGH, "\nIncorrect syntax, use: 'whois <name>' or 'whois <id>'\n");
        adm_players(ent, client);
        return;
    }

    q2a_strncpy(a1, gi.argv(1), sizeof(a1)-1);
    a1[sizeof(a1)-1] = 0;

    temp = q2a_atoi(a1);
    if ((temp == 0) && (strcmp(a1, "0"))) {
        temp = -1;
    }

    //do numbers first
    if ((temp < maxclients->value) && (temp >= 0)) {
        if ((proxyinfo[temp].inuse) && (proxyinfo[temp].userid >= 0)) {
            //got match, dump details except if admin has proper flag
            if (proxyinfo[temp].admin_level & ADMIN_LEVEL7) {
                gi.cprintf(ent, PRINT_HIGH, "  Unable to fetch info for %i\n", temp);
                return;
            }
            gi.cprintf(ent, PRINT_HIGH, "\n  Whois details for client %i\n", temp);
            whoisDumpDetails(client, ent, proxyinfo[temp].userid);
            return;
        }

    }
    //then process all connected clients
    for (i = 0; i < maxclients->value; i++) {
        if ((proxyinfo[i].inuse) && (proxyinfo[i].userid >= 0)) {
            //only do partial match on these, dump all that apply
            if (q2a_strcmp(proxyinfo[i].name, a1) == 0) {
                if (proxyinfo[i].admin_level & ADMIN_LEVEL7) {
                    gi.cprintf(ent, PRINT_HIGH, "  Unable to fetch info for %s\n", a1);
                    return;
                }
                gi.cprintf(ent, PRINT_HIGH, "\n  Whois details for %s\n", proxyinfo[i].name);
                whoisDumpDetails(client, ent, proxyinfo[i].userid);
                //got match, dump details
                return;
            }
        }
    }
    //then if still no match process our stored list
    for (i = 0; i < WHOIS_COUNT; i++) {
        if ((whois_details[i].dyn[0].name[0]) || (whois_details[i].dyn[1].name[0]) || (whois_details[i].dyn[2].name[0]) ||
                (whois_details[i].dyn[3].name[0]) || (whois_details[i].dyn[4].name[0]) || (whois_details[i].dyn[5].name[0]) ||
                (whois_details[i].dyn[6].name[0]) || (whois_details[i].dyn[7].name[0]) || (whois_details[i].dyn[8].name[0]) ||
                (whois_details[i].dyn[9].name[0])) {
            //r1ch: wtf?
            if (((q2a_strcmp(whois_details[i].dyn[0].name, a1) == 0)) ||
                    ((q2a_strcmp(whois_details[i].dyn[1].name, a1) == 0)) ||
                    ((q2a_strcmp(whois_details[i].dyn[2].name, a1) == 0)) ||
                    ((q2a_strcmp(whois_details[i].dyn[3].name, a1) == 0)) ||
                    ((q2a_strcmp(whois_details[i].dyn[4].name, a1) == 0)) ||
                    ((q2a_strcmp(whois_details[i].dyn[5].name, a1) == 0)) ||
                    ((q2a_strcmp(whois_details[i].dyn[6].name, a1) == 0)) ||
                    ((q2a_strcmp(whois_details[i].dyn[7].name, a1) == 0)) ||
                    ((q2a_strcmp(whois_details[i].dyn[8].name, a1) == 0)) ||
                    ((q2a_strcmp(whois_details[i].dyn[9].name, a1) == 0))) {
                gi.cprintf(ent, PRINT_HIGH, "\n  Whois details for %s\n", a1);
                whoisDumpDetails(client, ent, i);
                //got a match, dump details
                return;
            }

        }
    }
    gi.cprintf(ent, PRINT_HIGH, "  No entry found for %s\n", a1);
}

/**
 * Prints one stored whois record: its remembered aliases (up to 10,
 * numbered, skipping empty slots) followed by when that record was last
 * seen connecting.
 *
 * The record's IP is appended to each line only if the *requesting*
 * player has any admin level at all - that's the privacy split here.
 * Ordinary players can see that a set of names belong to one person,
 * which is the point of the feature, without being handed the address
 * tying those names to a real machine; admins get the address too, since
 * that's what they'd need to act on it (bans are IP-based).
 *
 * client: the requesting player's index, used only for that admin_level
 *         check - not the player being looked up.
 * ent:    who to print to.
 * userid: index into whois_details of the record to print.
 *
 * Called from whois() above, from each of its three lookup passes.
 */
void whoisDumpDetails(int client, edict_t *ent, int userid) {
    unsigned int i;
    for (i = 0; i < 10; i++) {
        if (whois_details[userid].dyn[i].name[0]) {
            if (!proxyinfo[client].admin_level) {
                gi.cprintf(ent, PRINT_HIGH, "    %02i. %s\n", i + 1, whois_details[userid].dyn[i].name);
            } else {
                gi.cprintf(ent, PRINT_HIGH, "    %02i. %s %s\n", i + 1, whois_details[userid].dyn[i].name, whois_details[userid].ip);
            }
        }
    }
    gi.cprintf(ent, PRINT_HIGH, "  Last seen: %s\n\n", whois_details[userid].seen);
}

/**
 * Starts a brand new whois record for a client who didn't match an
 * existing one: stores their IP as the record's key, seeds the first
 * alias slot with the name they're using now, and points
 * proxyinfo[client].userid at it so later renames (whoisNewName()) and
 * lookups know which record is theirs.
 *
 * Once the table is full (WHOIS_COUNT >= whois_active, the cfg-set size
 * whois_details was allocated with) this steps WHOIS_COUNT back one and
 * writes over that slot, so it's always the most recently added record
 * that gets clobbered - older history is kept indefinitely and never
 * ages out, and the table effectively stops growing rather than cycling.
 * Raising whois_active is the only way to keep more records.
 *
 * client: the client index to build the record from (its IP and current
 *         name) and to assign the resulting userid to.
 * ent:    unused here.
 *
 * Called from whoisGetID() below when no stored record matches the
 * connecting IP, and from checkForNameChange() (g_init.c) for a client
 * who renames while still having no record.
 */
void whoisAddUser(int client, edict_t *ent) {
    if (WHOIS_COUNT >= whois_active) {
        WHOIS_COUNT = WHOIS_COUNT - 1; //If max reached, replace latest entry with new client
    }
    whois_details[WHOIS_COUNT].id = WHOIS_COUNT;
    q2a_strncpy(whois_details[WHOIS_COUNT].ip, IP(client), WHOISIPLEN-1);
    q2a_strncpy(whois_details[WHOIS_COUNT].dyn[0].name, proxyinfo[client].name, WHOISNAMELEN-1);
    proxyinfo[client].userid = WHOIS_COUNT;
    WHOIS_COUNT++;
}

/**
 * Files the client's current name into their whois record's alias list -
 * the actual act of "remembering" a name, and so the thing that builds
 * up the history whois() later reports.
 *
 * Fills the first empty one of the 10 slots, and bails out early if the
 * name is already recorded, so repeatedly reconnecting under the same
 * name doesn't consume the whole list. Once all 10 are full it shifts
 * every entry down one - dropping the oldest, slot 0 - and appends the
 * new name at slot 9, making the alias list a 10-deep FIFO of the most
 * recent distinct names.
 *
 * A client with no record yet (userid == -1) is handed to whoisGetID()
 * to get one; that in turn calls back here once it has, which is safe
 * because it only does so after a userid has been assigned.
 *
 * client: the client whose current proxyinfo name is being recorded.
 * ent:    only passed through to whoisGetID().
 *
 * Called from checkForNameChange() (g_init.c) whenever a client renames
 * mid-game, and from whoisGetID() when a returning player's record is
 * matched, to catch the name they came back under.
 */
void whoisNewName(int client, edict_t *ent) {
    //called when a client changes name
    unsigned int i;

    if (proxyinfo[client].userid == -1) {
        whoisGetID(client, ent);
        return;
    } else {
        for (i = 0; i < 10; i++) {
            if (!whois_details[proxyinfo[client].userid].dyn[i].name[0]) {
                //this is empty, so add here
                q2a_strcpy(whois_details[proxyinfo[client].userid].dyn[i].name, proxyinfo[client].name);
                return;
            }
            if (q2a_strcmp(whois_details[proxyinfo[client].userid].dyn[i].name, proxyinfo[client].name) == 0) {
                //the name already exists, return
                return;
            }
        }
    }
    //if we got here we have a new name but no free slots, so remove 1 for insertion
    for (i = 0; i < 9; i++) {
        q2a_strcpy(whois_details[proxyinfo[client].userid].dyn[i].name, whois_details[proxyinfo[client].userid].dyn[i + 1].name);
    }
    q2a_strcpy(whois_details[proxyinfo[client].userid].dyn[9].name, proxyinfo[client].name);
}

/**
 * Ties a connecting client to their whois record, creating one if this
 * is an address that's never been seen. Scans the stored records for one
 * whose IP matches theirs and, on a hit, adopts that record's index as
 * proxyinfo[client].userid and records whatever name they've turned up
 * under this time (whoisNewName()); otherwise hands off to
 * whoisAddUser() to start a fresh record.
 *
 * The IP is the identity here - that's what makes the whole feature
 * work, since it's the one thing a player can't trivially change between
 * sessions the way they can a name, so it's what lets a rename or a
 * reconnect still be linked to the same alias history.
 *
 * client: the connecting client, whose IP is matched and whose userid is
 *         set.
 * ent:    only passed through to whoisNewName()/whoisAddUser().
 *
 * Called from ClientConnect() (g_init.c) when whois_active is set, and
 * from whoisNewName() for a client that somehow has no record yet.
 */
void whoisGetID(int client, edict_t *ent) {
    //called when a client connects
    unsigned int i;
    for (i = 0; i < WHOIS_COUNT; i++) {
        if (q2a_strcmp(whois_details[i].ip, IP(client)) == 0) {
            //got a match, store new id
            proxyinfo[client].userid = i;
            whoisNewName(client, ent);
            return;
        }
    }
    whoisAddUser(client, ent);
}

/**
 * Stamps a client's whois record with the current wall clock time, so
 * whoisDumpDetails() can report how long ago that identity was last
 * around - useful context when an admin is looking at a name and trying
 * to work out whether it's a regular or someone who turned up once. Uses
 * ctime() and trims the newline it tacks on, leaving a human-readable
 * string that gets written to whois.dat verbatim.
 *
 * Does nothing for a client with no record yet (userid < 0), so it's
 * safe to call before whoisGetID() has assigned one.
 *
 * client: whose record to stamp.
 * ent:    unused here.
 *
 * Called from ClientConnect() (g_init.c), right after whoisGetID().
 * Note the inline comment below says connect *and* disconnect, but only
 * the connect half is actually wired up - so "last seen" really means
 * "last connected", not when they left.
 */
void whoisUpdateSeen(int client, edict_t *ent) {
    //to be called on client connect and disconnect
    time_t ltimetemp;
    time(&ltimetemp);
    if (proxyinfo[client].userid >= 0) {
        q2a_strcpy(whois_details[proxyinfo[client].userid].seen, ctime(&ltimetemp));
        whois_details[proxyinfo[client].userid].seen[strlen(whois_details[proxyinfo[client].userid].seen) - 1] = 0;
    }
}

/**
 * Serializes the whole whois table to moddir/whois.dat so the alias
 * history survives a server restart - without this the table would only
 * ever be as good as the current uptime, which would defeat the point of
 * tracking identities over time.
 *
 * One record per line, whitespace separated: id, ip, last-seen, then all
 * 10 alias slots. Because whoisReadFile() parses that back with
 * fscanf("%s"), any embedded space would split one field into two and
 * knock the whole line's columns out of alignment - so every space is
 * written as '?' and turned back on read. The last-seen field is the
 * reason this matters at all: ctime() strings are full of spaces. Empty
 * alias slots are written as a lone '?' so all 10 columns are always
 * present and positional.
 *
 * That encoding is lossy in one direction: a name that genuinely
 * contains '?' comes back with spaces in place of them.
 *
 * Records with no IP are skipped, since the IP is the record's key and
 * one without it could never be matched again anyway.
 *
 * Takes no parameters; writes the global whois_details table.
 *
 * Called from ShutdownGame() (g_main.c) so the table is flushed on the
 * way down, and from the ADMIN_LEVEL8 "!writewhois" command in
 * doAdminCommand() (g_admin.c) to checkpoint it on demand without
 * restarting.
 */
void whoisWriteFile(void) {
    //file format...?
    //id ip seen names
    //when do we want to write this file? it might be processor hungry
    //maybe create a timer that is checked on each spawnentities
    //if 1 day has elapsed then write file
    FILE *f;
    char name[256];
    char temp[256];
    int temp_len;
    unsigned int i, j, k;

    Q_snprintf(name, sizeof(name), "%s/%s", moddir, WHOISFILE);

    f = fopen(name, "wb");
    if (!f) {
        return;
    }

    for (i = 0; i < WHOIS_COUNT; i++) {
        if (whois_details[i].ip[0] == 0) {
            continue;
        }

        q2a_strncpy(temp, whois_details[i].ip, sizeof(temp)-1);
        temp_len = strlen(temp);

        //convert spaces to �
        for (j = 0; j < temp_len; j++) {
            if (temp[j] == ' ') {
                temp[j] = '?';
            }
        }
        fprintf(f, "%i %s ", whois_details[i].id, temp);

        q2a_strncpy(temp, whois_details[i].seen, sizeof(temp)-1);
        temp_len = strlen(temp);

        for (j = 0; j < temp_len; j++) {
            if (temp[j] == ' ') {
                temp[j] = '?';
            }
        }
        fprintf(f, "%s ", temp);

        for (j = 0; j < 10; j++) {
            if (whois_details[i].dyn[j].name[0]) {
                q2a_strncpy(temp, whois_details[i].dyn[j].name, sizeof(temp)-1);
                temp_len = strlen(temp);

                for (k = 0; k < temp_len; k++) {
                    if (temp[k] == ' ') {
                        temp[k] = '?';
                    }
                }
                fprintf(f, "%s ", temp);
            } else {
                fprintf(f, "? ");
            }
        }
        fprintf(f, "\n");
    }
    fclose(f);
}

/**
 * Loads moddir/whois.dat back into the table, reversing what
 * whoisWriteFile() encoded: '?' characters become spaces again, and an
 * alias slot that's just the placeholder becomes an empty slot rather
 * than a literal "?" name.
 *
 * The placeholder test also accepts a leading byte of 255/-1 (the same
 * value either way, depending on whether char is signed on this
 * platform) - that's an older whois.dat format that used a raw 0xFF byte
 * where '?' is used now, so pre-existing files still load.
 *
 * Reads at most whois_active records, which is exactly how many
 * whois_details was allocated for in InitGame(), so a file grown larger
 * than the current setting is truncated rather than overrunning the
 * table. WHOIS_COUNT is reset first, so this replaces the in-memory
 * table outright rather than merging into it.
 *
 * Takes no parameters; fills the global whois_details table.
 *
 * Called from InitGame() (g_init.c) right after the table is allocated,
 * and from whoisReloadFileRun() below.
 */
void whoisReadFile(void) {
    FILE *f;
    char name[256];
    unsigned int i, j;
    int temp_len, name_len;

    Q_snprintf(name, sizeof(name), "%s/%s", moddir, WHOISFILE);
    q2a_printf("reading whois file: %s\n", name);
    f = fopen(name, "rb");
    if (!f) {
        q2a_printf("WARNING: %s could not be found\n", name);
        return;
    }

    WHOIS_COUNT = 0;
    while ((!feof(f)) && (WHOIS_COUNT < whois_active)) {
        fscanf(f, "%i %s %s %s %s %s %s %s %s %s %s %s %s",
                &whois_details[WHOIS_COUNT].id,
                whois_details[WHOIS_COUNT].ip,
                whois_details[WHOIS_COUNT].seen,
                whois_details[WHOIS_COUNT].dyn[0].name,
                whois_details[WHOIS_COUNT].dyn[1].name,
                whois_details[WHOIS_COUNT].dyn[2].name,
                whois_details[WHOIS_COUNT].dyn[3].name,
                whois_details[WHOIS_COUNT].dyn[4].name,
                whois_details[WHOIS_COUNT].dyn[5].name,
                whois_details[WHOIS_COUNT].dyn[6].name,
                whois_details[WHOIS_COUNT].dyn[7].name,
                whois_details[WHOIS_COUNT].dyn[8].name,
                whois_details[WHOIS_COUNT].dyn[9].name);

        //convert all � back to spaces
        temp_len = strlen(whois_details[WHOIS_COUNT].ip);
        for (i = 0; i < temp_len; i++) {
            if (whois_details[WHOIS_COUNT].ip[i] == '?') {
                whois_details[WHOIS_COUNT].ip[i] = ' ';
            }
        }

        temp_len = strlen(whois_details[WHOIS_COUNT].seen);
        for (i = 0; i < temp_len; i++) {
            if (whois_details[WHOIS_COUNT].seen[i] == '?') {
                whois_details[WHOIS_COUNT].seen[i] = ' ';
            }
        }

        for (i = 0; i < 10; i++) {
            if ((whois_details[WHOIS_COUNT].dyn[i].name[0] == 255)
                    || (whois_details[WHOIS_COUNT].dyn[i].name[0] == -1)
                    || (whois_details[WHOIS_COUNT].dyn[i].name[0] == '?')) {
                whois_details[WHOIS_COUNT].dyn[i].name[0] = 0;
            } else {
                name_len = strlen(whois_details[WHOIS_COUNT].dyn[i].name);
                for (j = 0; j < name_len; j++) {
                    if (whois_details[WHOIS_COUNT].dyn[i].name[j] == '?') {
                        whois_details[WHOIS_COUNT].dyn[i].name[j] = ' ';
                    }
                }
            }
        }
        WHOIS_COUNT++;
    }
    fclose(f);
}

/**
 * "!reloadwhoisfile" - re-reads whois.dat from disk, for picking up a
 * file that was edited or replaced outside the server without having to
 * restart it.
 *
 * Two things worth knowing before using it: whoisReadFile() replaces
 * the in-memory table outright, so any names recorded since the last
 * whoisWriteFile() are discarded; and connected players keep the
 * proxyinfo[].userid they were already assigned, which after a reload
 * may index a different record than it did before.
 *
 * startarg: unused, this command takes no arguments.
 * ent:      who to confirm to.
 * client:   unused here.
 *
 * Called via the "reloadwhoisfile" entry in q2aCommands[] (g_cmd.c),
 * from an in-game admin console or rcon.
 */
void whoisReloadFileRun(int startarg, edict_t *ent, int client) {
    whoisReadFile();
    gi.cprintf(ent, PRINT_HIGH, "whois file reloaded.\n");
}
