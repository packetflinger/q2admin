/*
Copyright (C) 2016 Joe Reid (joe@joereid.com)

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

game_import_t gi;       // server access from q2admin
game_export_t ge;       // q2admin access from server
game_export_t *ge_mod;  // game access from q2admin

cvar_t *rcon_password;
cvar_t *gamedir;
cvar_t *maxclients;
cvar_t *logfile;
cvar_t *rconpassword;
cvar_t *port;
cvar_t *serverbindip;
cvar_t *q2aconfig;
cvar_t *q2adminbantxt;
cvar_t *q2adminbanremotetxt;
cvar_t *q2adminbanremotetxt_enable;
cvar_t *q2adminanticheat_enable;
cvar_t *q2adminanticheat_file;
cvar_t *q2adminhashlist_enable;
cvar_t *q2adminhashlist_dir;

// for per-map entity substitutions
cvar_t *tune_spawn_railgun;
cvar_t *tune_spawn_bfg;
cvar_t *tune_spawn_quad;
cvar_t *tune_spawn_invulnerability;
cvar_t *tune_spawn_powershield;
cvar_t *tune_spawn_megahealth;
cvar_t *tune_spawn_rocketlauncher;
cvar_t *tune_spawn_hyperblaster;
cvar_t *tune_spawn_grenadelauncher;
cvar_t *tune_spawn_chaingun;
cvar_t *tune_spawn_machinegun;
cvar_t *tune_spawn_supershotgun;
cvar_t *tune_spawn_shotgun;
cvar_t *tune_spawn_machinegun;
cvar_t *tune_spawn_grenades;

cvar_t *configfile_ban;
cvar_t *configfile_bypass;
cvar_t *configfile_cloud;
cvar_t *configfile_cvar;
cvar_t *configfile_disable;
cvar_t *configfile_flood;
cvar_t *configfile_login;
cvar_t *configfile_log;
cvar_t *configfile_rcon;
cvar_t *configfile_spawn;
cvar_t *configfile_vote;

qboolean quake2dirsupport = qtrue;
qboolean fpsFloodExempt = qfalse;

char dllname[256];
char gamelibrary[MAX_QPATH] = {""}; // forward library name from config file
char gmapname[MAX_QPATH];
char version[256];

char *finalentities;

qboolean cloud_enabled           = qfalse;
char     cloud_address[256]      = "127.0.0.1";
int      cloud_port              = 9988;
qboolean cloud_encryption        = qtrue;
char     cloud_privatekey[256]   = "private.pem";
char     cloud_publickey[256]    = "public.pem";
char     cloud_serverkey[256]    = "server.pem";
char     cloud_uuid[37]          = "00000000-0000-0000-0000-000000000000";
int      cloud_flags             = 4095; // 12 bits worth
char     cloud_dns[3]            = "64"; // ipv6 first then ipv4
char     cloud_cmd_teleport[25]  = "!teleport";
char     cloud_cmd_invite[25]    = "!invite";
char     cloud_cmd_seen[25]      = "!seen";
char     cloud_cmd_whois[25]     = "!whois";

qboolean vpn_enable              = qfalse;
qboolean vpn_kick                = qtrue;
char     vpn_api_key[33]         = "";

int      ip_limit                = 0;

char     http_cacert_path[256]   = "/etc/ssl/certs";
qboolean http_debug              = qfalse;
qboolean http_enable             = qtrue;
qboolean http_verifyssl          = qtrue;

ip_cache_t *ipcache;

int USERINFOCHANGE_TIME = 60;
int USERINFOCHANGE_COUNT = 40;
int client_map_cfg = 6;
int gl_driver_max_changes = 3;
int gl_driver_check = 0;
int max_pmod_noreply = 2;
int msec_int = 5;
int msec_kick_on_bad = 2;
int msec_max = 6000;
int speedbot_check_type = 3;

char client_msg[256];
char serverip[256] = {""};
char lanip[256] = {""};

qboolean do_franck_check = qtrue;
qboolean q2a_command_check = qfalse;
qboolean do_vid_restart = qfalse;
qboolean private_command_kick = qfalse;

qboolean dllloaded = qfalse;

qboolean zbotdetect = qtrue;
qboolean mapcfgexec = qfalse;
qboolean checkClientIpAddress = qtrue;

qboolean nameChangeFloodProtect = qfalse;
int nameChangeFloodProtectNum = 5;
int nameChangeFloodProtectSec = 2;
int nameChangeFloodProtectSilence = 10;
char nameChangeFloodProtectMsg[256];

qboolean skinChangeFloodProtect = qfalse;
int skinChangeFloodProtectNum = 5;
int skinChangeFloodProtectSec = 2;
int skinChangeFloodProtectSilence = 10;
char skinChangeFloodProtectMsg[256];

struct chatflood_s floodinfo = {
    qfalse, 5, 2, 10
};

char chatFloodProtectMsg[256];


qboolean disconnectuser = qtrue;
qboolean displayzbotuser = qtrue;
int numofdisplays = 4;
char zbotuserdisplay[256];
char timescaleuserdisplay[256];
char hackuserdisplay[256];
char skincrashmsg[256];
char defaultreconnectmessage[256];
qboolean displaynamechange = qtrue;
qboolean disconnectuserimpulse = qfalse;
int maximpulses = 1;


qboolean displayimpulses = qfalse;

//r1ch 2005-01-26 disable hugely buggy commands BEGIN
/*qboolean play_team_enable = qfalse;
qboolean play_all_enable = qfalse;
qboolean play_person_enable = qfalse;*/
//r1ch 2005-01-26 disable hugely buggy commands END

qboolean say_person_enable = qfalse;
qboolean say_group_enable = qfalse;
qboolean extendedsay_enable = qfalse;
qboolean spawnentities_enable = qfalse;
qboolean spawnentities_internal_enable = qfalse;
qboolean vote_enable = qfalse;
qboolean consolechat_disable = qfalse;
qboolean gamemaptomap = qfalse;
qboolean banOnConnect = qtrue;
qboolean lockDownServer = qfalse;
qboolean printmessageonplaycmds = qtrue;

int randomwaitreporttime = 55;

int maxMsgLevel = 3;

qboolean serverinfoenable = qtrue;

char zbotmotd[256];
char motd[4096];

int maxrateallowed = 0;
int minrateallowed = 0;
int maxfpsallowed = 0;
int minfpsallowed = 0;

char buffer[0x10000];
char buffer2[256];

char adminpassword[256];

char customServerCmd[256];
char customClientCmd[256];
char customClientCmdConnect[256];
char customServerCmdConnect[256];

char clientVoteCommand[256];

char reconnect_address[256] = {0};
int reconnect_time = 60;
int reconnect_checklevel = 0;

int logfilecheckcount;

proxyinfo_t *proxyinfo;
proxyinfo_t *proxyinfoBase;
proxyreconnectinfo_t *reconnectproxyinfo;

reconnect_info* reconnectlist;
retrylist_info* retrylist;
int maxReconnectList = 0;
int maxretryList = 0;

int lframenum;
float ltime;

int proxy_bwproxy = 1;
int proxy_nitro2 = 1;

int q2adminrunmode = 100;
int maxclientsperframe = 100;
int framesperprocess = 0;


qboolean cl_pitchspeed_display = qtrue;
qboolean cl_pitchspeed_enable = qfalse;
qboolean cl_pitchspeed_kick = qfalse;
char cl_pitchspeed_kickmsg[256];

qboolean cl_anglespeedkey_display = qtrue;
qboolean cl_anglespeedkey_enable = qfalse;
qboolean cl_anglespeedkey_kick = qfalse;
char cl_anglespeedkey_kickmsg[256];

qboolean filternonprintabletext = qfalse;

char lockoutmsg[256];

char com_token[MAX_TOKEN_CHARS];

/*
==============
COM_Parse
 
Parse a token out of a string
==============
 */
char *COM_Parse(char **data_p, char **command_p)
{
    int c;
    int len;
    char *data;

    data = *data_p;
    len = 0;
    com_token[0] = 0;

    if (!data) {
        *data_p = NULL;
        return"";
    }

    // skip whitespace
skipwhite:
    while ((c = *data) <= ' ') {
        if (c == 0) {
            *data_p = NULL;
            return "";
        }
        data++;
    }

    // skip // comments
    if (c == '/' && data[1] == '/') {
        while (*data && *data != '\n') {
            data++;
        }

        goto skipwhite;
    }

    // handle quoted strings specially
    if (c == '\"') {
        data++;

        if (command_p) {
            *command_p = data;
        }

        while (1) {
            c = *data++;
            if (c == '\"' || !c) {
                com_token[len] = 0;
                *data_p = data;
                return com_token;
            }
            if (len < MAX_TOKEN_CHARS) {
                com_token[len] = c;
                len++;
            }
        }
    }

    if (command_p) {
        *command_p = data;
    }

    // parse a regular word
    do {
        if (len < MAX_TOKEN_CHARS) {
            com_token[len] = c;
            len++;
        }
        data++;
        c = *data;
    } while (c > 32);

    if (len == MAX_TOKEN_CHARS) {
        len = 0;
    }
    com_token[len] = 0;

    *data_p = data;
    return com_token;
}

