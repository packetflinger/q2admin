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

baninfo_t *banhead;
chatbaninfo_t *chatbanhead;

qboolean ChatBanning_Enable = qfalse;
qboolean IPBanning_Enable = qfalse;
qboolean NickBanning_Enable = qfalse;
qboolean VersionBanning_Enable = qfalse;

qboolean kickOnNameChange = qfalse;

char defaultBanMsg[256];
char *currentBanMsg;

long banNumUpto = 0;
long chatBanNumUpto = 0;
char defaultChatBanMsg[256];

/**
 * Download an http(s) text file containing ban definitions and apply them.
 *
 * Reads the remove file in to a blob of bytes, parse the entire blob.
 */
qboolean ReadRemoteBanFile(char *bfname) {
    generic_file_t file;
    size_t dataLen;

    file.size = 0xffff;
    file.data = G_Malloc(file.size);
    file.index = 0;

    HTTP_GetFile(&file, bfname);
    parseBanFileContents(file.data);

    G_Free(file.data);
    return qtrue;
}

/**
 * Read a local text file containing ban definitions and apply them.
 *
 * Process each ban file line by line
 */
qboolean ReadBanFile(char *bfname) {
    FILE *banfile;
    unsigned int uptoLine = 0;

    banfile = fopen(bfname, "rt");
    if (!banfile) {
        return qfalse;
    }

    // read line by line
    while (fgets(buffer, 256, banfile)) {
        char *data = buffer;

        SKIPBLANK(data);
        uptoLine++;

        if (!(data[0] == ';' || data[0] == '\n' || isBlank(data))) {
            if (startContains(data, "BAN:")) {
                data = ban_parseBan(data);
            } else if (startContains(data, "CHATBAN:")) {
                data = ban_parseChatban(data);
            } else if (startContains(data, "INCLUDE:")) {
                data = ban_parseInclude(data);
            } else {
                gi.cprintf(NULL, PRINT_HIGH,
                        "[q2admin] invalid ban at line %d, ignoring\n",
                        uptoLine);
                continue;
            }
        }
    }
    fclose(banfile);
    return qtrue;
}

/**
 * Remove all player and chat bans, freeing any memory
 */
void freeBanLists(void) {
    while (banhead) {
        baninfo_t *freeentry = banhead;
        banhead = banhead->next;

        if (freeentry->msg) {
            gi.TagFree(freeentry->msg);
        }

        gi.TagFree(freeentry);
    }

    while (chatbanhead) {
        chatbaninfo_t *freeentry = chatbanhead;
        chatbanhead = chatbanhead->next;

        if (freeentry->msg) {
            gi.TagFree(freeentry->msg);
        }

        gi.TagFree(freeentry);
    }

    banNumUpto = 0;
    chatBanNumUpto = 0;
}

/**
 * Load bans from the various ban files configured. These include:
 *
 * 1. q2-folder ban file
 * 2. mod-folder ban file
 * 3. a remote ban file on http server
 * 4. any ban files that are included in any of the above files
 *
 * Ban parsing is recursive, it will parse files included from other files
 */
void readBanLists(void) {
    char cfgRemoteFileEnabled[100];
    qboolean ret;

    freeBanLists();

    ret = ReadBanFile(configfile_ban->string);  // q2-folder first
    Q_snprintf(buffer, sizeof(buffer), "%s/%s", moddir, configfile_ban->string);
    if (ReadBanFile(buffer)) {  // mod-folder next
        ret = qtrue;
    }

    if (!ret) {
        gi.cprintf(NULL, PRINT_HIGH, "WARNING: %s could not be found\n", configfile_ban->string);
        logEvent(LT_INTERNALWARN, 0, NULL, va("%s could not be found", configfile_ban->string), IW_BANSETUPLOAD, 0.0);
    }

    q2a_strncpy(cfgRemoteFileEnabled, q2adminbanremotetxt_enable->string, sizeof(cfgRemoteFileEnabled));

    if (cfgRemoteFileEnabled[0] == '1') {
        char cfgRemoteFile[100];

        if (!q2adminbanremotetxt || isBlank(q2adminbanremotetxt->string)) {
            q2a_strncpy(cfgRemoteFile, BANLISTREMOTEFILE, sizeof(cfgRemoteFile));
        } else {
            q2a_strncpy(cfgRemoteFile, q2adminbanremotetxt->string, sizeof(cfgRemoteFile));
        }

        ret = ReadRemoteBanFile(cfgRemoteFile);

        Q_snprintf(buffer, sizeof(buffer), "%s/%s", moddir, cfgRemoteFile);
        if (ReadBanFile(buffer)) {
            ret = qtrue;
        }

        if (!ret) {
            gi.dprintf("WARNING: " BANLISTREMOTEFILE " could not be found\n");
            logEvent(LT_INTERNALWARN, 0, NULL, BANLISTREMOTEFILE " could not be found", IW_BANSETUPLOAD, 0.0);
        }
    }
}

/**
 * Called when a server admin uses the "sv !ban..." command
 */
