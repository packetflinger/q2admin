/**
 * Q2Admin
 * ref/admin related stuff
 */

#include "g_local.h"

admin_t admin_pass[MAX_ADMINS];
admin_t bypass_pass[MAX_ADMINS];
int num_admins = 0;
int num_bypasses = 0;

/**
 * Loads the two separate credential tables q2admin grants elevated
 * access from: admin_pass[] (q2a_loginfile, default q2a_login.cfg) and
 * bypass_pass[] (q2a_bypassfile, default q2a_bypass.cfg). They're kept
 * as two distinct files/tables because they grant two different things -
 * admin_pass entries carry an ADMIN_LEVEL1-9 bitmask that gates which
 * in-game moderation commands a player can use once authenticated (see
 * listAdminCommands()), while bypass_pass entries (per BYPASSFILE's own
 * "anticheat requirement bypass" comment) exempt a trusted player from
 * some of q2admin's proxy/bot detection probes rather than granting any
 * moderation power - conflating the two would mean either every admin
 * has to also be anticheat-exempt or vice versa.
 *
 * Each file is just whitespace-separated "name password level" lines,
 * read up to MAX_ADMINS entries or EOF. Any table slots beyond what was
 * actually read are explicitly zeroed out (level = 0) rather than left
 * as-is, since this isn't only called once at startup - it can be
 * re-read later via reloadLoginFileRun() without a restart, and without
 * this a user removed from the file would otherwise keep their old
 * level from a previous load.
 *
 * Takes no parameters; reads moddir plus the two cvars above to find the
 * files.
 *
 * Called from InitGame() (g_init.c) at startup, and from
 * reloadLoginFileRun() (below) so an admin can reload the credential
 * files live via console/rcon without restarting the server.
 */
void readAdminConfig(void) {
    FILE *f;
    char name[256];
    int i, i2;

    Q_snprintf(name, sizeof(name), "%s/%s", moddir, configfile_login->string);

    f = fopen(name, "rb");
    if (!f) {
        gi.dprintf("WARNING: %s could not be found\n", name);
        goto file2;
        return;
    }

    i = 0;
    while ((!feof(f)) && (i < MAX_ADMINS)) {
        fscanf(f, "%s %s %d", admin_pass[i].name, admin_pass[i].password, &admin_pass[i].level);
        if (admin_pass[i].level > 0) {
            i++;
        }
    }
    num_admins = i;
    if (i < MAX_ADMINS) {
        for (i2 = i; i2 < MAX_ADMINS; i2++) {
            admin_pass[i2].level = 0;
        }
    }
    gi.cprintf(NULL, PRINT_HIGH, "%d admin users loaded\n", i);
    fclose(f);

file2:
    ;
    Q_snprintf(name, sizeof(name), "%s/%s", moddir, configfile_bypass->string);

    f = fopen(name, "rb");
    if (!f) {
        gi.dprintf("WARNING: %s could not be found\n", name);
        return;
    }

    i = 0;
    while ((!feof(f)) && (i < MAX_ADMINS)) {
        fscanf(f, "%s %s %d", bypass_pass[i].name, bypass_pass[i].password, &bypass_pass[i].level);
        if (bypass_pass[i].level > 0) {
            i++;
        }
    }
    num_bypasses = i;
    if (i < MAX_ADMINS) {
        for (i2 = i; i2 < MAX_ADMINS; i2++) {
            bypass_pass[i2].level = 0;
        }
    }
    gi.cprintf(NULL, PRINT_HIGH, "%d bypass users loaded\n", i);
    fclose(f);
}

/**
 * Prints the specific in-game admin commands a player has just unlocked.
 * ADMIN_LEVEL1-9 (g_admin.h) is a bitmask, and a player's granted level
 * (proxyinfo[client].admin_level, set from the entry getAdminLevel()
 * matched in admin_pass[]) can be any combination of those bits -
 * without this, a freshly authenticated admin would have no way to know
 * which of the level-gated commands they actually have access to. Each
 * command is only printed if its level bit is set, and !writewhois
 * (ADMIN_LEVEL8) is additionally gated on whois_active, since that
 * command is meaningless if the whois tracking feature itself is off.
 * ADMIN_LEVEL7 and ADMIN_LEVEL9 currently have no command mapped here to
 * list (see the "???" against them in g_admin.h).
 *
 * ent:    who to print the list to.
 * client: their client index, used to read proxyinfo[client].admin_level.
 *
 * Called from doClientCommand()'s "!admin" handling (g_cmd.c),
 * immediately after a successful admin login, to show the player what
 * they just gained access to.
 */