/**
 * Get the IPv4/IPv6 address from the userinfo string
 */
char *FindIpAddressInUserInfo(char *userinfo, qboolean *userInfoOverflow) {
    char *ip = Info_ValueForKey(userinfo, "ip");

    if (userInfoOverflow) {
        *userInfoOverflow = qfalse;
    }
    if (*ip == 0) {
        char *ipuserinfo = userinfo + q2a_strlen(userinfo);
        while (ipuserinfo > userinfo && *ipuserinfo != '\\') { // find last "\\"
            ipuserinfo--;
        }
        if (ipuserinfo - 3 >= userinfo &&
                *(ipuserinfo - 3) == '\\' &&
                *(ipuserinfo - 2) == 'i' &&
                *(ipuserinfo - 1) == 'p') {
            if (userInfoOverflow) {
                *userInfoOverflow = qtrue;
            }
            return ipuserinfo + 1;
        }
    }
    return ip;
}

/**
 * Called when the server is initialized
 */
void InitGame(void) {
    int i;

    profile_init(1);
    profile_init(2);

    proxyinfo = NULL;

    if (!dllloaded) {
        return;
    }

    if (q2adminrunmode < 100) {
        gi.dprintf("(Q2Admin runlevel %d)\n", q2adminrunmode);
    }

    if (q2adminrunmode == 0) {
        ge_mod->Init();
        G_MergeEdicts();
        return;
    }

    profile_start(1);
    profile_start(2);
    
    /* Be carefull with all functions called from this one (like dprintf_internal) 
    to not use proxyinfo pointer because it's not initialized yet. -Harven */
    ge_mod->Init(); 
    
    profile_stop(2, "mod->InitGame", 0, NULL);

    G_MergeEdicts();

    maxclients = gi.cvar("maxclients", "4", 0);
    logfile = gi.cvar("logfile", "0", 0);
    rconpassword = gi.cvar("rcon_password", "", 0);
    proxyinfoBase = G_Malloc((maxclients->value + 1) * sizeof(proxyinfo_t));
    q2a_memset(proxyinfoBase, 0x0, (maxclients->value + 1) * sizeof(proxyinfo_t));
    proxyinfo = proxyinfoBase;
    proxyinfo += 1;
    proxyinfo[-1].inuse = 1;

    reconnectproxyinfo = G_Malloc(maxclients->value * sizeof(proxyreconnectinfo_t));
    q2a_memset(reconnectproxyinfo, 0x0, maxclients->value * sizeof(proxyreconnectinfo_t));

    reconnectlist = (reconnect_info *) G_Malloc(maxclients->value * sizeof(reconnect_info));
    maxReconnectList = 0;

    retrylist = (retrylist_info *) G_Malloc(maxclients->value * sizeof (retrylist_info));
    maxretryList = 0;

    logEvent(LT_SERVERINIT, 0, NULL, NULL, 0, 0.0);

    banhead = NULL;
    motd[0] = 0;

    for (i = -1; i < maxclients->value; i++) {
        proxyinfo[i].speedfreeze = 0;
        proxyinfo[i].msec_bad = 0;
        proxyinfo[i].enteredgame = 0;
        proxyinfo[i].msec_start = 0;
        proxyinfo[i].msec_count = 0;
        proxyinfo[i].msec_last = 0;
        proxyinfo[i].show_fps = qfalse;
        proxyinfo[i].frames_count = 0;
        proxyinfo[i].timescale = 0;
        proxyinfo[i].q2a_admin = 0;
        proxyinfo[i].q2a_bypass = 0;
        proxyinfo[i].clientcommand = 0;
        proxyinfo[i].userinfo_changed_count = 0;
        proxyinfo[i].userinfo_changed_start = ltime;
        proxyinfo[i].pmod_noreply_count = 0;
        proxyinfo[i].pcmd_noreply_count = 0;
        proxyinfo[i].private_command = 0;
        proxyinfo[i].pmod = 0;
        proxyinfo[i].pver = 0;
        proxyinfo[i].done_server_and_blocklist = 0;
        proxyinfo[i].cmdlist = 0;
        proxyinfo[i].cmdlist_timeout = 0;
        proxyinfo[i].newcmd_timeout = 0;
        proxyinfo[i].pmodver = 0;
        proxyinfo[i].gl_driver[0] = 0;
        proxyinfo[i].gl_driver_changes = 0;
        proxyinfo[i].vid_restart = qfalse;
        proxyinfo[i].userid = -1;
        proxyinfo[i].inuse = 0;
        proxyinfo[i].admin = 0;
        proxyinfo[i].clientcommand = 0;
        proxyinfo[i].stuffFile = 0;
        proxyinfo[i].floodinfo.chatFloodProtect = qfalse;
        proxyinfo[i].impulsesgenerated = 0;
        proxyinfo[i].retries = 0;
        proxyinfo[i].rbotretries = 0;
        proxyinfo[i].charindex = 0;
        proxyinfo[i].teststr[0] = 0;
        proxyinfo[i].cl_pitchspeed = 0;
        proxyinfo[i].cl_anglespeedkey = 0.0;
        proxyinfo[i].votescast = 0;
        proxyinfo[i].votetimeout = 0;
        proxyinfo[i].checked_hacked_exe = 0;
        proxyinfo[i].ent = 0;

        removeClientCommands(i);
    }

    ipcache = 0;

    Read_Admin_cfg();

    if (whois_active) {
        whois_details = G_Malloc(whois_active * sizeof (user_details));
        memset(whois_details, 0, whois_active * sizeof (user_details));
        gi.dprintf("Reading whois file...\n");
        whois_read_file();
    }

    finalentities = G_Malloc(0xffff);

    profile_stop(1, "q2admin->InitGame", 0, NULL);

    HTTP_Init();

    CA_Init();
}

/**
 * Substitute a single edict in the entities string.
 *
 * newents: the new entity string (the destination)
 * sub: the replacement (supplied via cvar)
 * needle: what we're looking for
 * token: what we have
 * found: a match was found and substituted
 */
void SubstituteEntity(char *newents, cvar_t *sub, char *needle, char *token, qboolean *found) {
    if (sub->string[0] && (!Q_stricmp(needle, token))) {
        q2a_strcat(newents, va("\"%s\"\n", sub->string));
        gi.cprintf(NULL, PRINT_HIGH, "Entity swap: %s > %s\n", needle, sub->string);
        *found = qtrue;
    }
}
/**
 * Make a new entity string making any needed substitutions based on cvars
 */