void banRun(int startarg, edict_t *ent, int client) {
    char *cp;
    char *tempcp;
    int clienti, num;
    unsigned int i, save;
    qboolean like, all, re;
    baninfo_t *newentry;
    char savecmd[384];
    char strbuffer[384];
    qboolean nocheck = qfalse;
    char *ipstr;
    char tempip[INET6_ADDRSTRLEN];
    qboolean allver;
    char *masktoken;

    if (gi.argc() <= startarg) {
        gi.cprintf(ent, PRINT_HIGH, BANCMD_LAYOUT);
        return;
    }

    cp = gi.argv(startarg);
    startarg++;

    // allocate memory for ban record
    newentry = gi.TagMalloc(sizeof(baninfo_t), TAG_LEVEL);
    q2a_memset(newentry, 0, sizeof(baninfo_t));

    q2a_strncpy(savecmd, "BAN: ", sizeof(savecmd));


    // get +/-
    if (*cp == '-') {
        newentry->exclude = qtrue;

        if (gi.argc() <= startarg) {
            gi.cprintf(ent, PRINT_HIGH, "UpTo: %s\n", savecmd);
            gi.cprintf(ent, PRINT_HIGH, BANCMD_LAYOUT);
            gi.TagFree(newentry);
            return;
        }

        cp = gi.argv(startarg);
        startarg++;

        q2a_strcat(savecmd, "- ");
    } else if (*cp == '+') {
        newentry->exclude = qfalse;

        if (gi.argc() <= startarg) {
            gi.cprintf(ent, PRINT_HIGH, "UpTo: %s\n", savecmd);
            gi.cprintf(ent, PRINT_HIGH, BANCMD_LAYOUT);
            gi.TagFree(newentry);
            return;
        }

        cp = gi.argv(startarg);
        startarg++;

        q2a_strcat(savecmd, "+ ");
    } else {
        newentry->exclude = qtrue;
        q2a_strcat(savecmd, "- ");
    }

    if (startContains(cp, "ALL")) {
        newentry->type = NICKALL;
        newentry->maxnumberofconnects = 0;
        newentry->numberofconnects = 0;
        newentry->msg = NULL;
        newentry->floodinfo.chatFloodProtect = qfalse;
        q2a_memset(&newentry->addr, 0, sizeof(netadr_t));
        all = qtrue;


        if (gi.argc() <= startarg) {
            cp = "";
        } else {
            cp = gi.argv(startarg);
            startarg++;
        }
    } else {
        all = qfalse;

        // Name:
        if (startContains(cp, "NAME")) {
            if (gi.argc() <= startarg) {
                gi.cprintf(ent, PRINT_HIGH, "UpTo: %s\n", savecmd);
                gi.cprintf(ent, PRINT_HIGH, BANCMD_LAYOUT);
                gi.TagFree(newentry);
                return;
            }

            cp = gi.argv(startarg);
            startarg++;

            q2a_strcat(savecmd, "NAME ");

            // Like?
            if (startContains(cp, "LIKE")) {
                like = qtrue;

                if (gi.argc() <= startarg) {
                    gi.cprintf(ent, PRINT_HIGH, "UpTo: %s\n", savecmd);
                    gi.cprintf(ent, PRINT_HIGH, BANCMD_LAYOUT);
                    gi.TagFree(newentry);
                    return;
                }

                cp = gi.argv(startarg);
                startarg++;

                q2a_strcat(savecmd, "LIKE ");

            }//re?
            else if (startContains(cp, "RE")) {
                like = qfalse;
                re = qtrue;

                if (gi.argc() <= startarg) {
                    gi.cprintf(ent, PRINT_HIGH, "UpTo: %s\n", savecmd);
                    gi.cprintf(ent, PRINT_HIGH, BANCMD_LAYOUT);
                    gi.TagFree(newentry);
                    return;
                }

                cp = gi.argv(startarg);
                startarg++;

                q2a_strcat(savecmd, "RE ");
            } else {
                like = qfalse;
                re = qfalse;
            }

            // BLANK or ALL or name
            if (startContains(cp, "BLANK")) {
                newentry->type = NICKBLANK;
                q2a_strcat(savecmd, "BLANK ");
            } else if (startContains(cp, "ALL")) {
                newentry->type = NICKALL;
                q2a_strcat(savecmd, "ALL ");
            } else if (startContains(cp, "%P")) {
                if (gi.argc() <= startarg) {
                    gi.cprintf(ent, PRINT_HIGH, "UpTo: %s\n", savecmd);
                    gi.cprintf(ent, PRINT_HIGH, BANCMD_LAYOUT);
                    gi.TagFree(newentry);
                    return;
                }

                cp = gi.argv(startarg);
                startarg++;

                if (!isdigit(*cp)) {
                    gi.cprintf(ent, PRINT_HIGH, "UpTo: %s\n", savecmd);
                    gi.cprintf(ent, PRINT_HIGH, BANCMD_LAYOUT);
                    gi.TagFree(newentry);
                    return;
                }

                if (like) {
                    newentry->type = NICKLIKE;
                } else if (re) {
                    // This is NICKLIKE instead of NICKRE on purpose. Since
                    // you're limited to the player's name when using %P
                    // making the rule a regex is a waste of resources. A LIKE
                    // rule will be an identical match without the overhead.
                    newentry->type = NICKLIKE;
                } else {
                    newentry->type = NICKEQ;
                }

                clienti = q2a_atoi(cp);

                if (clienti < 0 || clienti > maxclients->value || !proxyinfo[clienti].inuse) {
                    gi.cprintf(ent, PRINT_HIGH, "UpTo: %s\n", savecmd);
                    gi.cprintf(ent, PRINT_HIGH, BANCMD_LAYOUT);
                    gi.TagFree(newentry);
                    return;
                }

                q2a_strncpy(newentry->nick, proxyinfo[clienti].name, sizeof(newentry->nick));
                q2a_strcat(savecmd, "\"");
                q2a_strcat(savecmd, proxyinfo[clienti].name);
                q2a_strcat(savecmd, "\" ");

                if (newentry->type == NICKRE) { // compile RE
                    q2a_strncpy(strbuffer, newentry->nick, sizeof(strbuffer));
                    q_strupr(strbuffer);

                    newentry->r = re_compile(strbuffer);
                    if (!newentry->r) {
                        gi.cprintf(ent, PRINT_HIGH, "UpTo: %s\n", savecmd);
                        gi.cprintf(ent, PRINT_HIGH, BANCMD_LAYOUT);
                        gi.TagFree(newentry);
                        return;
                    }
                }
            } else {
                if (like) {
                    newentry->type = NICKLIKE;
                } else if (re) {
                    newentry->type = NICKRE;
                } else {
                    newentry->type = NICKEQ;
                }

                q2a_strcat(savecmd, "\"");
                q2a_strcat(savecmd, cp);
                q2a_strcat(savecmd, "\" ");

                // copy name
                processstring(newentry->nick, cp, sizeof(newentry->nick) - 1, 0);
            }

            if (newentry->type == NICKRE) { // compile RE
                q2a_strncpy(strbuffer, newentry->nick, sizeof(strbuffer));
                q_strupr(strbuffer);

                newentry->r = re_compile(strbuffer);
                if (!newentry->r) {
                    gi.cprintf(ent, PRINT_HIGH, "UpTo: %s\n", savecmd);
                    gi.cprintf(ent, PRINT_HIGH, BANCMD_LAYOUT);
                    gi.TagFree(newentry);
                    return;
                }
            }

            if (gi.argc() <= startarg) {
                cp = "";
            } else {
                cp = gi.argv(startarg);
                startarg++;
            }
        } else {
            newentry->type = NICKALL;
        }

        // get ip address
        q2a_memset(&newentry->addr, 0, sizeof(netadr_t));
        if (startContains(cp, "IP")) {
            if (gi.argc() <= startarg) {
                gi.cprintf(ent, PRINT_HIGH, "UpTo: %s\n", savecmd);
                gi.cprintf(ent, PRINT_HIGH, BANCMD_LAYOUT);
                gi.TagFree(newentry);
                return;
            }

            cp = gi.argv(startarg);
            startarg++;

            q2a_strcat(savecmd, "IP ");
            if (startContains(cp, "VPN")) {
                newentry->vpn = qtrue;
                q2a_strcat(savecmd, "VPN ");
            } else {
                if (startContains(cp, "%P")) {
                    if (gi.argc() <= startarg) {
                        gi.cprintf(ent, PRINT_HIGH, "UpTo: %s\n", savecmd);
                        gi.cprintf(ent, PRINT_HIGH, BANCMD_LAYOUT);
                        gi.TagFree(newentry);
                        return;
                    }

                    cp = gi.argv(startarg);
                    startarg++;

                    if (!isdigit(*cp)) {
                        gi.cprintf(ent, PRINT_HIGH, "UpTo: %s\n", savecmd);
                        gi.cprintf(ent, PRINT_HIGH, BANCMD_LAYOUT);
                        gi.TagFree(newentry);
                        return;
                    }

                    clienti = q2a_atoi(cp);
                    newentry->addr = proxyinfo[clienti].address;

                    while (isdigit(*cp)) {
                        cp++;
                    }

                    // catch use of a mask (eg: %p #/##)
                    if (startContains(cp, "/")) {
                        cp++;
                        int masklen = q2a_atoi(cp);
                        if (masklen >= 0 && masklen <= 128) {
                            newentry->addr.mask_bits = masklen;
                        }
                        while (isdigit(*cp)) {
                            cp++;
                        }
                        SKIPBLANK(cp);
                    }

                    if (clienti < 0 || clienti > maxclients->value || !proxyinfo[clienti].inuse) {
                        gi.cprintf(ent, PRINT_HIGH, "UpTo: %s\n", savecmd);
                        gi.cprintf(ent, PRINT_HIGH, BANCMD_LAYOUT);
                        gi.TagFree(newentry);
                        return;
                    }

                    ipstr = IPSTRMASK(&newentry->addr);
                    Q_snprintf(
                        savecmd + q2a_strlen(savecmd),
                        INET6_ADDRSTRLEN,
                        "%s ",
                        ipstr
                    );

                    while (isdigit(*cp)) {
                        cp++;
                    }

                    if (*cp != 0) {
                        gi.cprintf(ent, PRINT_HIGH, "UpTo: %s\n", savecmd);
                        gi.cprintf(ent, PRINT_HIGH, BANCMD_LAYOUT);
                        gi.TagFree(newentry);
                        return;
                    }
                } else {
                    q2a_strcat(savecmd, cp);
                    q2a_strcat(savecmd, " ");

                    if (isxdigit(*cp)) {
                        tempcp = cp;
                        // find the end of the IP string
                        while (!isspace(*tempcp)) {
                            tempcp++;
                        }
                        q2a_memset(tempip, 0, sizeof(tempip));
                        q2a_memcpy(tempip, cp, (tempcp-cp));
                        newentry->addr = net_parseIPAddressMask(tempip);
                        cp = tempcp;
                    }
                    SKIPBLANK(cp);
                }
            }

            if (gi.argc() <= startarg) {
                cp = "";
            } else {
                cp = gi.argv(startarg);
                startarg++;
            }
        }

        if (startContains(cp, "VERSION")) {
            if (gi.argc() <= startarg) {
                gi.cprintf(ent, PRINT_HIGH, "UpTo: %s\n", savecmd);
                gi.cprintf(ent, PRINT_HIGH, BANCMD_LAYOUT);
                gi.TagFree(newentry);
                return;
            }

            cp = gi.argv(startarg);
            startarg++;

            q2a_strcat(savecmd, "VERSION ");
            if (startContains(cp, "LIKE")) {
                newentry->vtype = VERSION_LIKE;

                if (gi.argc() <= startarg) {
                    gi.cprintf(ent, PRINT_HIGH, "UpTo: %s\n", savecmd);
                    gi.cprintf(ent, PRINT_HIGH, BANCMD_LAYOUT);
                    gi.TagFree(newentry);
                    return;
                }

                cp = gi.argv(startarg);
                startarg++;
                q2a_strcat(savecmd, "LIKE ");
            } else if (startContains(cp, "RE")) {
                newentry->vtype = VERSION_REGEX;
                if (gi.argc() <= startarg) {
                    gi.cprintf(ent, PRINT_HIGH, "UpTo: %s\n", savecmd);
                    gi.cprintf(ent, PRINT_HIGH, BANCMD_LAYOUT);
                    gi.TagFree(newentry);
                    return;
                }
                cp = gi.argv(startarg);
                startarg++;
                q2a_strcat(savecmd, "RE ");
            } else {
                newentry->vtype = VERSION_EQUALS;
            }

            q2a_strcat(savecmd, "\"");
            q2a_strcat(savecmd, cp);
            q2a_strcat(savecmd, "\" ");

            q2a_memset(newentry->version, 0, sizeof(newentry->version));
            processstring(newentry->version, cp, sizeof(newentry->version)-1, '\"');
            if (gi.argc() <= startarg) {
                cp = "";
            } else {
                cp = gi.argv(startarg);
                startarg++;
            }
            allver = qfalse;
            if (newentry->vtype == VERSION_REGEX) {
                q2a_strncpy(strbuffer, newentry->version, sizeof(strbuffer));
                q_strupr(strbuffer);
                newentry->vr = re_compile(strbuffer);
                if (!newentry->vr) {
                    gi.cprintf(ent, PRINT_HIGH, "UpTo: %s\n", savecmd);
                    gi.cprintf(ent, PRINT_HIGH, BANCMD_LAYOUT);
                    gi.TagFree(newentry);
                    return;
                }
            }
        } else {
            allver = qtrue;
        }
    }

    // get PASSWORD
    if (!newentry->exclude && startContains(cp, "PASSWORD")) {
        if (gi.argc() <= startarg) {
            gi.cprintf(ent, PRINT_HIGH, "UpTo: %s\n", savecmd);
            gi.cprintf(ent, PRINT_HIGH, BANCMD_LAYOUT);
            gi.TagFree(newentry);
            return;
        }

        cp = gi.argv(startarg);
        startarg++;

        q2a_strcat(savecmd, "PASSWORD ");
        q2a_strcat(savecmd, "\"");
        q2a_strcat(savecmd, cp);
        q2a_strcat(savecmd, "\" ");

        // copy password
        processstring(newentry->password, cp, sizeof (newentry->password) - 1, 0);

        if (gi.argc() <= startarg) {
            cp = "";
        } else {
            cp = gi.argv(startarg);
            startarg++;
        }
    } else {
        newentry->password[0] = 0;
    }


    // get MAX
    if (!newentry->exclude && startContains(cp, "MAX")) {
        if (gi.argc() <= startarg) {
            gi.cprintf(ent, PRINT_HIGH, "UpTo: %s\n", savecmd);
            gi.cprintf(ent, PRINT_HIGH, BANCMD_LAYOUT);
            gi.TagFree(newentry);
            return;
        }

        cp = gi.argv(startarg);
        startarg++;

        q2a_strcat(savecmd, "MAX ");
        q2a_strcat(savecmd, cp);
        q2a_strcat(savecmd, " ");

        newentry->maxnumberofconnects = q2a_atoi(cp);

        if (gi.argc() <= startarg) {
            cp = "";
        } else {
            cp = gi.argv(startarg);
            startarg++;
        }
    } else {
        newentry->maxnumberofconnects = 0;
    }

    newentry->numberofconnects = 0;

    // get FLOOD
    if (!newentry->exclude && startContains(cp, "FLOOD")) {
        if (gi.argc() <= startarg + 2) {
            gi.cprintf(ent, PRINT_HIGH, "UpTo: %s\n", savecmd);
            gi.cprintf(ent, PRINT_HIGH, BANCMD_LAYOUT);
            gi.TagFree(newentry);
            return;
        }

        cp = gi.argv(startarg);
        startarg++;

        newentry->floodinfo.chatFloodProtectNum = q2a_atoi(cp);

        cp = gi.argv(startarg);
        startarg++;

        newentry->floodinfo.chatFloodProtectSec = q2a_atoi(cp);

        cp = gi.argv(startarg);
        startarg++;

        newentry->floodinfo.chatFloodProtectSilence = q2a_atoi(cp);

        if (gi.argc() <= startarg) {
            cp = "";
        } else {
            cp = gi.argv(startarg);
            startarg++;
        }

        if (newentry->floodinfo.chatFloodProtectNum && newentry->floodinfo.chatFloodProtectSec) {
            Q_snprintf(
                    savecmd + q2a_strlen(savecmd),
                    sizeof(savecmd) - q2a_strlen(savecmd),
                    "FLOOD %d %d %d ",
                    newentry->floodinfo.chatFloodProtectNum,
                    newentry->floodinfo.chatFloodProtectSec,
                    newentry->floodinfo.chatFloodProtectSilence
            );
            newentry->floodinfo.chatFloodProtect = qtrue;
        } else {
            newentry->floodinfo.chatFloodProtect = qfalse;
        }
    } else {
        newentry->floodinfo.chatFloodProtect = qfalse;
    }


    // get MSG
    if (startContains(cp, "MSG")) {
        if (gi.argc() <= startarg) {
            gi.cprintf(ent, PRINT_HIGH, "UpTo: %s\n", savecmd);
            gi.cprintf(ent, PRINT_HIGH, BANCMD_LAYOUT);
            gi.TagFree(newentry);
            return;
        }

        cp = gi.argv(startarg);
        startarg++;

        q2a_strcat(savecmd, "MSG ");
        q2a_strcat(savecmd, "\"");
        q2a_strcat(savecmd, cp);
        q2a_strcat(savecmd, "\" ");

        // copy MSG
        processstring(buffer2, cp, sizeof (buffer2) - 1, '\"');

        num = q2a_strlen(buffer2);

        if (num) {
            newentry->msg = gi.TagMalloc(num + 1, TAG_LEVEL);
            q2a_strncpy(newentry->msg, buffer2, num + 1);
        } else {
            newentry->msg = NULL;
        }

        if (gi.argc() <= startarg) {
            cp = "";
        } else {
            cp = gi.argv(startarg);
            startarg++;
        }
    } else {
        newentry->msg = NULL;
    }

    // get Timeout
    if (startContains(cp, "TIME")) {
        if (gi.argc() <= startarg) {
            gi.cprintf(ent, PRINT_HIGH, "UpTo: %s\n", savecmd);
            gi.cprintf(ent, PRINT_HIGH, BANCMD_LAYOUT);
            if (newentry->msg) {
                gi.TagFree(newentry->msg);
            }
            gi.TagFree(newentry);
            return;
        }

        cp = gi.argv(startarg);
        startarg++;

        newentry->timeout = q2a_atof(cp);
        if (newentry->timeout < 1.0) {
            newentry->timeout = 1.0;
        }

        newentry->timeout *= 60.0;
        newentry->timeout += ltime;

        if (gi.argc() <= startarg) {
            cp = "";
        } else {
            cp = gi.argv(startarg);
            startarg++;
        }
    } else {
        newentry->timeout = 0.0;
    }

    // get Save?
    if (startContains(cp, "SAVE")) {
        if (newentry->timeout >= 1.0) {
            gi.cprintf(ent, PRINT_HIGH, "UpTo: %s\n", savecmd);
            gi.cprintf(ent, PRINT_HIGH, BANCMD_LAYOUT);
            if (newentry->msg) {
                gi.TagFree(newentry->msg);
            }
            gi.TagFree(newentry);
            return;
        }

        if (gi.argc() <= startarg) {
            cp = "";
        } else {
            cp = gi.argv(startarg);
            startarg++;
        }

        newentry->loadType = LT_PERM;

        if (startContains(cp, "MOD")) {
            if (gi.argc() <= startarg) {
                cp = "";
            } else {
                cp = gi.argv(startarg);
                startarg++;
            }

            save = 2;
        } else {
            save = 1;
        }
    } else {
        newentry->loadType = LT_TEMP;
        save = 0;
    }


    // get nocheck
    if (startContains(cp, "NOCHECK")) {
        if (gi.argc() <= startarg) {
            cp = "";
        } else {
            cp = gi.argv(startarg);
            startarg++;
        }

        nocheck = qtrue;
    }

    if (*cp != 0) {
        // something is wrong...
        if (newentry->msg) {
            gi.TagFree(newentry->msg);
        }
        gi.TagFree(newentry);
        gi.cprintf(ent, PRINT_HIGH, "UpTo: %s\n", savecmd);
        gi.cprintf(ent, PRINT_HIGH, BANCMD_LAYOUT);
        return;
    }

    // do you have a valid ban record?
    if (!all && newentry->type == NICKALL && newentry->addr.mask_bits == 0 && newentry->maxnumberofconnects == 0 && (!allver && !newentry->version)) {
        // no, abort
        if (newentry->msg) {
            gi.TagFree(newentry->msg);
        }
        gi.dprintf("problems!\n");
        gi.TagFree(newentry);
        gi.cprintf(ent, PRINT_HIGH, "UpTo: %s\n", savecmd);
        gi.cprintf(ent, PRINT_HIGH, BANCMD_LAYOUT);
        return;
    } else {
        // we have the ban record...
        // insert at the head of the correct list.
        newentry->bannum = banNumUpto;
        banNumUpto++;

        newentry->next = banhead;
        banhead = newentry;

        gi.cprintf(ent, PRINT_HIGH, "Ban Added!!\n");

        if (save) {
            FILE *banlistfptr;

            if (save == 1) {
                q2a_strncpy(buffer, configfile_ban->string, sizeof(buffer));
            } else {
                Q_snprintf(buffer, sizeof(buffer), "%s/%s", moddir, configfile_ban->string);
            }

            banlistfptr = fopen(buffer, "at");
            if (!banlistfptr) {
                gi.cprintf(ent, PRINT_HIGH, "Error opening banfile!\n");
            } else {
                fprintf(banlistfptr, "%s\n", savecmd);
                fclose(banlistfptr);

                gi.cprintf(ent, PRINT_HIGH, "Ban stored.\n");
            }
        }

        if (!nocheck) {
            for (clienti = 0; clienti < maxclients->value; clienti++) {
                if (proxyinfo[clienti].inuse) {
                    edict_t *enti = getEnt((clienti + 1));
                    if (checkCheckIfBanned(enti, clienti)) {
                        logEvent(LT_BAN, clienti, enti, currentBanMsg, 0, 0.0);
                        gi.cprintf(NULL, PRINT_HIGH, "%s: %s (IP = %s)\n", proxyinfo[clienti].name, currentBanMsg, IP(clienti));
                        gi.cprintf(enti, PRINT_HIGH, "%s: %s\n", proxyinfo[clienti].name, currentBanMsg);
                        addCmdQueue(clienti, QCMD_DISCONNECT, 1, 0, currentBanMsg);
                    }
                }
            }
        }
    }
}

