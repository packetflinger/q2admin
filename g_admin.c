/**
 * Q2Admin
 * ref/admin related stuff
 */

#include "g_local.h"

admin_type admin_pass[MAX_ADMINS];
admin_type q2a_bypass_pass[MAX_ADMINS];
int num_admins = 0;
int num_q2a_admins = 0;

/**
 * Load admin and bypass users from the config files on disk.
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
        fscanf(f, "%s %s %d", q2a_bypass_pass[i].name, q2a_bypass_pass[i].password, &q2a_bypass_pass[i].level);
        if (q2a_bypass_pass[i].level > 0) {
            i++;
        }
    }
    num_q2a_admins = i;
    if (i < MAX_ADMINS) {
        for (i2 = i; i2 < MAX_ADMINS; i2++) {
            q2a_bypass_pass[i2].level = 0;
        }
    }
    gi.cprintf(NULL, PRINT_HIGH, "%d bypass users loaded\n", i);
    fclose(f);
}

/**
 * Show an admin what commands they are permitted to use
 */
void listAdminCommands(edict_t *ent, int client) {
    if (proxyinfo[client].q2a_admin & ADMIN_LEVEL1) {
        gi.cprintf(ent, PRINT_HIGH, "    - !boot <number>\n");
    }
    if (proxyinfo[client].q2a_admin & ADMIN_LEVEL2) {
        gi.cprintf(ent, PRINT_HIGH, "    - !dumpmsec\n");
    }
    if (proxyinfo[client].q2a_admin & ADMIN_LEVEL3) {
        gi.cprintf(ent, PRINT_HIGH, "    - !changemap <mapname>\n");
    }
    if (proxyinfo[client].q2a_admin & ADMIN_LEVEL4) {
        gi.cprintf(ent, PRINT_HIGH, "    - !dumpuser <num>\n");
    }
    if (proxyinfo[client].q2a_admin & ADMIN_LEVEL5) {
        gi.cprintf(ent, PRINT_HIGH, "    - !auth\n");
        gi.cprintf(ent, PRINT_HIGH, "    - !gfx\n");
    }
    if (proxyinfo[client].q2a_admin & ADMIN_LEVEL6) {
        gi.cprintf(ent, PRINT_HIGH, "    - !dostuff <num> <commands>\n");
    }
    if (proxyinfo[client].q2a_admin & ADMIN_LEVEL8) {
        if (whois_active) {
            gi.cprintf(ent, PRINT_HIGH, "    - !writewhois\n");
        }
    }
    gi.cprintf(ent, PRINT_HIGH, "\n");
}

/**
 *
 */
void reloadLoginFileRun(int startarg, edict_t *ent, int client) {
    Read_Admin_cfg();
    gi.cprintf(ent, PRINT_HIGH, "Login file reloaded.\n");
}

/**
 * Get the bypass level associated with a particular user entry
 */
int getBypassLevel(char *givenpass, char *givenname) {
    int got_level = 0;
    unsigned int i;

    for (i = 0; i < num_q2a_admins; i++) {
        if (!q2a_bypass_pass[i].level)
            break;
        if ((strcmp(givenpass, q2a_bypass_pass[i].password) == 0) && (strcmp(givenname, q2a_bypass_pass[i].name) == 0)) {
            got_level = q2a_bypass_pass[i].level;
            break;
        }
    }
    return got_level;
}

/**
 *
 */