void SubstituteEntities(char *newents, char *oldents) {
    qboolean replaced;

    tune_spawn_railgun = gi.cvar("tune_spawn_railgun", "", CVAR_GENERAL);
    tune_spawn_bfg = gi.cvar("tune_spawn_bfg", "", CVAR_GENERAL);
    tune_spawn_quad = gi.cvar("tune_spawn_quad", "", CVAR_GENERAL);
    tune_spawn_invulnerability = gi.cvar("tune_spawn_invulnerability", "", CVAR_GENERAL);
    tune_spawn_powershield = gi.cvar("tune_spawn_powershield", "", CVAR_GENERAL);
    tune_spawn_megahealth = gi.cvar("tune_spawn_megahealth", "", CVAR_GENERAL);
    tune_spawn_rocketlauncher = gi.cvar("tune_spawn_rocketlauncher", "", CVAR_GENERAL);
    tune_spawn_hyperblaster = gi.cvar("tune_spawn_hyperblaster", "", CVAR_GENERAL);
    tune_spawn_grenadelauncher = gi.cvar("tune_spawn_grenadelauncher", "", CVAR_GENERAL);
    tune_spawn_chaingun = gi.cvar("tune_spawn_chaingun", "", CVAR_GENERAL);
    tune_spawn_machinegun = gi.cvar("tune_spawn_machinegun", "", CVAR_GENERAL);
    tune_spawn_supershotgun = gi.cvar("tune_spawn_supershotgun", "", CVAR_GENERAL);
    tune_spawn_shotgun = gi.cvar("tune_spawn_shotgun", "", CVAR_GENERAL);
    tune_spawn_machinegun = gi.cvar("tune_spawn_machinegun", "", CVAR_GENERAL);
    tune_spawn_grenades = gi.cvar("tune_spawn_grenades ", "", CVAR_GENERAL);

    while (1) {
        char *com_tok = 0;
        char *classnamepos = 0;

        // parse the opening brace
        com_tok = COM_Parse(&oldents, NULL);
        if (!oldents) {
            break;
        }

        q2a_strcat(newents, va("%s\n", com_tok));

        if (com_tok[0] != '{') {
            break;
        }

        // go through all the dictionary pairs
        while (1) {
            // parse key
            com_tok = COM_Parse(&oldents, &classnamepos);
            if (com_tok[0] == '}') {
                q2a_strcat(newents, va("%s\n", com_tok));
                break;
            }

            if (!oldents) {
                break;
            }

            q2a_strcat(newents, va("\"%s\" ", com_tok));

            // parse value
            com_tok = COM_Parse(&oldents, NULL);
            if (!oldents) {
                break;
            }

            replaced = qfalse;
            SubstituteEntity(newents, tune_spawn_quad, "item_quad", com_tok, &replaced);
            SubstituteEntity(newents, tune_spawn_invulnerability, "item_invulnerability", com_tok, &replaced);
            SubstituteEntity(newents, tune_spawn_powershield, "item_power_shield", com_tok, &replaced);
            SubstituteEntity(newents, tune_spawn_megahealth, "item_health_mega", com_tok, &replaced);
            SubstituteEntity(newents, tune_spawn_bfg, "weapon_bfg", com_tok, &replaced);
            SubstituteEntity(newents, tune_spawn_railgun, "weapon_railgun", com_tok, &replaced);
            SubstituteEntity(newents, tune_spawn_rocketlauncher, "weapon_rocketlauncher", com_tok, &replaced);
            SubstituteEntity(newents, tune_spawn_hyperblaster, "weapon_hyperblaster", com_tok, &replaced);
            SubstituteEntity(newents, tune_spawn_grenadelauncher, "weapon_grenadelauncher", com_tok, &replaced);
            SubstituteEntity(newents, tune_spawn_chaingun, "weapon_chaingun", com_tok, &replaced);
            SubstituteEntity(newents, tune_spawn_machinegun, "weapon_machinegun", com_tok, &replaced);
            SubstituteEntity(newents, tune_spawn_supershotgun, "weapon_supershotgun", com_tok, &replaced);
            SubstituteEntity(newents, tune_spawn_shotgun, "weapon_shotgun", com_tok, &replaced);
            SubstituteEntity(newents, tune_spawn_grenades, "ammo_grenades", com_tok, &replaced);

            // add the original token if it wasn't replaced
            if (replaced == qfalse) {
                q2a_strcat(newents, va("\"%s\"\n", com_tok));
            }

            if (com_tok[0] == '}') {
                break;
            }
        }
    }
}

/**
 *
 */
void SpawnEntities(char *mapname, char *entities, char *spawnpoint) {
    int len, currentlen;
    FILE *motdptr;
    int i;
    char *backupentities = entities;
    profile_init(1);
    profile_init(2);

    if (!dllloaded) {
        return;
    }

    if (q2adminrunmode == 0) {
        ge_mod->SpawnEntities(mapname, backupentities, spawnpoint);
        G_MergeEdicts();
        return;
    }
    
    profile_start(1);

    for (i = -1; i < maxclients->value; i++) {

        if (i < 0 || proxyinfo[i].inuse == 0) {
            proxyinfo[i].speedfreeze = 0;
            proxyinfo[i].enteredgame = 0;
            proxyinfo[i].msec_bad = 0;
            proxyinfo[i].msec_start = 0;
            proxyinfo[i].timescale = 0;
            proxyinfo[i].frames_count = 0;
            proxyinfo[i].show_fps = qfalse;
            proxyinfo[i].msec_last = 0;
            proxyinfo[i].msec_count = 0;
            proxyinfo[i].q2a_admin = 0;
            proxyinfo[i].q2a_bypass = 0;
            proxyinfo[i].admin = 0;
            proxyinfo[i].clientcommand = 0;
            proxyinfo[i].floodinfo.chatFloodProtect = qfalse;
            proxyinfo[i].stuffFile = 0;
        } else {
            proxyinfo[i].clientcommand &= (LEVELCHANGE_KEEP);
        }

        proxyinfo[i].userinfo_changed_count = 0;
        proxyinfo[i].userinfo_changed_start = ltime;
        proxyinfo[i].pcmd_noreply_count = 0;
        proxyinfo[i].pmod_noreply_count = 0;
        proxyinfo[i].private_command = 0;
        proxyinfo[i].pmod = 0;
        proxyinfo[i].pver = 0;
        proxyinfo[i].done_server_and_blocklist = 0;
        proxyinfo[i].cmdlist = 0;
        proxyinfo[i].newcmd_timeout = 0;
        proxyinfo[i].cmdlist_timeout = 0;
        proxyinfo[i].pmodver = 0;
        proxyinfo[i].gl_driver_changes = 0;
        proxyinfo[i].gl_driver[0] = 0;
        proxyinfo[i].impulsesgenerated = 0;
        proxyinfo[i].rbotretries = 0;
        proxyinfo[i].retries = 0;
        proxyinfo[i].charindex = 0;
        proxyinfo[i].teststr[0] = 0;
        proxyinfo[i].cl_pitchspeed = 0;
        proxyinfo[i].cl_anglespeedkey = 0.0;
        proxyinfo[i].votescast = 0;
        proxyinfo[i].votetimeout = 0;
        proxyinfo[i].checked_hacked_exe = 0;
        removeClientCommands(i);
    }

    proxyinfo[-1].inuse = 1;
    freeBanLists();
    freeLRconLists();
    freeFloodLists();
    freeVoteLists();
    freeDisableLists();

    motd[0] = 0;
    if (zbotmotd[0]) {
        motdptr = fopen(zbotmotd, "rt");

        if (!motdptr) {
            zbotmotd[0] = 0;
        } else {

            len = 0;
            while (fgets(buffer, 256, motdptr)) {
                currentlen = q2a_strlen(buffer);

                if (len + currentlen > sizeof (motd)) {
                    break;
                }

                len += currentlen;
                q2a_strcat(motd, buffer);
            }

            fclose(motdptr);
        }
    }

    zbot_teststring1[7] = zbot_teststring_test1[7] = '0' + (int) (9.9 * random());
    zbot_teststring1[8] = zbot_teststring_test1[8] = '0' + (int) (9.9 * random());
    zbot_teststring_test2[3] = '0' + (int) (9.9 * random());
    zbot_teststring_test2[4] = '0' + (int) (9.9 * random());
    zbot_testchar1 = '0' + (int) (9.9 * random());
    zbot_testchar2 = '0' + (int) (9.9 * random());

    if (spawnentities_enable) {
        readSpawnLists();
        q2a_strncpy(buffer, moddir, sizeof(buffer));
        q2a_strcat(buffer, "/q2adminmaps/");
        q2a_strcat(buffer, mapname);
        q2a_strcat(buffer, ".q2aspawn");
        ReadSpawnFile(buffer, qtrue);

        qboolean replaceteam;
        qboolean entremoved;

        // parse out all the turned off entities...
        while (1) {
            //gi.cprintf(NULL, PRINT_HIGH, "%s\n", entstr);
            char *com_tok = 0;
            char *classnamepos = 0;
            char *teampos = 0;
            char keyname[256];

            // parse the opening brace
            com_tok = COM_Parse(&entities, NULL);
            if (!entities) {
                break;
            }

            //q2a_strcat(entstr, va("%s\n", com_tok));

            if (com_tok[0] != '{') {
                break;
            }

            replaceteam = qfalse;
            entremoved = qfalse;

            // go through all the dictionary pairs
            while (1) {
                // parse key
                com_tok = COM_Parse(&entities, &classnamepos);
                if (com_tok[0] == '}') {
                    break;
                }

                if (!entities) {
                    break;
                }

                q2a_strncpy(keyname, com_tok, sizeof (keyname) - 1);

                // parse value
                com_tok = COM_Parse(&entities, NULL);
                if (!entities) {
                    break;
                }

                if (com_tok[0] == '}') {
                    break;
                }

                if (q2a_strcmp(keyname, "team") == 0) {
                    teampos = classnamepos;
                    replaceteam = qtrue;
                }

                if (!Q_stricmp("classname", keyname) && checkDisabledEntities(com_tok)) {
                    // change the 'classname' entry to '_lassname', this makes the q2 code ingore it.
                    classnamepos[0] = '_';
                    // side-effect: it may cause error messages on the console screen depending on the mod...
                    entremoved = qtrue;
                }
            }

            // if teamed and removed, change 'team' entry to '_eam' to unlink it from the rest
            if (teampos && replaceteam && entremoved) {
                teampos[0] = '_';
                teampos = 0;
                replaceteam = qfalse;
            }
        }
    }

    q2a_memset(finalentities, 0, sizeof(finalentities));
    //SubstituteEntities(finalentities, backupentities);

    profile_start(2);
    //ge_mod->SpawnEntities(mapname, finalentities, spawnpoint);
    ge_mod->SpawnEntities(mapname, backupentities, spawnpoint);
    profile_stop(2, "mod->SpawnEntities", 0, NULL);

    G_MergeEdicts();

    readBanLists();
    AC_LoadExceptions();
    loadhashlist();
    readLRconLists();
    readFloodLists();
    readVoteLists();
    readDisableLists();
    readCheckVarLists();

    // exec the map cfg file...
    q2a_strncpy(gmapname, mapname, sizeof(gmapname));
    if (mapcfgexec) {
        q2a_strncpy(gmapname, mapname, sizeof(gmapname));
        q2a_strcpy(buffer, "exec mapcfg/");
        q2a_strcat(buffer, mapname);
        q2a_strcat(buffer, "-post.cfg\n");
        gi.AddCommandString(buffer);
    }

    if (!*serverip) {
        gi.dprintf("You have not set a server ip.  Please add the following to %s\nserverip \"ip\" where ip matches the outgoing one of the server.\n", CFGFILE);
    }

    cloud.maxclients = (int) maxclients->value;
    q2a_strncpy(cloud.mapname, mapname, sizeof(cloud.mapname));
    cloud.port = getport();
    cloud.frame_number = 0;

    if (cloud.state == CA_STATE_TRUSTED) {
        CA_Map(mapname);
    }

    profile_stop(1, "q2admin->SpawnEntities", 0, NULL);
}