void reloadbanfileRun(int startarg, edict_t *ent, int client) {
    readBanLists();
    gi.cprintf(ent, PRINT_HIGH, "Bans reloaded.\n");
}

/**
 * Check if a particular client is banned
 *
 * Called from ClientConnect()
 */
int checkBanList(edict_t *ent, int client) {
    baninfo_t *checkentry = banhead;
    baninfo_t *prevcheckentry = NULL;
    char strbuffer[256];

    while (checkentry) {
        if (checkentry->type != NOTUSED) {
            if (checkentry->timeout && checkentry->timeout < ltime) {
                unsigned int clienti;

                // found dead ban, delete...
                for (clienti = 0; clienti < maxclients->value; clienti++) {
                    if (proxyinfo[clienti].baninfo == checkentry) {
                        proxyinfo[clienti].baninfo = NULL;
                    }
                }

                if (prevcheckentry) {
                    prevcheckentry->next = checkentry->next;
                } else {
                    banhead = checkentry->next;
                }

                if (checkentry->msg) {
                    gi.TagFree(checkentry->msg);
                }

                gi.TagFree(checkentry);

                if (prevcheckentry) {
                    checkentry = prevcheckentry->next;
                } else {
                    checkentry = banhead;
                }
                continue;
            }

            // check name...
            if (checkentry->type != NICKALL) {
                if (!NickBanning_Enable) {
                    prevcheckentry = checkentry;
                    checkentry = checkentry->next;
                    continue;
                }

                switch (checkentry->type) {
                    case NICKEQ:
                        if (Q_stricmp(proxyinfo[client].name, checkentry->nick)) {
                            prevcheckentry = checkentry;
                            checkentry = checkentry->next;
                            continue;
                        }
                        break;

                    case NICKLIKE:
                        if (!stringContains(proxyinfo[client].name, checkentry->nick)) {
                            prevcheckentry = checkentry;
                            checkentry = checkentry->next;
                            continue;
                        }
                        break;

                    case NICKRE:
                        q2a_strncpy(strbuffer, proxyinfo[client].name, sizeof(strbuffer));
                        q_strupr(strbuffer);

                        int len;
                        if (re_matchp(checkentry->r, strbuffer, &len) != 0) {
                            prevcheckentry = checkentry;
                            checkentry = checkentry->next;
                            continue;
                        }
                        break;

                    case NICKBLANK:
                        if (!isBlank(proxyinfo[client].name)) {
                            prevcheckentry = checkentry;
                            checkentry = checkentry->next;
                            continue;
                        }
                        break;
                }
            }

            // check IP
            if (IPBanning_Enable) {
                if (checkentry->addr.mask_bits > 0) {
                    if (!net_contains(&checkentry->addr, &proxyinfo[client].address)) {
                        prevcheckentry = checkentry;
                        checkentry = checkentry->next;
                        continue;
                    }
                }
                if (checkentry->vpn && !proxyinfo[client].vpn.is_vpn) {
                    prevcheckentry = checkentry;
                    checkentry = checkentry->next;
                    continue;
                }
            }

            // check version
            if (VersionBanning_Enable && checkentry->version[0] != 0) {
                if (checkentry->vtype == VERSION_EQUALS) {
                    if (Q_stricmp(checkentry->version, proxyinfo[client].client_version) != 0) {
                        prevcheckentry = checkentry;
                        checkentry = checkentry->next;
                        continue;
                    }
                } else if (checkentry->vtype == VERSION_LIKE) {
                    if (!stringContains(proxyinfo[client].client_version, checkentry->version)) {
                        prevcheckentry = checkentry;
                        checkentry = checkentry->next;
                        continue;
                    }
                } else if (checkentry->vtype == VERSION_REGEX) {
                    q2a_strncpy(strbuffer, proxyinfo[client].client_version, sizeof(strbuffer));
                    q_strupr(strbuffer);
                    int len;
                    if (re_matchp(checkentry->vr, strbuffer, &len) != 0) {
                        prevcheckentry = checkentry;
                        checkentry = checkentry->next;
                        continue;
                    }
                }
            }

            if (checkentry->exclude) {  // ban situation
                if (checkentry->msg) {
                    currentBanMsg = checkentry->msg;
                }
                return 1;
            }

            if (checkentry->password[0]) {
                char *s = Info_ValueForKey(proxyinfo[client].userinfo, "pw");

                Q_snprintf(
                        strbuffer,
                        sizeof(strbuffer),
                        "%s allowed using password (ban id:%d)",
                        NAME(client), checkentry->bannum
                );
                logEvent(LT_ADMINLOG, client, ent, strbuffer, 0, 0.0);

                if (q2a_strcmp(checkentry->password, s)) {
                    if (checkentry->msg) {
                        currentBanMsg = checkentry->msg;
                    }
                    return 1;
                }
            }

            // check max connections..
            if (checkentry->maxnumberofconnects) {
                if (checkentry->numberofconnects >= checkentry->maxnumberofconnects) {
                    if (checkentry->msg) {
                        currentBanMsg = checkentry->msg;
                    }

                    return 1;
                }

                proxyinfo[client].baninfo = checkentry;
                checkentry->numberofconnects++;
            }

            // user included...  set user settings for this include ban

            if (checkentry->floodinfo.chatFloodProtect) {
                proxyinfo[client].floodinfo = checkentry->floodinfo;
            }

            return 0;
        }

        prevcheckentry = checkentry;
        checkentry = checkentry->next;
    }

    return 0;
}

