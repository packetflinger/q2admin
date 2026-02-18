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

/**
 * Fetch an Anticheat exception file from an HTTP server
 */
bool AC_GetRemoteFile(char *bfname) {
    FILE *outf;
    char localfilename[MAX_QPATH];

    Q_snprintf(localfilename, sizeof(localfilename), "%s/%s", moddir, ANTICHEATEXCEPTIONLOCALFILE);
    outf = fopen(localfilename, "r");
    if (!outf) {
        gi.cprintf(NULL, PRINT_HIGH, "Error opening local anticheat exception file.\n");
        return false;
    }
    /*
    if (!GetURLContents(bfname)) {
        gi.cprintf(NULL, PRINT_HIGH, "Error fetching remote anticheat file: %s\n", bfname);
        fclose(outf);
        return false;
    }
    */
    fclose(outf);
    return true;
}

/**
 *
 */
void AC_UpdateList(void) {
    if ((int) q2adminanticheat_enable->value) {
        bool ret;
        char cfgAnticheatRemoteList[100];

        if (!q2adminanticheat_file || isBlank(q2adminanticheat_file->string)) {
            q2a_strncpy(cfgAnticheatRemoteList, ANTICHEATEXCEPTIONREMOTEFILE, sizeof(cfgAnticheatRemoteList));
        } else {
            q2a_strncpy(cfgAnticheatRemoteList, q2adminanticheat_file->string, sizeof(cfgAnticheatRemoteList));
        }
        ret = AC_GetRemoteFile(cfgAnticheatRemoteList);
        if (!ret) {
            //gi.dprintf("WARNING: " ANTICHEATEXCEPTIONREMOTEFILE " could not be found\n");
            logEvent(LT_INTERNALWARN, 0, NULL, ANTICHEATEXCEPTIONREMOTEFILE " could not be found", IW_BANSETUPLOAD, 0.0, true);
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
bool ReadRemoteHashListFile(char *bfname, char *blname) {
    FILE *outf;
    generic_file_t file;
    size_t len;

    Q_snprintf(buffer, sizeof(buffer), "%s/%s", moddir, blname);
    outf = fopen(buffer, "w");
    if (!outf) {
        gi.dprintf("Error opening local hash list file.\n");
        return false;
    }

    file.size = 0xffff;
    file.data = G_Malloc(file.size);
    file.index = 0;

    HTTP_GetFile(&file, bfname);
    len = fwrite(file.data, file.index, 1, outf);
    fclose(outf);
    G_Free(file.data);
    if (!(len == 1 || len == file.index)) {
        return false;
    }
    return true;
}

/**
 * download up to date anticheat config file including all execptions for r1ch
 * ugly code.. ;x
 */
void getR1chHashList(char *hashname) {

    char cfgHashList_enabled[100];
    q2a_strncpy(cfgHashList_enabled, q2adminhashlist_enable->string, sizeof(cfgHashList_enabled));

    if (cfgHashList_enabled[0] == '1') {
        bool ret;
        char cfgHashRemoteList[100];

        if (!q2adminhashlist_dir || isBlank(q2adminhashlist_dir->string)) {
            q2a_strcat(q2a_strcat(q2a_strncpy(cfgHashRemoteList, HASHLISTREMOTEDIR, sizeof(cfgHashRemoteList)), "/"), hashname);
        } else {
            q2a_strcat(q2a_strcat(q2a_strncpy(cfgHashRemoteList, q2adminhashlist_dir->string, sizeof(cfgHashRemoteList)), "/"), hashname);
        }
        ret = ReadRemoteHashListFile(cfgHashRemoteList, hashname);

        if (!ret) {
            // gi.dprintf("WARNING: " HASHLISTREMOTEDIR " could not be found\n");
            logEvent(LT_INTERNALWARN, 0, NULL, HASHLISTREMOTEDIR " could not be found", IW_BANSETUPLOAD, 0.0, true);
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
    }
}

/**
 * Called for the reloadhashlist command
 */
void reloadhashlistRun(int startarg, edict_t *ent, int client) {
    loadhashlist();
    gi.cprintf(ent, PRINT_HIGH, "Remote hashlist loaded.\n");
}