void listAdminCommands(edict_t *ent, int client) {
    if (proxyinfo[client].admin_level & ADMIN_LEVEL1) {
        gi.cprintf(ent, PRINT_HIGH, "    - !boot <number>\n");
    }
    if (proxyinfo[client].admin_level & ADMIN_LEVEL2) {
        gi.cprintf(ent, PRINT_HIGH, "    - !dumpmsec\n");
    }
    if (proxyinfo[client].admin_level & ADMIN_LEVEL3) {
        gi.cprintf(ent, PRINT_HIGH, "    - !changemap <mapname>\n");
    }
    if (proxyinfo[client].admin_level & ADMIN_LEVEL4) {
        gi.cprintf(ent, PRINT_HIGH, "    - !dumpuser <num>\n");
    }
    if (proxyinfo[client].admin_level & ADMIN_LEVEL5) {
        gi.cprintf(ent, PRINT_HIGH, "    - !auth\n");
        gi.cprintf(ent, PRINT_HIGH, "    - !gfx\n");
    }
    if (proxyinfo[client].admin_level & ADMIN_LEVEL6) {
        gi.cprintf(ent, PRINT_HIGH, "    - !dostuff <num> <commands>\n");
    }
    if (proxyinfo[client].admin_level & ADMIN_LEVEL8) {
        if (whois_active) {
            gi.cprintf(ent, PRINT_HIGH, "    - !writewhois\n");
        }
    }
    gi.cprintf(ent, PRINT_HIGH, "\n");
}

/**
 * Reload the admin/bypass users from disk.
 */
void reloadLoginFileRun(int startarg, edict_t *ent, int client) {
    readAdminConfig();
    gi.cprintf(ent, PRINT_HIGH, "Login file reloaded.\n");
}

/**
 * Checks a name+password pair a player typed via "!bypass <name>
 * <password>" against the bypass_pass[] table loaded by
 * readAdminConfig(), and returns the level to grant if it matches - the
 * credential check behind bypass_pass entries' "anticheat requirement
 * bypass" purpose (see BYPASSFILE), letting a trusted player be exempted
 * from some of q2admin's proxy/bot detection probes. Mirrors
 * getAdminLevel() below, just for the bypass table instead of the admin
 * one.
 *
 * Stops at the first bypass_pass[i].level == 0 entry as a defensive
 * check, though that shouldn't actually trigger in normal operation -
 * readAdminConfig() only ever fills bypass_pass[0..num_bypasses) with
 * entries that already have level > 0.
 *
 * givenpass: the password the player typed. Note the parameter order -
 *            password first, then name - matches how the caller passes
 *            gi.argv(2)/gi.argv(1) (the command is "!bypass name pass"),
 *            so don't swap them when calling this.
 * givenname: the name the player typed.
 *
 * Returns the matched entry's level (> 0) on a match, 0 if nothing in
 * bypass_pass[] matches both the name and password exactly.
 *
 * Called from doClientCommand()'s "!bypass" handling (g_cmd.c) to decide
 * what to set proxyinfo[client].bypass_level to.
 */
int getBypassLevel(char *givenpass, char *givenname) {
    int got_level = 0;
    unsigned int i;

    for (i = 0; i < num_bypasses; i++) {
        if (!bypass_pass[i].level)
            break;
        if ((strcmp(givenpass, bypass_pass[i].password) == 0) && (strcmp(givenname, bypass_pass[i].name) == 0)) {
            got_level = bypass_pass[i].level;
            break;
        }
    }
    return got_level;
}