int checkCheckIfBanned(edict_t *ent, int client) {
    if (proxyinfo[client].baninfo) {
        if (proxyinfo[client].baninfo->numberofconnects) {
            proxyinfo[client].baninfo->numberofconnects--;
        }
        proxyinfo[client].baninfo = NULL;
    }
    if (!IPBanning_Enable && !NickBanning_Enable && !VersionBanning_Enable) {
        return 0;
    }
    currentBanMsg = defaultBanMsg;
    return checkBanList(ent, client);
}

void listbansRun(int startarg, edict_t *ent, int client) {
    addCmdQueue(client, QCMD_DISPBANS, 0, 0, 0);
}

void displayNextBan(edict_t *ent, int client, long bannum) {
    long upto = bannum;
    baninfo_t *findentry = banhead;

    bannum++;

    if (bannum == 1) {
        gi.cprintf(ent, PRINT_HIGH, "Current ban list:\n");
    }

    while (findentry && upto) {
        findentry = findentry->next;

        if (findentry && findentry->type != NOTUSED) {
            if (!findentry->timeout || findentry->timeout > ltime) {
                upto--;
            }
        }
    }

    if (findentry) {
        if (findentry->loadType == LT_TEMP) {
            Q_snprintf(buffer, sizeof(buffer), " (%ld, Temporary) BAN:", findentry->bannum);
        } else {
            Q_snprintf(buffer, sizeof(buffer), " (%ld, Permanent) BAN:", findentry->bannum);
        }

        if (!findentry->exclude) {
            q2a_strcat(buffer, " +");
        } else {
            q2a_strcat(buffer, " -");
        }

        if (findentry->type == NICKALL && findentry->addr.mask_bits == 0 && !findentry->version) {
            q2a_strcat(buffer, " ALL");
        } else {
            if (findentry->type != NICKALL) {
                q2a_strcat(buffer, " NAME");

                if (findentry->type == NICKBLANK) {
                    q2a_strcat(buffer, " BLANK");
                } else {
                    if (findentry->type == NICKLIKE) {
                        q2a_strcat(buffer, " LIKE");
                    } else if (findentry->type == NICKRE) {
                        q2a_strcat(buffer, " RE");
                    }

                    q2a_strcat(buffer, " \"");
                    q2a_strcat(buffer, findentry->nick);
                    q2a_strcat(buffer, "\"");
                }
            }

            if (findentry->addr.mask_bits != 0) {
                Q_snprintf(
                        buffer + q2a_strlen(buffer),
                        sizeof(buffer) - q2a_strlen(buffer),
                        " IP %s",
                        net_addressToString(&findentry->addr, qfalse, qfalse, qtrue)
                );
            }

            if (findentry->vpn) {
                Q_snprintf(
                        buffer + q2a_strlen(buffer),
                        sizeof(buffer) - q2a_strlen(buffer),
                        " IP VPN"
                );
            }
        }

        if (findentry->version[0] != 0) {
            Q_snprintf(
                    buffer + q2a_strlen(buffer),
                    sizeof(buffer) - q2a_strlen(buffer),
                    " VERSION %s\"%s\"",
                    (findentry->vtype == VERSION_REGEX) ? "RE " : (findentry->vtype == VERSION_LIKE) ? "LIKE " : "", findentry->version
            );
        }

        if (!findentry->exclude && findentry->password[0]) {
            q2a_strcat(buffer, " PASSWORD \"");
            q2a_strcat(buffer, findentry->password);
            q2a_strcat(buffer, "\"");
        }

        if (!findentry->exclude && findentry->maxnumberofconnects) {
            Q_snprintf(
                    buffer + q2a_strlen(buffer),
                    sizeof(buffer) - q2a_strlen(buffer),
                    " MAX %d",
                    findentry->maxnumberofconnects
            );
        }

        if (!findentry->exclude && findentry->floodinfo.chatFloodProtect) {
            Q_snprintf(
                    buffer + q2a_strlen(buffer),
                    sizeof(buffer) - q2a_strlen(buffer),
                    " FLOOD %d %d %d",
                    findentry->floodinfo.chatFloodProtectNum,
                    findentry->floodinfo.chatFloodProtectSec,
                    findentry->floodinfo.chatFloodProtectSilence
            );
        }

        if (findentry->msg) {
            q2a_strcat(buffer, " MSG \"");
            q2a_strcat(buffer, findentry->msg);
            q2a_strcat(buffer, "\"");
        }

        if (findentry->timeout) {
            Q_snprintf(
                    buffer + q2a_strlen(buffer),
                    sizeof(buffer) - q2a_strlen(buffer),
                    " TIME %g",
                    (findentry->timeout - ltime) / 60.0
            );
        }

        gi.cprintf(ent, PRINT_HIGH, "%s\n", buffer);
        addCmdQueue(client, QCMD_DISPBANS, 0, bannum, 0);
    } else {
        gi.cprintf(ent, PRINT_HIGH, "End of ban list.\n");
    }
}

