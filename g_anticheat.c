/*
Copyright (C) 2007-2008 MDVz0r

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
#include "g_file.h"

/**
 * Fetch an Anticheat exception file from an HTTP server
 */
qboolean AC_GetRemoteFile(char *bfname) {
    FILE *outf;
    char localfilename[MAX_QPATH];

    Q_snprintf(localfilename, sizeof(localfilename), "%s/%s", moddir, ANTICHEATEXCEPTIONLOCALFILE);
    outf = fopen(localfilename, "r");
    if (!outf) {
        gi.cprintf(NULL, PRINT_HIGH, "Error opening local anticheat exception file.\n");
        return qfalse;
    }
    if (!GetURLContents(bfname)) {
        gi.cprintf(NULL, PRINT_HIGH, "Error fetching remote anticheat file: %s\n", bfname);
        fclose(outf);
        return qfalse;
    }
    fclose(outf);
    return qtrue;
}

/**
 *
 */
void AC_UpdateList(void) {
    if ((int) q2adminanticheat_enable->value) {
        qboolean ret;
        char cfgAnticheatRemoteList[100];

        if (!q2adminanticheat_file || isBlank(q2adminanticheat_file->string)) {
            q2a_strncpy(cfgAnticheatRemoteList, ANTICHEATEXCEPTIONREMOTEFILE, sizeof(cfgAnticheatRemoteList));
        } else {
            q2a_strncpy(cfgAnticheatRemoteList, q2adminanticheat_file->string, sizeof(cfgAnticheatRemoteList));
        }
        ret = AC_GetRemoteFile(cfgAnticheatRemoteList);
        if (!ret) {
            gi.dprintf("WARNING: " ANTICHEATEXCEPTIONREMOTEFILE " could not be found\n");
            logEvent(LT_INTERNALWARN, 0, NULL, ANTICHEATEXCEPTIONREMOTEFILE " could not be found", IW_BANSETUPLOAD, 0.0);
            return;
        }
        gi.dprintf("Remote anticheat config downloaded\n");
    }
}

/**
 * exec the exception config file
 *
 * Execute exception list even if the download was not succeeded, since there
 * is probably an old version available.
 */
void AC_LoadExceptions(void) {	
    if ((int) q2adminanticheat_enable->value) {
        AC_UpdateList();
        q2a_strncpy(buffer, "exec " ANTICHEATEXCEPTIONLOCALFILE "\n", sizeof(buffer));
        gi.AddCommandString(buffer);
    }
}

/**
 * This seems redundant
 */
void AC_ReloadExceptions(int startarg, edict_t *ent, int client) {
    AC_LoadExceptions();
    gi.cprintf(ent, PRINT_HIGH, "Exceptionlist loaded.\n");
}

/**
 *
 */
qboolean ReadRemoteHashListFile(char *bfname, char *blname) {
    URL_FILE *handle;
    FILE *outf;

    Q_snprintf(buffer, sizeof(buffer), "%s/%s", moddir, blname);

    // copy from url line by line with fgets //
    outf = fopen(buffer, "w");
    if (!outf) {
        gi.dprintf("Error opening local hash list file.\n");
        return qfalse;
    }

    handle = url_fopen(bfname, "r");
    if (!handle) {
        gi.dprintf("Error opening remote hash list file.\n");
        fclose(outf);
        return qfalse;
    }

    while (!url_feof(handle)) {
        if (!url_fgets(buffer, sizeof (buffer), handle)) {
            // if it did timeout we are not trying again forever... - hifi
            gi.dprintf("Timeout while waiting for remote hashlist reply.\n");
            url_fclose(handle);
            fclose(outf);
            return qfalse;
        }
        fwrite(buffer, 1, strlen(buffer), outf);
    }
    url_fclose(handle);
    fclose(outf);
    return qtrue;
}

/**
 * download up to date anticheat config file including all execptions for r1ch
 * ugly code.. ;x
 */
void getR1chHashList(char *hashname) {

    char cfgHashList_enabled[100];
    q2a_strncpy(cfgHashList_enabled, q2adminhashlist_enable->string, sizeof(cfgHashList_enabled));

    if (cfgHashList_enabled[0] == '1') {
        qboolean ret;
        char cfgHashRemoteList[100];

        if (!q2adminhashlist_dir || isBlank(q2adminhashlist_dir->string)) {
            q2a_strcat(q2a_strcat(q2a_strncpy(cfgHashRemoteList, HASHLISTREMOTEDIR, sizeof(cfgHashRemoteList)), "/"), hashname);
        } else {
            q2a_strcat(q2a_strcat(q2a_strncpy(cfgHashRemoteList, q2adminhashlist_dir->string, sizeof(cfgHashRemoteList)), "/"), hashname);
        }
        ret = ReadRemoteHashListFile(cfgHashRemoteList, hashname);

        if (!ret) {
            gi.dprintf("WARNING: " HASHLISTREMOTEDIR " could not be found\n");
            logEvent(LT_INTERNALWARN, 0, NULL, HASHLISTREMOTEDIR " could not be found", IW_BANSETUPLOAD, 0.0);
        }
    }
}

/**
 * Load the cvar, hash and tokens hash lists
 */
void loadhashlist(void) {
    char cfgHashList_enabled[100];
    q2a_strncpy(cfgHashList_enabled, q2adminhashlist_enable->string, sizeof(cfgHashList_enabled));
    if (cfgHashList_enabled[0] == '1') {
        getR1chHashList("anticheat-cvars.txt");
        getR1chHashList("anticheat-hashes.txt");
        getR1chHashList("anticheat-tokens.txt");
        q2a_strcpy(buffer, "svacupdate\n");
        gi.AddCommandString(buffer);
    }
}

/**
 * Called for the reloadhashlist command
 */
void reloadhashlistRun(int startarg, edict_t *ent, int client) {
    loadhashlist();
    gi.cprintf(ent, PRINT_HIGH, "Remote hashlist loaded.\n");
}