/**
 * Checks a name+password pair a player typed via "!admin <name>
 * <password>" against the admin_pass[] table loaded by
 * readAdminConfig(), and returns the ADMIN_LEVEL1-9 bitmask to grant if
 * it matches - the credential check behind in-game admin access.
 * Mirrors getBypassLevel() above, just against the admin table instead
 * of the bypass one.
 *
 * Stops at the first admin_pass[i].level == 0 entry as a defensive
 * check, though that shouldn't actually trigger in normal operation -
 * readAdminConfig() only ever fills admin_pass[0..num_admins) with
 * entries that already have level > 0.
 *
 * givenpass: the password the player typed. Note the parameter order -
 *            password first, then name - matches how the caller passes
 *            gi.argv(2)/gi.argv(1) (the command is "!admin name pass"),
 *            so don't swap them when calling this.
 * givenname: the name the player typed.
 *
 * Returns the matched entry's level bitmask (> 0) on a match, 0 if
 * nothing in admin_pass[] matches both the name and password exactly.
 *
 * Called from doClientCommand()'s "!admin" handling (g_cmd.c) to decide
 * what to set proxyinfo[client].admin_level to before calling
 * listAdminCommands() to show the player what they just gained access to.
 */
int getAdminLevel(char *givenpass, char *givenname) {
    int got_level = 0;

    for (unsigned int i = 0; i < num_admins; i++) {
        if (!admin_pass[i].level)
            break;
        if ((strcmp(givenpass, admin_pass[i].password) == 0) && (strcmp(givenname, admin_pass[i].name) == 0)) {
            got_level = admin_pass[i].level;
            break;
        }
    }
    return got_level;
}

/**
 * List out the players and their index. This probably shouldn't be an admin
 * command, most mods provide a command like this, it's not providing any
 * privileged information.
 */
void adm_players(edict_t *ent, int client) {
    gi.cprintf(ent, PRINT_HIGH, "Player List\n");
    for (int i = 0; i < maxclients->value; i++) {
        if (proxyinfo[i].inuse) {
            gi.cprintf(ent, PRINT_HIGH, "  %2i : %s\n", i, NAME(i));
        }
    }
}

/**
 * !dumpmsec - prints every currently-active client's msec total from
 * their last completed msec-tracking window (proxyinfo[i].msec.previous)
 * - not a live, per-frame value. ClientThink() (g_client.c) accumulates
 * ucmd->msec into msec.total continuously and only snapshots it into
 * msec.previous once every msec.timespan seconds when that window rolls
 * over, so this shows however that window last closed out, which could
 * be up to msec.timespan seconds stale.
 *
 * This exists so an admin can manually eyeball the same msec numbers
 * ClientThink()'s automatic speedhack detection (msec.max_allowed/
 * min_required) is comparing against, independent of whatever action
 * (freeze, kick) that detection already took.
 *
 * ent:    who to print the results to.
 * client: the invoking admin's client index; unused here.
 *
 * Called from doAdminCommand() (g_admin.c), gated on ADMIN_LEVEL2, when
 * an authenticated admin issues "!dumpmsec".
 */
void adm_dumpmsec(edict_t *ent, int client) {
    gi.cprintf(ent, PRINT_HIGH, "Player MSEC Values:\n");
    for (int i = 0; i < maxclients->value; i++) {
        if (proxyinfo[i].inuse) {
            gi.cprintf(ent, PRINT_HIGH, "  %2i : %-16s %d\n", i, NAME(i), proxyinfo[i].msec.previous);
        }
    }
}

/**
 * Display detailed info about a particular player, mostly from their userinfo
 * string.
 */