int get_admin_level(char *givenpass, char *givenname) {
    int got_level = 0;
    unsigned int i;

    for (i = 0; i < num_admins; i++) {
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
 * Admin command to display the current (from the previous ClientThink run)
 * msec value for each player connected.
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
    if (check) {
        if (!proxyinfo[user].inuse) {
            return;
        }
    }

    proxyinfo_t *pi = &proxyinfo[user];
    char *ui = pi->userinfo;

    gi.cprintf(ent, PRINT_HIGH, "User Info for \"%s\" [%d]\n",NAME(user), user);
    gi.cprintf(ent, PRINT_HIGH, "  IP Address   %s\n", IP(user));
    gi.cprintf(ent, PRINT_HIGH, "  Client ver   %s\n", pi->client_version);
    gi.cprintf(ent, PRINT_HIGH, "  MSG          %s\n", Info_ValueForKey(ui, "msg"));
    gi.cprintf(ent, PRINT_HIGH, "  Spectator    %s\n", Info_ValueForKey(ui, "spectator"));
    gi.cprintf(ent, PRINT_HIGH, "  cl_maxfps    %s\n", Info_ValueForKey(ui, "cl_maxfps"));
    gi.cprintf(ent, PRINT_HIGH, "  Gender       %s\n", Info_ValueForKey(ui, "gender"));
    gi.cprintf(ent, PRINT_HIGH, "  FOV          %s\n", Info_ValueForKey(ui, "fov"));
    gi.cprintf(ent, PRINT_HIGH, "  Rate         %s\n", Info_ValueForKey(ui, "rate"));
    gi.cprintf(ent, PRINT_HIGH, "  Skin         %s\n", Info_ValueForKey(ui, "skin"));
    gi.cprintf(ent, PRINT_HIGH, "  Hand         %s\n", Info_ValueForKey(ui, "hand"));
    if (strlen(pi->gl_driver)) {
        gi.cprintf(ent, PRINT_HIGH, "  gl_driver    %s\n", pi->gl_driver);
    }
    if (proxyinfo[client].q2a_admin & ADMIN_LEVEL8) {
        gi.cprintf(ent, PRINT_HIGH, "  Full Userinfo\n    \"%s\"\n", ui);
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
 *
 */
void ADMIN_gfx(edict_t *ent) {
    unsigned int i;
    for (i = 0; i < maxclients->value; i++) {
        if (proxyinfo[i].inuse) {
            stuffcmd(getEnt((i + 1)), "say I'm using $gl_driver ( $vid_ref ) / $gl_mode\n");
        }
    }
}

/**
 *
 */
void ADMIN_boot(edict_t *ent, int client, int user) {
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
 *
 */
void ADMIN_changemap(edict_t *ent, int client, char *mname) {
    char tmptext[100];
    if (gi.argc() < 2) {
        adm_players(ent, client);
        return;
    }
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
 *
 */
int ADMIN_process_command(edict_t *ent, int client) {
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

    if (proxyinfo[client].q2a_admin & ADMIN_LEVEL1) {
        //Level 1 commands
        if (strcmp(gi.argv(0), "!boot") == 0) {
            ADMIN_boot(ent, client, atoi(gi.argv(1)));
            done = 1;
        }
    }

    if (proxyinfo[client].q2a_admin & ADMIN_LEVEL2) {
        //Level 2 commands
        if (strcmp(gi.argv(0), "!dumpmsec") == 0) {
            adm_dumpmsec(ent, client);
            done = 1;
        }
    }
    if (proxyinfo[client].q2a_admin & ADMIN_LEVEL3) {
        //Level 3 commands
        if (strcmp(gi.argv(0), "!changemap") == 0) {
            ADMIN_changemap(ent, client, gi.argv(1));
            done = 1;
        }
    }
    if (proxyinfo[client].q2a_admin & ADMIN_LEVEL4) {
        //Level 4 commands
        if (strcmp(gi.argv(0), "!dumpuser") == 0) {
            adm_dumpuser(ent, client, atoi(gi.argv(1)), true);
            done = 1;
        } else if (strcmp(gi.argv(0), "!dumpuser_any") == 0) {
            adm_dumpuser(ent, client, atoi(gi.argv(1)), false);
            done = 1;
        }
    }
    if (proxyinfo[client].q2a_admin & ADMIN_LEVEL5) {
        //Level 5 commands
        if (strcmp(gi.argv(0), "!auth") == 0) {
            ADMIN_auth(ent);
            done = 1;
            gi.cprintf(ent, PRINT_HIGH, "A new auth command has been issued.\n");
        } else if (strcmp(gi.argv(0), "!gfx") == 0) {
            ADMIN_gfx(ent);
            done = 1;
            gi.cprintf(ent, PRINT_HIGH, "Graphics command issued.\n");
        }
    }

    if (proxyinfo[client].q2a_admin & ADMIN_LEVEL6) {
        //Level 7 commands

        if (strcmp(gi.argv(0), "!dostuff") == 0) {
            if (gi.argc() > 2) {
                send_to_client = atoi(gi.argv(1));
                if (strcmp(gi.argv(1), "all") == 0) {
                    for (send_to_client = 0; send_to_client < maxclients->value; send_to_client++)
                        if (proxyinfo[send_to_client].inuse) {
                            q2a_strncpy(send_string, gi.argv(2), sizeof(send_string));
                            if (gi.argc() > 3)
                                for (i = 3; i < gi.argc(); i++) {
                                    strcat(send_string, " ");
                                    strcat(send_string, gi.argv(i));
                                }
                            send_to_ent = getEnt((send_to_client + 1));
                            stuffcmd(send_to_ent, send_string);
                            gi.cprintf(ent, PRINT_HIGH, "Client %d (%s) has been stuffed!\n", send_to_client, proxyinfo[send_to_client].name);
                        }
                } else
                    if (proxyinfo[send_to_client].inuse) {
                    q2a_strncpy(send_string, gi.argv(2), sizeof(send_string));
                    if (gi.argc() > 3)
                        for (i = 3; i < gi.argc(); i++) {
                            strcat(send_string, " ");
                            strcat(send_string, gi.argv(i));
                        }
                    send_to_ent = getEnt((send_to_client + 1));
                    stuffcmd(send_to_ent, send_string);
                    gi.cprintf(ent, PRINT_HIGH, "Client %d (%s) has been stuffed!\n", send_to_client, proxyinfo[send_to_client].name);
                }
            }
            done = 2;
        }
    }

    if (proxyinfo[client].q2a_admin & ADMIN_LEVEL8) {
        if ((strcmp(gi.argv(0), "!writewhois") == 0) && (whois_active)) {
            whois_write_file();
            done = 1;
            gi.cprintf(ent, PRINT_HIGH, "Whois file written.\n");
        }
    }
    return done;
}