void delbanRun(int startarg, edict_t *ent, int client) {
    if (gi.argc() > startarg) {
        int banToDelete = q2a_atoi(gi.argv(startarg));
        baninfo_t *findentry = banhead, *prevban = NULL;

        while (findentry) {
            if (findentry->bannum == banToDelete) {
                break;
            }

            prevban = findentry;
            findentry = findentry->next;
        }

        if (findentry) {
            unsigned int clienti;

            for (clienti = 0; clienti < maxclients->value; clienti++) {
                if (proxyinfo[clienti].baninfo == findentry) {
                    proxyinfo[clienti].baninfo = NULL;
                }
            }
            if (prevban) {
                prevban->next = findentry->next;
            } else {
                banhead = findentry->next;
            }
            if (findentry->msg) {
                gi.TagFree(findentry->msg);
            }
            gi.TagFree(findentry);
            gi.cprintf(ent, PRINT_HIGH, "Ban deleted.\n");
        } else {
            gi.cprintf(ent, PRINT_HIGH, "Ban not found.\n");
        }
    } else {
        gi.cprintf(ent, PRINT_HIGH, "No ban number supplied to delete.\n");
    }
}

void chatbanRun(int startarg, edict_t *ent, int client) {
    char *cp;
    unsigned int num, save;
    chatbaninfo_t *cnewentry;
    char savecmd[256];
    char strbuffer[256];

    if (gi.argc() <= startarg) {
        gi.cprintf(ent, PRINT_HIGH, CHATBANCMD_LAYOUT);
        return;
    }

    cp = gi.argv(startarg);
    startarg++;

    // allocate memory for ban record
    cnewentry = gi.TagMalloc(sizeof (chatbaninfo_t), TAG_LEVEL);
    cnewentry->r = 0;

    q2a_strncpy(savecmd, "CHATBAN: ", sizeof(savecmd));

    if (startContains(cp, "LIKE")) {
        if (gi.argc() <= startarg) {
            gi.cprintf(ent, PRINT_HIGH, "UpTo: %s\n", savecmd);
            gi.cprintf(ent, PRINT_HIGH, CHATBANCMD_LAYOUT);
            gi.TagFree(cnewentry);
            return;
        }

        cp = gi.argv(startarg);
        startarg++;

        q2a_strcat(savecmd, "LIKE ");

        cnewentry->type = CHATLIKE;

    }//re?
    else if (startContains(cp, "RE")) {
        if (gi.argc() <= startarg) {
            gi.cprintf(ent, PRINT_HIGH, "UpTo: %s\n", savecmd);
            gi.cprintf(ent, PRINT_HIGH, CHATBANCMD_LAYOUT);
            gi.TagFree(cnewentry);
            return;
        }

        cp = gi.argv(startarg);
        startarg++;


        q2a_strcat(savecmd, "RE ");

        cnewentry->type = CHATRE;
    } else {
        cnewentry->type = CHATLIKE;
        q2a_strcat(savecmd, "LIKE ");
    }

    q2a_strcat(savecmd, "\"");
    q2a_strcat(savecmd, cp);
    q2a_strcat(savecmd, "\" ");

    // copy chat
    cp = processstring(cnewentry->chat, cp, sizeof (cnewentry->chat) - 1, 0);

    if (cnewentry->type == CHATRE) { // compile RE
        q2a_strncpy(strbuffer, cnewentry->chat, sizeof(strbuffer));
        q_strupr(strbuffer);
        cnewentry->r = re_compile(strbuffer);
        if (!cnewentry->r) {
            gi.cprintf(ent, PRINT_HIGH, "UpTo: %s\n", savecmd);
            gi.cprintf(ent, PRINT_HIGH, CHATBANCMD_LAYOUT);
            gi.TagFree(cnewentry);
            return;
        }
    }

    if (gi.argc() <= startarg) {
        cp = "";
    } else {
        cp = gi.argv(startarg);
        startarg++;
    }

    // get MSG
    if (startContains(cp, "MSG")) {
        if (gi.argc() <= startarg) {
            gi.cprintf(ent, PRINT_HIGH, "UpTo: %s\n", savecmd);
            gi.cprintf(ent, PRINT_HIGH, CHATBANCMD_LAYOUT);
            gi.TagFree(cnewentry);
            return;
        }

        cp = gi.argv(startarg);
        startarg++;

        q2a_strcat(savecmd, "MSG ");
        q2a_strcat(savecmd, "\"");
        q2a_strcat(savecmd, cp);
        q2a_strcat(savecmd, "\" ");

        // copy MSG
        processstring(buffer2, cp, sizeof (buffer2) - 1, '\"');

        num = q2a_strlen(buffer2);

        if (num) {
            cnewentry->msg = gi.TagMalloc(num + 1, TAG_LEVEL);
            q2a_strncpy(cnewentry->msg, buffer2, num + 1);
        } else {
            cnewentry->msg = NULL;
        }

        if (gi.argc() <= startarg) {
            cp = "";
        } else {
            cp = gi.argv(startarg);
            startarg++;
        }
    } else {
        cnewentry->msg = NULL;
    }

    // get Save?
    if (startContains(cp, "SAVE")) {
        if (gi.argc() <= startarg) {
            cp = "";
        } else {
            cp = gi.argv(startarg);
            startarg++;
        }

        cnewentry->loadType = LT_PERM;

        if (startContains(cp, "MOD")) {
            if (gi.argc() <= startarg) {
                cp = "";
            } else {
                cp = gi.argv(startarg);
                startarg++;
            }

            save = 2;
        } else {
            save = 1;
        }
    } else {
        cnewentry->loadType = LT_TEMP;
        save = 0;
    }

    if (*cp != 0) {
        // something is wrong...
        if (cnewentry->msg) {
            gi.TagFree(cnewentry->msg);
        }
        gi.TagFree(cnewentry);
        gi.cprintf(ent, PRINT_HIGH, "UpTo: %s\n", savecmd);
        gi.cprintf(ent, PRINT_HIGH, CHATBANCMD_LAYOUT);
        return;
    }

    // we have the chat ban record...
    // insert at the head of the correct list.
    cnewentry->bannum = chatBanNumUpto;
    chatBanNumUpto++;

    cnewentry->next = chatbanhead;
    chatbanhead = cnewentry;

    gi.cprintf(ent, PRINT_HIGH, "Chatban added.\n");

    if (save) {
        FILE *banlistfptr;

        if (save == 1) {
            q2a_strncpy(buffer, configfile_ban->string, sizeof(buffer));
        } else {
            Q_snprintf(buffer, sizeof(buffer), "%s/%s", moddir, configfile_ban->string);
        }

        banlistfptr = fopen(buffer, "at");
        if (!banlistfptr) {
            gi.cprintf(ent, PRINT_HIGH, "Error opening banfile!\n");
        } else {
            fprintf(banlistfptr, "%s\n", savecmd);
            fclose(banlistfptr);

            gi.cprintf(ent, PRINT_HIGH, "Chatban stored.\n");
        }
    }
}

