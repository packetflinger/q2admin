/**
 * Q2Admin
 * whois tracking functions
 */

#include "g_local.h"

user_details *whois_details;
int WHOIS_COUNT = 0;
int whois_active = 0;

/**
 *
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

    strncpy(a1, gi.argv(1), sizeof(a1));
    a1[sizeof(a1)-1] = 0;

    temp = q2a_atoi(a1);
    if ((temp == 0) && (strcmp(a1, "0"))) {
        temp = -1;
    }

    //do numbers first
    if ((temp < maxclients->value) && (temp >= 0)) {
        if ((proxyinfo[temp].inuse) && (proxyinfo[temp].userid >= 0)) {
            //got match, dump details except if admin has proper flag
            if (proxyinfo[temp].q2a_admin & ADMIN_LEVEL7) {
                gi.cprintf(ent, PRINT_HIGH, "  Unable to fetch info for %i\n", temp);
                return;
            }
            gi.cprintf(ent, PRINT_HIGH, "\n  Whois details for client %i\n", temp);
            whois_dumpdetails(client, ent, proxyinfo[temp].userid);
            return;
        }

    }
    //then process all connected clients
    for (i = 0; i < maxclients->value; i++) {
        if ((proxyinfo[i].inuse) && (proxyinfo[i].userid >= 0)) {
            //only do partial match on these, dump all that apply
            if (q2a_strcmp(proxyinfo[i].name, a1) == 0) {
                if (proxyinfo[i].q2a_admin & ADMIN_LEVEL7) {
                    gi.cprintf(ent, PRINT_HIGH, "  Unable to fetch info for %s\n", a1);
                    return;
                }
                gi.cprintf(ent, PRINT_HIGH, "\n  Whois details for %s\n", proxyinfo[i].name);
                whois_dumpdetails(client, ent, proxyinfo[i].userid);
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
                whois_dumpdetails(client, ent, i);
                //got a match, dump details
                return;
            }

        }
    }
    gi.cprintf(ent, PRINT_HIGH, "  No entry found for %s\n", a1);
}

/**
 *
 */
void whois_dumpdetails(int client, edict_t *ent, int userid) {
    unsigned int i;
    for (i = 0; i < 10; i++) {
        if (whois_details[userid].dyn[i].name[0]) {
            if (!proxyinfo[client].q2a_admin) {
                gi.cprintf(ent, PRINT_HIGH, "    %02i. %s\n", i + 1, whois_details[userid].dyn[i].name);
            } else {
                gi.cprintf(ent, PRINT_HIGH, "    %02i. %s %s\n", i + 1, whois_details[userid].dyn[i].name, whois_details[userid].ip);
            }
        }
    }
    gi.cprintf(ent, PRINT_HIGH, "  Last seen: %s\n\n", whois_details[userid].seen);
}

/**
 *
 */
void whois_adduser(int client, edict_t *ent) {
    if (WHOIS_COUNT >= whois_active) {
        WHOIS_COUNT = WHOIS_COUNT - 1; //If max reached, replace latest entry with new client
    }
    whois_details[WHOIS_COUNT].id = WHOIS_COUNT;
    q2a_strncpy(whois_details[WHOIS_COUNT].ip, IP(client), 22);
    q2a_strncpy(whois_details[WHOIS_COUNT].dyn[0].name, proxyinfo[client].name, 16);
    proxyinfo[client].userid = WHOIS_COUNT;
    WHOIS_COUNT++;
}

/**
 *
 */
void whois_newname(int client, edict_t *ent) {
    //called when a client changes name
    unsigned int i;

    if (proxyinfo[client].userid == -1) {
        whois_getid(client, ent);
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
 *
 */
void whois_getid(int client, edict_t *ent) {
    //called when a client connects
    unsigned int i;
    for (i = 0; i < WHOIS_COUNT; i++) {
        if (q2a_strcmp(whois_details[i].ip, IP(client)) == 0) {
            //got a match, store new id
            proxyinfo[client].userid = i;
            whois_newname(client, ent);
            return;
        }
    }
    whois_adduser(client, ent);
}

/**
 *
 */
void whois_update_seen(int client, edict_t *ent) {
    //to be called on client connect and disconnect
    time_t ltimetemp;
    time(&ltimetemp);
    if (proxyinfo[client].userid >= 0) {
        q2a_strcpy(whois_details[proxyinfo[client].userid].seen, ctime(&ltimetemp));
        whois_details[proxyinfo[client].userid].seen[strlen(whois_details[proxyinfo[client].userid].seen) - 1] = 0;
    }
}

/**
 *
 */
void whois_write_file(void) {
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

        q2a_strncpy(temp, whois_details[i].ip, sizeof(temp));
        temp_len = strlen(temp);

        //convert spaces to �
        for (j = 0; j < temp_len; j++) {
            if (temp[j] == ' ') {
                temp[j] = '?';
            }
        }
        fprintf(f, "%i %s ", whois_details[i].id, temp);

        q2a_strncpy(temp, whois_details[i].seen, sizeof(temp));
        temp_len = strlen(temp);

        for (j = 0; j < temp_len; j++) {
            if (temp[j] == ' ') {
                temp[j] = '?';
            }
        }
        fprintf(f, "%s ", temp);

        for (j = 0; j < 10; j++) {
            if (whois_details[i].dyn[j].name[0]) {
                q2a_strncpy(temp, whois_details[i].dyn[j].name, sizeof(temp));
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
 *
 */
void whois_read_file(void) {
    FILE *f;
    char name[256];
    unsigned int i, j;
    int temp_len, name_len;

    Q_snprintf(name, sizeof(name), "%s/%s", moddir, WHOISFILE);

    f = fopen(name, "rb");
    if (!f) {
        gi.dprintf("WARNING: %s could not be found\n", name);
        return;
    }

    WHOIS_COUNT = 0;
    while ((!feof(f)) && (WHOIS_COUNT < whois_active)) {
        fscanf(f, "%i %s %s %s %s %s %s %s %s %s %s %s %s",
                &whois_details[WHOIS_COUNT].id,
                &whois_details[WHOIS_COUNT].ip,
                &whois_details[WHOIS_COUNT].seen,
                &whois_details[WHOIS_COUNT].dyn[0].name,
                &whois_details[WHOIS_COUNT].dyn[1].name,
                &whois_details[WHOIS_COUNT].dyn[2].name,
                &whois_details[WHOIS_COUNT].dyn[3].name,
                &whois_details[WHOIS_COUNT].dyn[4].name,
                &whois_details[WHOIS_COUNT].dyn[5].name,
                &whois_details[WHOIS_COUNT].dyn[6].name,
                &whois_details[WHOIS_COUNT].dyn[7].name,
                &whois_details[WHOIS_COUNT].dyn[8].name,
                &whois_details[WHOIS_COUNT].dyn[9].name);

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
 *
 */
void reloadWhoisFileRun(int startarg, edict_t *ent, int client) {
    whois_read_file();
    gi.cprintf(ent, PRINT_HIGH, "Whois file reloaded.\n");
}