/**
 *
 */
qboolean UpdateInternalClientInfo(int client, edict_t *ent, char *userinfo, qboolean* userInfoOverflow) {
    char *ip = FindIpAddressInUserInfo(userinfo, userInfoOverflow);

    if (*ip) {
        unsigned int i;
        int num;

        if (q2a_strcmp(ip, "loopback") == 0) {
            net_parseIP(&proxyinfo[client].address, "127.0.0.1:0");
        } else {
            net_parseIP(&proxyinfo[client].address, ip);
        }
    }

    ip = Info_ValueForKey(userinfo, "Nitro2");

    if (*ip) {
        if (proxy_nitro2) {
            proxyinfo[client].clientcommand |= CCMD_NITRO2PROXY;
        } else {
            return qtrue;
        }
    }

    ip = Info_ValueForKey(userinfo, "bwproxy");

    if (*ip) {
        if (proxy_bwproxy) {
            proxyinfo[client].clientcommand |= CCMD_NITRO2PROXY;
        } else {
            return qtrue;
        }
    }

    return qfalse;
}

/**
 *
 */
qboolean checkReconnectUserInfoSame(char *userinfo1, char *userinfo2) {
    if (reconnect_checklevel) {
        char *cp1 = FindIpAddressInUserInfo(userinfo1, 0);
        char *cp2 = FindIpAddressInUserInfo(userinfo2, 0);
        char *bp = cp1;

        if (*cp1 == 0 || *cp2 == 0) {
            return qfalse;
        }

        while (*bp && *bp != ':') {
            bp++;
        }

        *bp = 0;

        bp = cp2;

        while (*bp && *bp != ':') {
            bp++;
        }

        *bp = 0;

        if (q2a_strcmp(cp1, cp2) != 0) {
            return qfalse;
        }

        cp1 = Info_ValueForKey(userinfo1, "name");
        cp2 = Info_ValueForKey(userinfo2, "name");

        if (q2a_strcmp(cp1, cp2) != 0) {
            return qfalse;
        }

        cp1 = Info_ValueForKey(userinfo1, "skin");
        cp2 = Info_ValueForKey(userinfo2, "skin");

        return (q2a_strcmp(cp1, cp2) == 0);
    }

    return (q2a_strcmp(userinfo1, userinfo2) == 0);
}

/**
 *
 */
qboolean checkReconnectList(char *username) {
    unsigned int i;

    for (i = 0; i < maxclients->value; i++) {
        if (reconnectproxyinfo[i].inuse && q2a_strcmp(reconnectproxyinfo[i].name, username) == 0) {
            reconnectproxyinfo[i].inuse = qfalse;
            return qfalse;
        }
    }
    return qtrue;
}

/**
 * Called when a new player first connects to the server, before entering the game
 */