int checkCheckIfChatBanned(char *txt) {
    chatbaninfo_t *checkentry = chatbanhead;
    char strbuffer[4096];

    // filter out characters that are disallowed.
    if (filternonprintabletext) {
        char *cp = txt;

        while (*cp) {
            if (!isprint(*cp)) {
                *cp = ' ';
            }

            *cp++;
        }
    }


    if (!ChatBanning_Enable) {
        return 0;
    }

    currentBanMsg = defaultChatBanMsg;

    while (checkentry) {
        switch (checkentry->type) {
            case CHATLIKE:
                if (!stringContains(txt, checkentry->chat)) {
                    checkentry = checkentry->next;
                    continue;
                }
                break;

            case CHATRE:
                q2a_strncpy(strbuffer, txt, sizeof(strbuffer));
                q_strupr(strbuffer);
                int len;
                if (re_matchp(checkentry->r, strbuffer, &len) != 0) {
                    checkentry = checkentry->next;
                    continue;
                }
                break;
        }


        // ok, a ban situation..
        if (checkentry->msg) {
            currentBanMsg = checkentry->msg;
        }

        return 1;
    }

    return 0;
}

void listchatbansRun(int startarg, edict_t *ent, int client) {
    addCmdQueue(client, QCMD_DISPCHATBANS, 0, 0, 0);

    gi.cprintf(ent, PRINT_HIGH, "Start Chat Ban List:\n");
}