void adm_dumpuser(edict_t *ent, int client, int user, bool check) {
    if (gi.argc() < 2) {
        adm_players(ent, client);
        return;
    }
    if (!VALIDCLIENT(user)) {
        return;
    }
    if (check) {
        if (!proxyinfo[user].inuse) {
            return;
        }
    }

    proxyinfo_t *pi = &proxyinfo[user];
    char *ui = pi->userinfo.raw;

    gi.cprintf(ent, PRINT_HIGH, "\nUser Info for \"%s\" [%d]\n",NAME(user), user);
    gi.cprintf(ent, PRINT_HIGH, "  ip address   %s\n", IP(user));
    gi.cprintf(ent, PRINT_HIGH, "  sw version   %s\n", pi->client_version);

    if (FEATURE_SUPPORTED(GMF_EXTRA_USERINFO)) {
        gi.cprintf(ent, PRINT_HIGH, "  Protocol     %d\n", pi->protocol_major);
        gi.cprintf(ent, PRINT_HIGH, "  protocol     %d\n", pi->protocol_minor);
        gi.cprintf(ent, PRINT_HIGH, "  challenge    %d\n", pi->challenge);
        gi.cprintf(ent, PRINT_HIGH, "  mtu          %d\n", pi->mtu);
        gi.cprintf(ent, PRINT_HIGH, "  qport        %d\n", pi->qport);
        gi.cprintf(ent, PRINT_HIGH, "  zlib         %s\n", pi->zlib ? "yes" : "no");
    }

    gi.cprintf(ent, PRINT_HIGH, "  msg level    %s\n", Info_ValueForKey(ui, "msg"));
    gi.cprintf(ent, PRINT_HIGH, "  spectator    %s\n", Info_ValueForKey(ui, "spectator"));
    gi.cprintf(ent, PRINT_HIGH, "  cl_maxfps    %s\n", Info_ValueForKey(ui, "cl_maxfps"));
    gi.cprintf(ent, PRINT_HIGH, "  gender       %s\n", Info_ValueForKey(ui, "gender"));
    gi.cprintf(ent, PRINT_HIGH, "  fov          %s\n", Info_ValueForKey(ui, "fov"));
    gi.cprintf(ent, PRINT_HIGH, "  rate         %s\n", Info_ValueForKey(ui, "rate"));
    gi.cprintf(ent, PRINT_HIGH, "  skin         %s\n", Info_ValueForKey(ui, "skin"));
    gi.cprintf(ent, PRINT_HIGH, "  hand         %s\n", Info_ValueForKey(ui, "hand"));

    if (strlen(pi->gl_driver)) {
        gi.cprintf(ent, PRINT_HIGH, "  gl_driver    %s\n", pi->gl_driver);
    }
    if (proxyinfo[client].admin_level & ADMIN_LEVEL8) {
        gi.cprintf(ent, PRINT_HIGH, "\nFull Userinfo:\n  \"%s\"\n", ui);
    }
}

/**
 * Make each client say their version info. This is pointless given the
 * !versions command built-in to modern clients.
 */
void adm_auth(edict_t *ent) {
    for (int i = 0; i < maxclients->value; i++) {
        if (proxyinfo[i].inuse) {
            stuffcmd(getEnt((i + 1)), "say I'm using $version\n");
        }
    }
}

/**
 * Force all players to display their GL driver and mode.
 */
void adm_gfx(edict_t *ent) {
    unsigned int i;
    for (i = 0; i < maxclients->value; i++) {
        if (proxyinfo[i].inuse) {
            stuffcmd(getEnt((i + 1)), "say I'm using $gl_driver ( $vid_ref ) / $gl_mode\n");
        }
    }
}

/**
 * Kick a player
 */
void adm_boot(edict_t *ent, int client, int user) {
    char tmptext[100];
    if (gi.argc() < 2) {
        adm_players(ent, client);
        return;
    }
    if ((user >= 0) && (user < maxclients->value)) {
        if (proxyinfo[user].inuse) {
            gi.bprintf(PRINT_HIGH, "%s was kicked by %s.\n", proxyinfo[user].name, proxyinfo[client].name);
            Q_snprintf(tmptext, sizeof(tmptext), "\nkick %d\n", user);
            gi.AddCommandString(tmptext);
        }
    }
}

/**
 * Force a map change
 */
void adm_changemap(edict_t *ent, int client, char *mname) {
    char tmptext[100];
    if (q2a_strstr(mname, "\"")) {
        return;
    }
    if (q2a_strstr(mname, ";")) {
        return;
    }
    gi.bprintf(PRINT_HIGH, "%s is changing map to %s.\n", proxyinfo[client].name, mname);
    Q_snprintf(tmptext, sizeof(tmptext), "\nmap %s\n", mname);
    gi.AddCommandString(tmptext);
}

/**
 * Handles running of admin commands by admin players
 */