qboolean ClientConnect(edict_t *ent, char *userinfo) {
    int client;
    char *s;
    char *skinname;

    qboolean ret;
    qboolean userInfoOverflow = qfalse;

    profile_init(1);
    profile_init(2);

    if (!dllloaded) return qfalse;

    if (q2adminrunmode == 0) {
        ret = ge_mod->ClientConnect(ent, userinfo);
        G_MergeEdicts();
        return ret;
    }

    profile_start(1);

    // allways clearout just in case there isn't any clients (therefore runframe doesn't get called)
    if (maxReconnectList) {
        unsigned int i;

        for (i = 0; i < maxReconnectList; i++) {
            if (reconnectlist[i].reconnecttimeout < ltime) {
                // wipe out the retry list if it's the only one pointing to it.
                unsigned int j;

                for (j = 0; j < maxReconnectList; j++) {
                    if (j != i && reconnectlist[j].retrylistidx == reconnectlist[i].retrylistidx) {
                        break;
                    }
                }

                if (j >= maxReconnectList) {
                    if (reconnectlist[i].retrylistidx + 1 < maxretryList) {
                        q2a_memmove(&(retrylist[reconnectlist[i].retrylistidx]), &(retrylist[reconnectlist[i].retrylistidx + 1]), (maxretryList - (reconnectlist[i].retrylistidx + 1)) * sizeof (retrylist_info));
                    }
                    maxretryList--;
                }

                // wipe out the reconnect list entry
                if (i + 1 < maxReconnectList) {
                    q2a_memmove(&(reconnectlist[i]), &(reconnectlist[i + 1]), (maxReconnectList - (i + 1)) * sizeof (reconnect_info));
                    i--;
                }
                maxReconnectList--;
            }
        }
    }

    client = getEntOffset(ent) - 1;

    if (proxyinfo[client].baninfo) {
        if (proxyinfo[client].baninfo->numberofconnects) {
            proxyinfo[client].baninfo->numberofconnects--;
        }

        proxyinfo[client].baninfo = NULL;
    }

    proxyinfo[client].private_command = 0;
    proxyinfo[client].pmod = 0;
    proxyinfo[client].userinfo_changed_count = 0;
    proxyinfo[client].userinfo_changed_start = ltime;
    proxyinfo[client].pcmd_noreply_count = 0;
    proxyinfo[client].pmod_noreply_count = 0;
    proxyinfo[client].pver = 0;
    proxyinfo[client].done_server_and_blocklist = 0;
    proxyinfo[client].cmdlist = 0;
    proxyinfo[client].newcmd_timeout = 0;
    proxyinfo[client].cmdlist_timeout = 0;
    proxyinfo[client].pmodver = 0;
    proxyinfo[client].gl_driver_changes = 0;
    proxyinfo[client].gl_driver[0] = 0;
    proxyinfo[client].speedfreeze = 0;
    proxyinfo[client].enteredgame = ltime;
    proxyinfo[client].msec_bad = 0;
    proxyinfo[client].msec_start = 0;
    proxyinfo[client].show_fps = qfalse;
    proxyinfo[client].msec_last = 0;
    proxyinfo[client].msec_count = 0;
    proxyinfo[client].timescale = 0;
    proxyinfo[client].frames_count = 0;
    proxyinfo[client].q2a_admin = 0;
    proxyinfo[client].q2a_bypass = 0;
    proxyinfo[client].userid = -1;
    proxyinfo[client].vid_restart = qfalse;
    proxyinfo[client].inuse = 0;
    proxyinfo[client].admin = 0;
    proxyinfo[client].clientcommand = 0;
    proxyinfo[client].retries = 0;
    proxyinfo[client].rbotretries = 0;
    proxyinfo[client].charindex = 0;
    proxyinfo[client].name[0] = 0;
    proxyinfo[client].skin[0] = 0;
    proxyinfo[client].stuffFile = 0;
    proxyinfo[client].impulsesgenerated = 0;
    proxyinfo[client].floodinfo.chatFloodProtect = qfalse;
    proxyinfo[client].cl_pitchspeed = 0;
    proxyinfo[client].cl_anglespeedkey = 0.0;
    proxyinfo[client].votescast = 0;
    proxyinfo[client].votetimeout = 0;
    proxyinfo[client].checked_hacked_exe = 0;
    proxyinfo[client].ent = ent;
    removeClientCommands(client);

    ret = 1;

    if (client < maxclients->value) {
        if (UpdateInternalClientInfo(client, ent, userinfo, &userInfoOverflow)) {
            Q_snprintf(buffer, sizeof(buffer), zbotuserdisplay, proxyinfo[client].name);
            currentBanMsg = buffer;
            logEvent(LT_BAN, client, ent, currentBanMsg, 0, 0.0);

            if (!banOnConnect) {
                ret = 0;
            } else {
                proxyinfo[client].clientcommand |= CCMD_BANNED;
                q2a_strncpy(proxyinfo[client].buffer, currentBanMsg, sizeof(proxyinfo[client].buffer));
            }
        }
    }

    // check for malformed or illegal info strings
    if (!Info_Validate(userinfo)) {
        q2a_strcpy(userinfo, "\\name\\badinfo\\skin\\male/grunt");
    }

    q2a_strncpy(proxyinfo[client].userinfo, userinfo, sizeof(proxyinfo[client].userinfo));

    // set name
    s = Info_ValueForKey(userinfo, "name");
    if (*s == 0) {
        s = NULL;
        return qfalse;
    }

    q2a_strncpy(proxyinfo[client].name, s, sizeof (proxyinfo[client].name) - 1);

    skinname = Info_ValueForKey(userinfo, "skin");
    if (*skinname == 0) {
        return qfalse;
    }

    if (strlen(skinname) > 38) {
        gi.cprintf(NULL, PRINT_HIGH, "%s: Skin name exceeds 38 characters (IP = %s)\n", proxyinfo[client].name, IP(client));
        return qfalse;
    }

    q2a_strncpy(proxyinfo[client].skin, skinname, sizeof (proxyinfo[client].skin) - 1);

    //   q2a_strcpy(ent->client->pers.netname, proxyinfo[client].name);
    q2a_strncpy(proxyinfo[client].userinfo, userinfo, sizeof (proxyinfo[client].userinfo) - 1);

    if (lockDownServer && checkReconnectList(proxyinfo[client].name)) {
        currentBanMsg = lockoutmsg;

        logEvent(LT_BAN, client, ent, currentBanMsg, 0, 0.0);
        gi.cprintf(NULL, PRINT_HIGH, "%s: %s (IP = %s)\n", proxyinfo[client].name, currentBanMsg, IP(client));

        if (banOnConnect) {
            ret = 0;
        } else {
            proxyinfo[client].clientcommand |= CCMD_BANNED;
            q2a_strncpy(proxyinfo[client].buffer, currentBanMsg, sizeof(proxyinfo[client].buffer));
        }
    } else if (checkClientIpAddress && !HASIP(client)) // check for invlaid IP's and don't let them in :)
    {
        char *ip = FindIpAddressInUserInfo(userinfo, 0);
        gi.cprintf(NULL, PRINT_HIGH, "%s: %s (%s)\n", proxyinfo[client].name, "Client doesn't have a valid IP address", ip);
        logEvent(LT_INVALIDIP, client, ent, userinfo, 0, 0.0);

        if (banOnConnect) {
            ret = 0;
        } else {
            proxyinfo[client].clientcommand |= CCMD_BANNED;
            q2a_strcpy(proxyinfo[client].buffer, "Client doesn't have a valid IP address");
        }
    } else if (checkCheckIfBanned(ent, client)) {
        logEvent(LT_BAN, client, ent, currentBanMsg, 0, 0.0);
        gi.cprintf(NULL, PRINT_HIGH, "%s: %s (IP = %s)\n", proxyinfo[client].name, currentBanMsg, IP(client));

        if (banOnConnect) {
            ret = 0;
        } else {
            proxyinfo[client].clientcommand |= CCMD_BANNED;
            q2a_strncpy(proxyinfo[client].buffer, currentBanMsg, sizeof(proxyinfo[client].buffer));
        }
    } else if (ret && !(proxyinfo[client].clientcommand & CCMD_BANNED)) {
        qboolean doConnect = qtrue;

        // is reconnect_address set?
        if (!isBlank(reconnect_address)) {
            char *ip = FindIpAddressInUserInfo(userinfo, 0);
            char *bp = ip;
            unsigned int i;

            if (*ip == 0) {
                // force a reconnect and exit...
                doConnect = qfalse;
                proxyinfo[client].clientcommand |= CCMD_RECONNECT;
            } else {
                while (*bp && *bp != ':') {
                    bp++;
                }

                *bp = 0;


                // check to see if they are in the reconnect list?
                for (i = 0; i < maxReconnectList; i++) {
                    if (checkReconnectUserInfoSame(userinfo, reconnectlist[i].userinfo)) {
                        // found a match...
                        break;
                    } else {
                        // check if the same IP and delete... this stops proxies from reconnecting...
                        char *reconnectip = FindIpAddressInUserInfo(reconnectlist[i].userinfo, 0);
                        bp = reconnectip;

                        if (*reconnectip) {
                            while (*bp && *bp != ':') {
                                bp++;
                            }

                            *bp = 0;

                            if (strcmp(ip, reconnectip) == 0) {
                                // remove from list and continue with connect
                                if (i + 1 < maxReconnectList) {
                                    q2a_memmove(&(reconnectlist[i]), &(reconnectlist[i + 1]), (maxReconnectList - (i + 1)) * sizeof (reconnect_info));
                                    i--;
                                }
                                maxReconnectList--;
                            }
                        }
                    }
                }

                if (i < maxReconnectList) {
                    unsigned int j;

                    // remove the retry list entry if needed...
                    for (j = 0; j < maxReconnectList; j++) {
                        if (j != i && reconnectlist[j].retrylistidx == reconnectlist[i].retrylistidx) {
                            break;
                        }
                    }

                    if (j >= maxReconnectList) {
                        if (reconnectlist[i].retrylistidx + 1 < maxretryList) {
                            q2a_memmove(&(retrylist[reconnectlist[i].retrylistidx]), &(retrylist[reconnectlist[i].retrylistidx + 1]), (maxretryList - (reconnectlist[i].retrylistidx + 1)) * sizeof (retrylist_info));
                        }
                        maxretryList--;
                    }

                    // remove from list and continue with connect
                    if (i + 1 < maxReconnectList) {
                        q2a_memmove(&(reconnectlist[i]), &(reconnectlist[i + 1]), (maxReconnectList - (i + 1)) * sizeof (reconnect_info));
                    }
                    maxReconnectList--;
                } else {
                    // force a reconnect and exit...

                    doConnect = qfalse;
                    proxyinfo[client].clientcommand |= CCMD_RECONNECT;
                }
            }
        }


        if (doConnect) {
            profile_start(2);
            ret = ge_mod->ClientConnect(ent, userinfo);
            profile_stop(2, "mod->ClientConnect", client, ent);

            G_MergeEdicts();
            CA_PlayerConnect(ent);
        }
    }

    if (whois_active) {
        whois_getid(client, ent);
        whois_update_seen(client, ent);
    }

    if (vpn_enable) {
        LookupVPNStatus(ent);
    }

    if (ret) {
        logEvent(LT_CLIENTCONNECT, client, ent, NULL, 0, 0.0);

        if (userInfoOverflow) {
            gi.cprintf(NULL, PRINT_HIGH, "%s: %s (%s)\n", proxyinfo[client].name, "WARNING: Client's userinfo space looks to have overflowed!", IP(client));
            proxyinfo[client].clientcommand |= CCMD_CLIENTOVERFLOWED;
        }
    }

    profile_stop(1, "q2admin->ClientConnect", client, ent);
    return ret;
}

/**
 *
 */