void displayNextChatBan(edict_t *ent, int client, long chatbannum) {
    long upto = chatbannum;
    chatbaninfo_t *findentry = chatbanhead;

    chatbannum++;

    while (findentry && upto) {
        findentry = findentry->next;
        upto--;
    }

    if (findentry) {
        if (findentry->loadType == LT_TEMP) {
            Q_snprintf(buffer, sizeof(buffer), " (%ld, Temporary) CHATBAN:", findentry->bannum);
        } else {
            Q_snprintf(buffer, sizeof(buffer), " (%ld, Permanent) CHATBAN:", findentry->bannum);
        }

        if (findentry->type == CHATLIKE) {
            q2a_strcat(buffer, " LIKE");
        } else if (findentry->type == CHATRE) {
            q2a_strcat(buffer, " RE");
        }

        q2a_strcat(buffer, " \"");
        q2a_strcat(buffer, findentry->chat);
        q2a_strcat(buffer, "\"");

        if (findentry->msg) {
            q2a_strcat(buffer, " MSG \"");
            q2a_strcat(buffer, findentry->msg);
            q2a_strcat(buffer, "\"");
        }

        gi.cprintf(ent, PRINT_HIGH, "%s\n", buffer);
        addCmdQueue(client, QCMD_DISPCHATBANS, 0, chatbannum, 0);
    } else {
        gi.cprintf(ent, PRINT_HIGH, "End Chat Ban List\n");
    }
}

void delchatbanRun(int startarg, edict_t *ent, int client) {
    if (gi.argc() > startarg) {
        int banToDelete = q2a_atoi(gi.argv(startarg));
        chatbaninfo_t *findentry = chatbanhead, *prevban = NULL;

        while (findentry) {
            if (findentry->bannum == banToDelete) {
                break;
            }

            prevban = findentry;
            findentry = findentry->next;
        }

        if (findentry) {
            if (prevban) {
                prevban->next = findentry->next;
            } else {
                chatbanhead = findentry->next;
            }
            if (findentry->msg) {
                gi.TagFree(findentry->msg);
            }
            gi.TagFree(findentry);
            gi.cprintf(ent, PRINT_HIGH, "Chat Ban deleted.\n");
        } else {
            gi.cprintf(ent, PRINT_HIGH, "Chat Ban not found.\n");
        }
    } else {
        gi.cprintf(ent, PRINT_HIGH, "No chat ban number supplied to delete.\n");
    }
}

/**
 *
 */
qboolean parseBanFileContents(unsigned char *data) {
    unsigned int uptoLine = 0;
    size_t len;
    unsigned char *start = data;

    if (!data) {
        return qfalse;
    }

    len = strlen(data);
    while ((data - start) < len) {
        SKIPBLANK(data);
        uptoLine++;

        if (!(data[0] == ';' || data[0] == '\n' || isBlank(data))) {
            if (startContains(data, "BAN:")) {
                data = ban_parseBan(data);
            } else if (startContains(data, "CHATBAN:")) {
                data = ban_parseChatban(data);
            } else if (startContains(data, "INCLUDE:")) {
                data = ban_parseInclude(data);
            } else {
                gi.cprintf(NULL, PRINT_HIGH,
                        "[q2admin] invalid ban at line %d, ignoring\n",
                        uptoLine);
                // just jump to the next line and try again
                while (data[0] != '\n' && data[0] != NULL) {
                    data++;
                }
                continue;
            }
        }
        data++;
    }
    return qtrue;
}

/**
 * Parse a line starting with "BAN:..."
 *
 * Returns a pointer to the end of the current line
 */
char *ban_parseBan(unsigned char *cp) {
    baninfo_t *newentry;
    qboolean all;
    qboolean like, re;
    char strbuffer[256];
    unsigned char *tempcp;
    char ipstr[INET6_ADDRSTRLEN];
    qboolean allver;
    int num;
    unsigned int i;

    q2a_memset(strbuffer, 0, sizeof(strbuffer));
    newentry = gi.TagMalloc(sizeof(baninfo_t), TAG_LEVEL);
    newentry->loadType = LT_PERM;
    newentry->timeout = 0.0;
    newentry->r = 0;
    cp += 4;
    SKIPBLANK(cp);

    // get +/-
    if (*cp == '-') {
        cp++;
        SKIPBLANK(cp);
        newentry->exclude = qtrue;
    } else if (*cp == '+') {
        cp++;
        SKIPBLANK(cp);
        newentry->exclude = qfalse;
    } else {
        newentry->exclude = qtrue;
    }

    if (startContains(cp, "ALL")) {
        newentry->type = NICKALL;
        newentry->maxnumberofconnects = 0;
        newentry->numberofconnects = 0;
        newentry->msg = NULL;
        q2a_memset(&newentry->addr, 0, sizeof(netadr_t));
        all = qtrue;

        cp += 3;
        SKIPBLANK(cp);
    } else {
        all = qfalse;

        // Name:
        if (startContains(cp, "NAME")) {
            cp += 4;
            SKIPBLANK(cp);

            // Like?
            if (startContains(cp, "LIKE")) {
                cp += 4;
                like = qtrue;
                re = qfalse;
                SKIPBLANK(cp);

            }//re?
            else if (startContains(cp, "RE")) {
                cp += 2;
                like = qfalse;
                re = qtrue;
                SKIPBLANK(cp);
            } else {
                like = qfalse;
                re = qfalse;
            }

            // BLANK or ALL or name
            if (startContains(cp, "BLANK")) {
                cp += 5;
                newentry->type = NICKBLANK;
            } else if (startContains(cp, "ALL")) {
                cp += 3;
                newentry->type = NICKALL;
            } else if (*cp == '\"') {
                if (like) {
                    newentry->type = NICKLIKE;
                } else if (re) {
                    newentry->type = NICKRE;
                } else {
                    newentry->type = NICKEQ;
                }

                cp++;
                cp = processstring(newentry->nick, cp, sizeof (newentry->nick) - 1, '\"');

                // make sure you are at the end quote
                while (*cp && *cp != '\"') {
                    cp++;
                }
                cp++;
            } else {
                newentry->type = NOTUSED;
            }

            if (newentry->type == NICKRE) { // compile RE
                q2a_strncpy(strbuffer, newentry->nick, sizeof(strbuffer));
                q_strupr(strbuffer);
                newentry->r = re_compile(strbuffer);
                if (!newentry->r) {
                    newentry->type = NICKEQ;
                }
            }
            SKIPBLANK(cp);
        } else {
            newentry->type = NICKALL;
        }

        // get ip address
        q2a_memset(&newentry->addr, 0, sizeof(netadr_t));

        if (startContains(cp, "IP")) {
            cp += 2;
            SKIPBLANK(cp);

            if (startContains(cp, "VPN")) {
                newentry->vpn = qtrue;
                cp += 3;
            } else {
                if (isxdigit(*cp)) {
                    tempcp = cp;
                    // find the end of the IP string
                    while (!isspace(*tempcp)) {
                        tempcp++;
                    }
                    q2a_memset(ipstr, 0, sizeof(ipstr));
                    q2a_memcpy(ipstr, cp, (tempcp-cp));
                    newentry->addr = net_parseIPAddressMask(ipstr);
                    cp = tempcp;
                }
            }
            SKIPBLANK(cp);
        }

        if (startContains(cp, "VERSION")) {
            cp += 7;
            SKIPBLANK(cp);
            if (startContains(cp, "LIKE")) {
                newentry->vtype = VERSION_LIKE;
                cp += 4;
                SKIPBLANK(cp);
            } else if (startContains(cp, "RE")) {
                newentry->vtype = VERSION_REGEX;
                cp += 2;
                SKIPBLANK(cp);
            } else {
                newentry->vtype = VERSION_EQUALS;
            }

            q2a_memset(newentry->version, 0, sizeof(newentry->version));
            cp++; // eat the opening quote
            processstring(newentry->version, cp, sizeof(newentry->version)-1, '\"');
            cp += strlen(newentry->version);
            allver = qfalse;
            if (newentry->vtype == VERSION_REGEX) {
                q2a_strncpy(strbuffer, newentry->version, sizeof(strbuffer));
                q_strupr(strbuffer);
                newentry->vr = re_compile(strbuffer);
                if (!newentry->vr) {
                    gi.TagFree(newentry);
                }
            }
        } else {
            allver = qtrue;
        }
    }

    // get PASSWORD
    if (!newentry->exclude && startContains(cp, "PASSWORD")) {
        cp += 8;
        SKIPBLANK(cp);
        if (*cp == '\"') {
            cp++;
            cp = processstring(newentry->password, cp, sizeof (newentry->password) - 1, '\"');

            // make sure you are at the end quote
            while (*cp && *cp != '\"') {
                cp++;
            }
            cp++;
        } else {
            newentry->type = NOTUSED;
        }
        SKIPBLANK(cp);
    } else {
        newentry->password[0] = 0;
    }

    // get MAX
    if (!newentry->exclude && startContains(cp, "MAX")) {
        cp += 3;
        SKIPBLANK(cp);
        newentry->maxnumberofconnects = q2a_atoi(cp);
        while (isdigit(*cp)) {
            cp++;
        }
        SKIPBLANK(cp);
    } else {
        newentry->maxnumberofconnects = 0;
    }
    newentry->numberofconnects = 0;

    // get FLOOD
    if (!newentry->exclude && startContains(cp, "FLOOD")) {
        cp += 5;
        SKIPBLANK(cp);
        newentry->floodinfo.chatFloodProtectNum = q2a_atoi(cp);
        while (isdigit(*cp)) {
            cp++;
        }
        SKIPBLANK(cp);
        newentry->floodinfo.chatFloodProtectSec = q2a_atoi(cp);
        while (isdigit(*cp)) {
            cp++;
        }
        SKIPBLANK(cp);
        newentry->floodinfo.chatFloodProtectSilence = q2a_atoi(cp);
        if (*cp == '-') {
            cp++;
        }
        while (isdigit(*cp)) {
            cp++;
        }
        SKIPBLANK(cp);
        if (newentry->floodinfo.chatFloodProtectNum && newentry->floodinfo.chatFloodProtectSec) {
            newentry->floodinfo.chatFloodProtect = qtrue;
        } else {
            newentry->floodinfo.chatFloodProtect = qfalse;
        }
    } else {
        newentry->floodinfo.chatFloodProtect = qfalse;
    }

    // get MSG
    if (startContains(cp, "MSG")) {
        cp += 3;
        SKIPBLANK(cp);
        cp++;
        cp = processstring(buffer2, cp, sizeof (buffer2) - 1, '\"');

        // make sure you are at the end quote
        while (*cp && *cp != '\"') {
            cp++;
        }
        cp++;
        SKIPBLANK(cp);
        num = q2a_strlen(buffer2);
        if (num) {
            newentry->msg = gi.TagMalloc(num + 1, TAG_LEVEL);
            q2a_strncpy(newentry->msg, buffer2, num + 1);
        } else {
            newentry->msg = NULL;
        }
    } else {
        newentry->msg = NULL;
    }

    // do you have a valid ban record?
    if (newentry->type == NOTUSED ||
            (!all && newentry->type == NICKALL && newentry->addr.mask_bits == 0 && newentry->maxnumberofconnects == 0 && (!allver && !newentry->version)) ||
            (newentry->type == NICKRE && !newentry->r)) {
        // no, abort
        if (newentry->msg) {
            gi.TagFree(newentry->msg);
        }
        gi.TagFree(newentry);
        gi.dprintf("Error loading BAN\n");
    } else {
        // we have the ban record...
        // insert at the head of the correct list.
        newentry->bannum = banNumUpto;
        banNumUpto++;

        newentry->next = banhead;
        banhead = newentry;
    }
    return cp;
}