int doAdminCommand(edict_t *ent, int client) {
    unsigned int i, done = 0;
    int send_to_client;
    edict_t *send_to_ent;
    char send_string[512];
    char abuffer[256];

    if (strlen(gi.args())) {
        Q_snprintf(abuffer, sizeof(abuffer), "COMMAND - %s %s", gi.argv(0), gi.args());
        logEvent(LT_ADMINLOG, client, ent, abuffer, 0, 0.0, true);
        gi.dprintf("%s\n", abuffer);
    }

    if (proxyinfo[client].admin_level & ADMIN_LEVEL1) {
        if (strcmp(gi.argv(0), "!boot") == 0) {
            adm_boot(ent, client, atoi(gi.argv(1)));
            done = 1;
        }
    }

    if (proxyinfo[client].admin_level & ADMIN_LEVEL2) {
        if (strcmp(gi.argv(0), "!dumpmsec") == 0) {
            adm_dumpmsec(ent, client);
            done = 1;
        }
    }
    if (proxyinfo[client].admin_level & ADMIN_LEVEL3) {
        if (strcmp(gi.argv(0), "!changemap") == 0) {
            adm_changemap(ent, client, gi.argv(1));
            done = 1;
        }
    }
    if (proxyinfo[client].admin_level & ADMIN_LEVEL4) {
        if (strcmp(gi.argv(0), "!dumpuser") == 0) {
            adm_dumpuser(ent, client, atoi(gi.argv(1)), true);
            done = 1;
        } else if (strcmp(gi.argv(0), "!dumpuser_any") == 0) {
            adm_dumpuser(ent, client, atoi(gi.argv(1)), false);
            done = 1;
        }
    }
    if (proxyinfo[client].admin_level & ADMIN_LEVEL5) {
        if (strcmp(gi.argv(0), "!auth") == 0) {
            adm_auth(ent);
            done = 1;
            gi.cprintf(ent, PRINT_HIGH, "A new auth command has been issued.\n");
        } else if (strcmp(gi.argv(0), "!gfx") == 0) {
            adm_gfx(ent);
            done = 1;
            gi.cprintf(ent, PRINT_HIGH, "Graphics command issued.\n");
        }
    }

    if (proxyinfo[client].admin_level & ADMIN_LEVEL6) {
        if (strcmp(gi.argv(0), "!dostuff") == 0) {
            if (gi.argc() > 2) {
                send_to_client = atoi(gi.argv(1));
                if (strcmp(gi.argv(1), "all") == 0) {
                    for (send_to_client = 0; send_to_client < maxclients->value; send_to_client++)
                        if (proxyinfo[send_to_client].inuse) {
                            q2a_strncpy(send_string, gi.argv(2), sizeof(send_string)-1);
                            if (gi.argc() > 3)
                                for (i = 3; i < gi.argc(); i++) {
                                    strncat(send_string, " ", sizeof(send_string) - strlen(send_string) - 1);
                                    strncat(send_string, gi.argv(i), sizeof(send_string) - strlen(send_string) - 1);
                                }
                            send_to_ent = getEnt((send_to_client + 1));
                            stuffcmd(send_to_ent, send_string);
                            gi.cprintf(ent, PRINT_HIGH, "Client %d (%s) has been stuffed!\n", send_to_client, proxyinfo[send_to_client].name);
                        }
                } else
                    if (VALIDCLIENT(send_to_client) && proxyinfo[send_to_client].inuse) {
                    q2a_strncpy(send_string, gi.argv(2), sizeof(send_string)-1);
                    if (gi.argc() > 3)
                        for (i = 3; i < gi.argc(); i++) {
                            strncat(send_string, " ", sizeof(send_string) - strlen(send_string) - 1);
                            strncat(send_string, gi.argv(i), sizeof(send_string) - strlen(send_string) - 1);
                        }
                    send_to_ent = getEnt((send_to_client + 1));
                    stuffcmd(send_to_ent, send_string);
                    gi.cprintf(ent, PRINT_HIGH, "Client %d (%s) has been stuffed!\n", send_to_client, proxyinfo[send_to_client].name);
                }
            }
            done = 2;
        }
    }

    if (proxyinfo[client].admin_level & ADMIN_LEVEL8) {
        if ((strcmp(gi.argv(0), "!writewhois") == 0) && (whois_active)) {
            whois_write_file();
            done = 1;
            gi.cprintf(ent, PRINT_HIGH, "Whois file written.\n");
        }
    }
    return done;
}