qboolean checkForNameChange(int client, edict_t *ent, char *userinfo) {
    char *s = Info_ValueForKey(userinfo, "name");
    char oldname[sizeof (proxyinfo[client].name)];
    char newname[sizeof (proxyinfo[client].name)];

    q2a_strncpy(newname, s, sizeof (newname) - 1);
    newname[sizeof (newname) - 1] = 0;

    if (proxyinfo[client].name[0] == 0) {
        q2a_strcpy(proxyinfo[client].name, newname);
    } else if (q2a_strcmp(proxyinfo[client].name, newname) != 0) {
        // check if ratbot detect?
        if (q2a_strcmp(newname, RATBOT_CHANGENAMETEST) == 0) {
            removeClientCommand(client, QCMD_TESTRATBOT4);
            proxyinfo[client].clientcommand &= ~CCMD_RATBOTDETECTNAME;
            proxyinfo[client].clientcommand |= CCMD_RBOTCLEAR;

            // ok not a ratbot.. turn off detection
            //      addCmdQueue(client, QCMD_CHANGENAME, 0, 0, 0);
            return qfalse;
        }

        // check for flooding..
        if (nameChangeFloodProtect) {
            if (proxyinfo[client].clientcommand & CCMD_NCSILENCE) {
                if (proxyinfo[client].namechangetimeout < ltime) {
                    proxyinfo[client].clientcommand &= ~CCMD_NCSILENCE;
                } else {
                    int secleft = (int) (proxyinfo[client].namechangetimeout - ltime) + 1;

                    //          q2a_strcpy(ent->client->pers.netname, proxyinfo[client].name);
                    addCmdQueue(client, QCMD_CHANGENAME, 0, 0, 0);

                    gi.cprintf(ent, PRINT_HIGH, "%d seconds of name change silence left.\n", secleft);
                    return qfalse;
                }
            }
        }

        q2a_strcpy(oldname, proxyinfo[client].name);
        q2a_strcpy(proxyinfo[client].name, newname);

        if (whois_active) {
            if (proxyinfo[client].userid == -1) {
                whois_adduser(client, ent);
            }
            whois_newname(client, ent);
        }

        if (checkCheckIfBanned(ent, client)) {
            logEvent(LT_BAN, client, ent, currentBanMsg, 0, 0.0);
            q2a_strcpy(proxyinfo[client].name, oldname);

            // display ban msg to user..
            //      gi.cprintf (ent, PRINT_HIGH, "Can't change name to a banned name\n");
            gi.cprintf(ent, PRINT_HIGH, "%s\n", currentBanMsg);
            //      q2a_strcpy(ent->client->pers.netname, proxyinfo[client].name);

            if (kickOnNameChange) {
                addCmdQueue(client, QCMD_DISCONNECT, 1, 0, currentBanMsg);
            } else {
                addCmdQueue(client, QCMD_CHANGENAME, 0, 0, 0);
            }
        } else {
            logEvent(LT_NAMECHANGE, client, ent, oldname, 0, 0.0);

            if (displaynamechange) {
                gi.bprintf(PRINT_HIGH, "%s changed name to %s\n", oldname, proxyinfo[client].name);
            }

            if (nameChangeFloodProtect) {
                if (proxyinfo[client].namechangetimeout < ltime) {
                    proxyinfo[client].namechangetimeout = ltime + nameChangeFloodProtectSec;
                    proxyinfo[client].namechangecount = 0;
                } else {
                    if (proxyinfo[client].namechangecount >= nameChangeFloodProtectNum) {
                        //            q2a_strcpy(ent->client->pers.netname, proxyinfo[client].name);
                        Q_snprintf(buffer, sizeof(buffer), nameChangeFloodProtectMsg, proxyinfo[client].name);
                        gi.bprintf(PRINT_HIGH, "%s\n", buffer);

                        if (nameChangeFloodProtectSilence == 0) {
                            addCmdQueue(client, QCMD_DISCONNECT, 0, 0, nameChangeFloodProtectMsg);
                        } else {
                            proxyinfo[client].namechangetimeout = ltime + nameChangeFloodProtectSilence;
                            proxyinfo[client].clientcommand |= CCMD_NCSILENCE;
                        }
                        return qfalse;
                    }

                    proxyinfo[client].namechangecount++;
                }
            }
        }
    }

    return qtrue;
}

/**
 *
 */
qboolean checkForSkinChange(int client, edict_t *ent, char *userinfo) {
    char *s = Info_ValueForKey(userinfo, "skin");
    char oldskin[sizeof (proxyinfo[client].skin)];
    char newskin[sizeof (proxyinfo[client].skin)];
    char *skinname;

    q2a_strncpy(newskin, s, sizeof (newskin) - 1);
    newskin[sizeof (newskin) - 1] = 0;

    if (proxyinfo[client].skin[0] == 0) {
        q2a_strcpy(proxyinfo[client].skin, newskin);
    } else if (q2a_strcmp(proxyinfo[client].skin, newskin) != 0) {
        // check for flooding..
        if (skinChangeFloodProtect) {
            if (proxyinfo[client].clientcommand & CCMD_SCSILENCE) {
                if (proxyinfo[client].skinchangetimeout < ltime) {
                    proxyinfo[client].clientcommand &= ~CCMD_SCSILENCE;
                } else {
                    int secleft = (int) (proxyinfo[client].skinchangetimeout - ltime) + 1;

                    // q2a_strcpy(ent->client->pers.netskin, proxyinfo[client].skin);
                    addCmdQueue(client, QCMD_CHANGESKIN, 0, 0, 0);

                    gi.cprintf(ent, PRINT_HIGH, "%d seconds of skin change silence left.\n", secleft);
                    return qfalse;
                }
            }
        }

        q2a_strcpy(oldskin, proxyinfo[client].skin);
        q2a_strcpy(proxyinfo[client].skin, newskin);

        logEvent(LT_SKINCHANGE, client, ent, oldskin, 0, 0.0);

        if (skinChangeFloodProtect) {
            if (proxyinfo[client].skinchangetimeout < ltime) {
                proxyinfo[client].skinchangetimeout = ltime + skinChangeFloodProtectSec;
                proxyinfo[client].skinchangecount = 0;
            } else {
                if (proxyinfo[client].skinchangecount >= skinChangeFloodProtectNum) {
                    Q_snprintf(buffer, sizeof(buffer), skinChangeFloodProtectMsg, proxyinfo[client].name);
                    gi.bprintf(PRINT_HIGH, "%s\n", buffer);

                    if (skinChangeFloodProtectSilence == 0) {
                        addCmdQueue(client, QCMD_DISCONNECT, 0, 0, skinChangeFloodProtectMsg);
                    } else {
                        proxyinfo[client].skinchangetimeout = ltime + skinChangeFloodProtectSilence;
                        proxyinfo[client].clientcommand |= CCMD_SCSILENCE;
                    }
                    return qfalse;
                }

                proxyinfo[client].skinchangecount++;
            }
        }
    }

    skinname = Info_ValueForKey(userinfo, "skin");
    if (strlen(skinname) > 38) {
        Q_snprintf(buffer, sizeof(buffer), skincrashmsg, proxyinfo[client].name);
        gi.bprintf(PRINT_HIGH, "%s\n", buffer);
        addCmdQueue(client, QCMD_DISCONNECT, 0, 0, skincrashmsg);
        return qfalse;
    }

    return qtrue;
}

/**
 * Client changed something (name, skin, rate, hand, etc)
 */