/**
 * Parse a CHATBAN: message in a ban file.
 */
char *ban_parseChatban(unsigned char *cp) {
    chatbaninfo_t *cnewentry;
    char strbuffer[256];
    int num;

    q2a_memset(strbuffer, 0, sizeof(strbuffer));
    cnewentry = gi.TagMalloc(sizeof(chatbaninfo_t), TAG_LEVEL);
    cnewentry->loadType = LT_PERM;
    cnewentry->r = 0;

    cp += 8;
    SKIPBLANK(cp);

    if (startContains(cp, "LIKE")) {
        cp += 4;
        SKIPBLANK(cp);
        cnewentry->type = CHATLIKE;
    } else if (startContains(cp, "RE")) {
        cp += 2;
        SKIPBLANK(cp);
        cnewentry->type = CHATRE;
    } else {
        cnewentry->type = CHATLIKE;
    }

    if (*cp == '\"') {
        cp++;
        cp = processstring(cnewentry->chat, cp, sizeof (cnewentry->chat) - 1, '\"');

        // make sure you are at the end quote
        while (*cp && *cp != '\"') {
            cp++;
        }
        cp++;
        SKIPBLANK(cp);

        if (cnewentry->type == CHATRE) { // compile RE
            q2a_strncpy(strbuffer, cnewentry->chat, sizeof(strbuffer));
            q_strupr(strbuffer);
            cnewentry->r = re_compile(strbuffer);
            if (!cnewentry->r) {
                cnewentry->type = CHATLIKE;
            }
        }
    } else {
        cnewentry->type = CNOTUSED;
    }

    // get MSG
    if (startContains(cp, "MSG")) {
        cp += 3;
        SKIPBLANK(cp);
        cp++; // swallow the "
        cp = processstring(buffer2, cp, sizeof (buffer2) - 1, '\"');

        // make sure you are at the end quote
        while (*cp && *cp != '\"') {
            cp++;
        }
        cp++;
        SKIPBLANK(cp);
        num = q2a_strlen(buffer2);

        if (num) {
            cnewentry->msg = gi.TagMalloc(num + 1, TAG_LEVEL);
            q2a_strncpy(cnewentry->msg, buffer2, num + 1);
        } else {
            cnewentry->msg = NULL;
        }
    } else {
        cnewentry->msg = NULL;
    }

    // do you have a valid ban record?
    if (cnewentry->type == CNOTUSED || (cnewentry->type == CHATRE && !cnewentry->r)) {
        // no, abort
        if (cnewentry->msg) {
            gi.TagFree(cnewentry->msg);
        }
        gi.TagFree(cnewentry);
        gi.dprintf("[q2admin] invalid chatban, syntax: %s\n", CHATBANFILE_LAYOUT);
    } else {
        // we have the ban record...
        // insert at the head of the correct list.
        cnewentry->bannum = chatBanNumUpto;
        chatBanNumUpto++;

        cnewentry->next = chatbanhead;
        chatbanhead = cnewentry;
    }
    return cp;
}

/**
 * Parse an INCLUDE: message in a ban file.
 */
char *ban_parseInclude(unsigned char *in) {
    char strbuffer[256];

    q2a_memset(strbuffer, 0, sizeof(strbuffer));
    in += 8;        // swallow "INCLUDE:"
    SKIPBLANK(in);
    if (*in == '\"') {
        in++;
        in = processstring(strbuffer, in, sizeof(strbuffer) - 1, '\"');
        if (strbuffer[0]) {
            if (startContains(strbuffer, "http")) {
                gi.cprintf(NULL, PRINT_HIGH, "[q2admin] reading remote ban file: %s\n", strbuffer);
                ReadRemoteBanFile(strbuffer);
            } else {
                if (validatePath(strbuffer) == PATH_INVALID) {
                    gi.cprintf(NULL, PRINT_HIGH, "[q2admin] invalid path in ban config: %s\n", strbuffer);
                } else {
                    gi.cprintf(NULL, PRINT_HIGH, "[q2admin] reading included ban file: %s\n", strbuffer);
                    ReadBanFile(strbuffer);
                }
            }
        } else {
            gi.dprintf("[q2admin] ban parse error, syntax: INCLUDE: \"<file|url>\"\n");
        }
    } else {
        gi.dprintf("[q2admin] ban parse error, syntax: INCLUDE: \"<file|url>\"\n");
    }
    return in;
}