void ClientUserinfoChanged(edict_t *ent, char *userinfo) {
    int client;
    qboolean passon;

    char *s = Info_ValueForKey(userinfo, "name");
    char tmptext[128];
    char *cl_max_temp;
    char *cl_max_curr;
    char *timescale_temp;
    int temp;

    profile_init(1);
    profile_init(2);

    if (!dllloaded) {
        return;
    }

    if (q2adminrunmode == 0) {
        ge_mod->ClientUserinfoChanged(ent, userinfo);
        G_MergeEdicts();
        return;
    }

    profile_start(1);

    client = getEntOffset(ent) - 1;

    logEvent(LT_CLIENTUSERINFO, client, ent, userinfo, 0, 0.0);

    //zgh_frk check
    if (stringContains(userinfo, "\\skon\\")) {
        gi.bprintf(PRINT_HIGH, "%s was caught cheating!\n", proxyinfo[client].name);
        Q_snprintf(tmptext, sizeof(tmptext), "kick %d\n", client);
        gi.AddCommandString(tmptext);
        logEvent(LT_ZBOT, client, ent, userinfo, -14, 0.0);
    }

    // Don't count excessive cl_maxfps changes as flooding if fpsfloodexempt is true.
    // Changing FPS frequently is normal behavior for jumpmod
    cl_max_temp = Info_ValueForKey(userinfo, "cl_maxfps");
    cl_max_curr = Info_ValueForKey(proxyinfo[client].userinfo, "cl_maxfps");
    if (!fpsFloodExempt && Q_stricmp(cl_max_temp, cl_max_curr) != 0) {
        proxyinfo[client].userinfo_changed_count++;
    }

    if (proxyinfo[client].userinfo_changed_count > USERINFOCHANGE_COUNT) {
        temp = ltime - proxyinfo[client].userinfo_changed_start;
        if (temp < USERINFOCHANGE_TIME) {
            gi.bprintf(PRINT_HIGH, "%s tried to flood the server (2)\n", proxyinfo[client].name);
            Q_snprintf(tmptext, sizeof(tmptext), "kick %d\n", client);
            gi.AddCommandString(tmptext);
        } else {
            //enuf time passed, reset count
            proxyinfo[client].userinfo_changed_count = 0;
            proxyinfo[client].userinfo_changed_start = ltime;
        }
    }

    passon = checkForNameChange(client, ent, userinfo);
    if (!checkForSkinChange(client, ent, userinfo)) {
        passon = qfalse;
    }

    if (passon && !(proxyinfo[client].clientcommand & BANCHECK)) {
        profile_start(2);
        ge_mod->ClientUserinfoChanged(ent, userinfo);
        profile_stop(2, "mod->ClientUserinfoChanged", client, ent);

        G_MergeEdicts();
    }

    proxyinfo[client].rate = q2a_atoi(Info_ValueForKey(userinfo, "rate"));

    if (maxrateallowed && proxyinfo[client].rate > maxrateallowed) {
        addCmdQueue(client, QCMD_CLIPTOMAXRATE, 0, 0, 0);
    }

    if (minrateallowed && proxyinfo[client].rate < minrateallowed) {
        addCmdQueue(client, QCMD_CLIPTOMINRATE, 0, 0, 0);
    }

    timescale_temp = Info_ValueForKey(userinfo, "timescale");

    if (strlen(timescale_temp)) {
        //if timescale has length, then its set
        proxyinfo[client].timescale = atoi(timescale_temp);

        //my check here, if timescale = 0 and it has length we will NOT allow
        if (proxyinfo[client].timescale == 0) {
            if (displayzbotuser) {
                gi.bprintf(PRINT_HIGH, "%s%s\n", timescaleuserdisplay, proxyinfo[client].name);
            }
            if (proxyinfo[client].inuse) {
                gi.cprintf(ent, PRINT_HIGH, PRV_KICK_MSG, proxyinfo[client].name);
            }

            addCmdQueue(client, QCMD_DISCONNECT, 1, 0, timescaleuserdisplay);
        } else {
            if (timescaledetect) {
                if (proxyinfo[client].timescale != 1) {
                    //if its not 1, make it so
                    addCmdQueue(client, QCMD_SETTIMESCALE, 0, 0, 0);
                }
            }
        }
    } else {
        //if not, we need to do initial check
        proxyinfo[client].timescale = 0;
        if (timescaledetect) {
            addCmdQueue(client, QCMD_SETUPTIMESCALE, 0, 0, 0);
        }
    }

    cl_max_temp = Info_ValueForKey(userinfo, "cl_maxfps");

    if (strlen(cl_max_temp)) {
        //if cl_maxfps has length, then its set
        proxyinfo[client].maxfps = atoi(cl_max_temp);

        //my check here, if maxfps = 0 and it has length we will NOT allow
        if (proxyinfo[client].maxfps == 0) {
            gi.bprintf(PRINT_HIGH, (PRV_KICK_MSG, proxyinfo[client].name));
            
            addCmdQueue(client, QCMD_DISCONNECT, 1, 0, (PRV_KICK_MSG, proxyinfo[client].name));
        } else {
            if (maxfpsallowed) {
                if (proxyinfo[client].maxfps > maxfpsallowed) {
                    addCmdQueue(client, QCMD_SETMAXFPS, 0, 0, 0);
                }
            }
        }
    } else {
        //if not, we need to do initial check
        proxyinfo[client].maxfps = 0;
        if (maxfpsallowed) {
            addCmdQueue(client, QCMD_SETUPMAXFPS, 0, 0, 0);
        }
    }

    if (minfpsallowed) {
        if (proxyinfo[client].maxfps == 0) {
            addCmdQueue(client, QCMD_SETUPMAXFPS, 0, 0, 0);
        } else if (proxyinfo[client].maxfps < minfpsallowed) {
            addCmdQueue(client, QCMD_SETMINFPS, 0, 0, 0);
        }
    }


    if (cl_pitchspeed_enable) {
        int newps = q2a_atoi(Info_ValueForKey(userinfo, "cl_pitchspeed"));

        if (newps == 0) {
            addCmdQueue(client, QCMD_SETUPCL_PITCHSPEED, 0, 0, 0);
        } else if (proxyinfo[client].cl_pitchspeed == 0) {
            proxyinfo[client].cl_pitchspeed = newps;
        } else {
            proxyinfo[client].cl_pitchspeed = newps;

            // ingore changes to 150 because action quake2 generates them itself...

            if (proxyinfo[client].cl_pitchspeed != 150) {
                logEvent(LT_ZBOT, client, ent, NULL, -7, 0.0);

                if (cl_pitchspeed_display) {
                    gi.bprintf(PRINT_HIGH, "%s changed cl_pitchspeed to %d\n", proxyinfo[client].name, newps);
                }

                if (cl_pitchspeed_kick) {
                    gi.cprintf(ent, PRINT_HIGH, "%s\n", cl_pitchspeed_kickmsg);

                    addCmdQueue(client, QCMD_DISCONNECT, 1, 0, cl_pitchspeed_kickmsg);
                }
            }
        }
    }

    if (cl_anglespeedkey_enable) {
        float newas = q2a_atof(Info_ValueForKey(userinfo, "cl_anglespeedkey"));

        if (newas == 0.0) {
            addCmdQueue(client, QCMD_SETUPCL_ANGLESPEEDKEY, 0, 0, 0);
        } else if (proxyinfo[client].cl_anglespeedkey == 0.0) {
            proxyinfo[client].cl_anglespeedkey = newas;
        } else {
            proxyinfo[client].cl_anglespeedkey = newas;

            // ingore changes to 1.5 because action quake2 generates them itself...

            if (proxyinfo[client].cl_anglespeedkey != 1.5) {
                logEvent(LT_ZBOT, client, ent, NULL, -9, 0.0);

                if (cl_anglespeedkey_display) {
                    gi.bprintf(PRINT_HIGH, "%s changed cl_anglespeedkey to %g\n", proxyinfo[client].name, newas);
                }

                if (cl_anglespeedkey_kick) {
                    gi.cprintf(ent, PRINT_HIGH, "%s\n", cl_anglespeedkey_kickmsg);

                    addCmdQueue(client, QCMD_DISCONNECT, 1, 0, cl_anglespeedkey_kickmsg);
                }
            }
        }
    }

    proxyinfo[client].msg = q2a_atoi(Info_ValueForKey(userinfo, "msg"));

    if (proxyinfo[client].msg > maxMsgLevel) {
        addCmdQueue(client, QCMD_MSGDISCONNECT, 2, 0, 0);
    }

    q2a_strncpy(proxyinfo[client].userinfo, userinfo, sizeof(proxyinfo[client].userinfo));

    proxyinfo[client].next_report = 0;

    CA_PlayerUpdate(client, proxyinfo[client].userinfo);
    
    profile_stop(1, "q2admin->ClientUserinfoChanged", client, ent);
}

/**
 * Called when a client is disconnected or quits
 */
void ClientDisconnect(edict_t *ent) {
    int client;

    profile_init(1);
    profile_init(2);

    if (!dllloaded) {
        return;
    }

    if (q2adminrunmode == 0) {
        ge_mod->ClientDisconnect(ent);
        G_MergeEdicts();
        return;
    }

    profile_start(1);

    client = getEntOffset(ent) - 1;

    if (client >= maxclients->value) {
        return;
    }

    CA_PlayerDisconnect(ent);
    
    if (!(proxyinfo[client].clientcommand & BANCHECK)) {
        profile_start(2);
        ge_mod->ClientDisconnect(ent);
        profile_stop(2, "mod->ClientDisconnect", client, ent);

        G_MergeEdicts();

        // setup the reconnect info if required.
        if (lockDownServer) {
            unsigned int i;

            for (i = 0; i < maxclients->value; i++) {
                if (!reconnectproxyinfo[i].inuse) {
                    q2a_strcpy(reconnectproxyinfo[i].name, proxyinfo[client].name);
                    reconnectproxyinfo[i].inuse = qtrue;
                    break;
                }
            }
        }
    }

    logEvent(LT_CLIENTDISCONNECT, client, ent, NULL, 0, 0.0);

    if (proxyinfo[client].baninfo) {
        if (proxyinfo[client].baninfo->numberofconnects) {
            proxyinfo[client].baninfo->numberofconnects--;
        }
        proxyinfo[client].baninfo = NULL;
    }

    if (proxyinfo[client].stuffFile) {
        fclose(proxyinfo[client].stuffFile);
    }

    proxyinfo[client].inuse = 0;
    proxyinfo[client].admin = 0;
    proxyinfo[client].retries = 0;
    proxyinfo[client].rbotretries = 0;
    proxyinfo[client].clientcommand = 0;
    proxyinfo[client].charindex = 0;
    proxyinfo[client].name[0] = 0;
    proxyinfo[client].skin[0] = 0;
    proxyinfo[client].stuffFile = 0;
    proxyinfo[client].impulsesgenerated = 0;
    proxyinfo[client].floodinfo.chatFloodProtect = qfalse;
    proxyinfo[client].votescast = 0;
    proxyinfo[client].votetimeout = 0;
    proxyinfo[client].checked_hacked_exe = 0;
    removeClientCommands(client);

    proxyinfo[client].userinfo_changed_count = 0;
    proxyinfo[client].userinfo_changed_start = ltime;
    proxyinfo[client].pcmd_noreply_count = 0;
    proxyinfo[client].pmod_noreply_count = 0;
    proxyinfo[client].private_command = 0;
    proxyinfo[client].pmod = 0;
    proxyinfo[client].pver = 0;
    proxyinfo[client].done_server_and_blocklist = 0;
    proxyinfo[client].cmdlist = 0;
    proxyinfo[client].newcmd_timeout = 0;
    proxyinfo[client].cmdlist_timeout = 0;
    proxyinfo[client].pmodver = 0;
    proxyinfo[client].gl_driver_changes = 0;
    proxyinfo[client].gl_driver[0] = 0;
    proxyinfo[client].speedfreeze = 0;
    proxyinfo[client].enteredgame = 0;
    proxyinfo[client].msec_bad = 0;
    proxyinfo[client].msec_start = 0;
    proxyinfo[client].timescale = 0;
    proxyinfo[client].frames_count = 0;
    proxyinfo[client].show_fps = qfalse;
    proxyinfo[client].msec_last = 0;
    proxyinfo[client].msec_count = 0;
    proxyinfo[client].q2a_admin = 0;
    proxyinfo[client].q2a_bypass = 0;
    proxyinfo[client].vid_restart = qfalse;
    proxyinfo[client].userid = -1;
    
    q2a_memset(&proxyinfo[client].vpn, 0, sizeof(vpn_t));
    q2a_memset(&proxyinfo[client].address, 0, sizeof(netadr_t));

    profile_stop(1, "q2admin->ClientDisconnect", 0, NULL);
}

/**
 * Called when a client is actually spawned into the game
 */
void ClientBegin(edict_t *ent) {
    int client;
    FILE *q2logfile;
    profile_init(1);
    profile_init(2);

    if (!dllloaded) {
        return;
    }

    if (q2adminrunmode == 0) {
        ge_mod->ClientBegin(ent);
        G_MergeEdicts();
        return;
    }

    profile_start(1);

    client = getEntOffset(ent) - 1;

    if (!(proxyinfo[client].clientcommand & BANCHECK)) {
        profile_start(2);
        ge_mod->ClientBegin(ent);
        profile_stop(2, "mod->ClientBegin", client, ent);

        G_MergeEdicts();
    } else {
        // setup ent to be valid...
        ent->client->ps.fov = 10;
    }

    if (client >= maxclients->value) {
        profile_stop(1, "q2admin->ClientBegin (client >= maxclients)", client, ent);
        return;
    }

    if (!proxyinfo[client].inuse) {
        proxyinfo[client].admin = 0;
        proxyinfo[client].speedfreeze = 0;
        proxyinfo[client].enteredgame = ltime;
        proxyinfo[client].msec_bad = 0;
        proxyinfo[client].msec_start = 0;
        proxyinfo[client].timescale = 0;
        proxyinfo[client].frames_count = 0;
        proxyinfo[client].show_fps = qfalse;
        proxyinfo[client].msec_last = 0;
        proxyinfo[client].msec_count = 0;
        proxyinfo[client].q2a_admin = 0;
        proxyinfo[client].q2a_bypass = 0;
    }

    proxyinfo[client].userinfo_changed_count = 0;
    proxyinfo[client].userinfo_changed_start = ltime;
    proxyinfo[client].pcmd_noreply_count = 0;
    proxyinfo[client].pmod_noreply_count = 0;
    proxyinfo[client].private_command = 0;
    proxyinfo[client].pmod = 0;
    proxyinfo[client].pver = 0;
    proxyinfo[client].done_server_and_blocklist = 0;
    proxyinfo[client].cmdlist = 0;
    proxyinfo[client].newcmd_timeout = 0;
    proxyinfo[client].cmdlist_timeout = 0;
    proxyinfo[client].pmodver = 0;
    proxyinfo[client].gl_driver_changes = 0;
    proxyinfo[client].gl_driver[0] = 0;
    proxyinfo[client].inuse = 0;
    proxyinfo[client].retries = 0;
    proxyinfo[client].rbotretries = 0;
    proxyinfo[client].charindex = 0;
    proxyinfo[client].teststr[0] = 0;
    proxyinfo[client].impulsesgenerated = 0;
    proxyinfo[client].votescast = 0;
    proxyinfo[client].votetimeout = 0;
    proxyinfo[client].checked_hacked_exe = 0;
    
    // positive value is normal client limit
    // negative value is vpn client limit (abs)
    // 0 is no limit
    if (ip_limit != 0) {
        int sameaddr = 1;
        for (int i=0; i<(int)maxclients->value; i++) {
            if (!proxyinfo[i].inuse || i == client) {
                continue;
            }
            if (net_addressesMatch(&proxyinfo[i].address, &proxyinfo[client].address)) {
                sameaddr++;
            }
        }
        if (sameaddr > ip_limit && ip_limit > 0) {
            Q_snprintf(buffer, sizeof(buffer), "Too many connections from the same IP address (%d)\n", sameaddr);
            gi.cprintf(ent, PRINT_HIGH, buffer);
            addCmdQueue(client, QCMD_DISCONNECT, 1, 0, buffer);
        }
        if (sameaddr > (ip_limit * -1) && ip_limit < 0 && proxyinfo[client].vpn.state == VPN_POSITIVE) {
            Q_snprintf(buffer, sizeof(buffer), "Too many VPN connections from the same IP address (%d)\n", sameaddr);
            gi.cprintf(ent, PRINT_HIGH, buffer);
            addCmdQueue(client, QCMD_DISCONNECT, 1, 0, buffer);
        }
    }

    if (num_q2a_admins) {
        addCmdQueue(client, QCMD_SPAMBYPASS, 60 + (10 * random()), 0, 0);
    }

    if (proxyinfo[client].clientcommand & CCMD_RECONNECT) {
        addCmdQueue(client, QCMD_RECONNECT, 1, 0, NULL);
    } else if (proxyinfo[client].clientcommand & CCMD_BANNED) {
        gi.cprintf(ent, PRINT_HIGH, "%s\n", proxyinfo[client].buffer);
        addCmdQueue(client, QCMD_DISCONNECT, 1, 0, proxyinfo[client].buffer);
    } else if (proxyinfo[client].clientcommand & CCMD_KICKED) {
        addCmdQueue(client, QCMD_DISCONNECT, 1, 0, "Kicked.");
    } else {
        addCmdQueue(client, QCMD_STARTUP, 0, 0, 0);

        if (adminpassword[0] && !proxyinfo[client].admin) {
            addCmdQueue(client, QCMD_TESTADMIN, 0, 0, 0);
        }
        if (num_admins && !proxyinfo[client].q2a_admin) {
            addCmdQueue(client, QCMD_TESTADMIN2, 0, 0, 0);
        }
        if (num_q2a_admins && !proxyinfo[client].q2a_bypass) {
            addCmdQueue(client, QCMD_TESTADMIN3, 0, 0, 0);
        }
        if (customClientCmdConnect[0] || customServerCmdConnect[0]) {
            addCmdQueue(client, QCMD_CONNECTCMD, 0, 0, 0);
        }

        if (timescaledetect) {
            addCmdQueue(client, QCMD_TESTTIMESCALE, 0, 0, 0);
        }

        if (proxyinfo[client].clientcommand & CCMD_CLIENTOVERFLOWED) {
            gi.cprintf(ent, PRINT_HIGH, "WARNING: Your userinfo looks to have overflowed. This may cause you problems during gameplay. Restart quake2 to clear your userinfo space.\n");
        }

        if (zbotmotd[0]) {
            gi.centerprintf(ent, motd);
        }

        addCmdQueue(client, QCMD_CHECKVARTESTS, (float) checkvar_poll_time, 0, 0);

        Q_snprintf(buffer, sizeof(buffer), "%s/qconsole.log", moddir);
        q2logfile = fopen(buffer, "rt");
        if (q2logfile) {
            fseek(q2logfile, 0, SEEK_END);
            proxyinfo[client].logfilecheckpos = ftell(q2logfile);
            fclose(q2logfile);
        }
    }

    logEvent(LT_CLIENTBEGIN, client, ent, NULL, 0, 0.0);
    profile_stop(1, "q2admin->ClientBegin", client, ent);
}

/**
 *
 */
void WriteGame(char *filename, qboolean autosave) {
    profile_init(1);
    if (!dllloaded) {
        return;
    }
    if (q2adminrunmode == 0) {
        ge_mod->WriteGame(filename, autosave);
        G_MergeEdicts();
        return;
    }
    profile_start(1);
    ge_mod->WriteGame(filename, autosave);
    G_MergeEdicts();
    profile_stop(1, "q2admin->WriteGame", 0, NULL);
}

/**
 * Currently just a pass-through
 */
void ReadGame(char *filename) {
    profile_init(1);
    if (!dllloaded) {
        return;
    }
    if (q2adminrunmode == 0) {
        ge_mod->ReadGame(filename);
        G_MergeEdicts();
        return;
    }
    profile_start(1);
    ge_mod->ReadGame(filename);
    G_MergeEdicts();
    profile_stop(1, "q2admin->ReadGame", 0, NULL);
}

/**
 *
 */
void WriteLevel(char *filename) {
    profile_init(1);
    if (!dllloaded) {
        return;
    }
    if (q2adminrunmode == 0) {
        ge_mod->WriteLevel(filename);
        G_MergeEdicts();
        return;
    }
    profile_start(1);
    ge_mod->WriteLevel(filename);
    G_MergeEdicts();
    profile_stop(1, "q2admin->WriteLevel", 0, NULL);
}

/**
 *
 */
void ReadLevel(char *filename) {
    profile_init(1);
    if (!dllloaded) {
        return;
    }
    if (q2adminrunmode == 0) {
        ge_mod->ReadLevel(filename);
        G_MergeEdicts();
        return;
    }
    profile_start(1);
    ge_mod->ReadLevel(filename);
    G_MergeEdicts();
    profile_stop(1, "q2admin->ReadLevel", 0, NULL);
}
