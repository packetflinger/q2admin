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

#include <stdbool.h>
#include "platform.h"
#include "shared.h"
#include "game.h"
#include "g_regex.h"
#include "g_cloud.h"
#include <ctype.h>
#include "g_json.h"
#include "g_net.h"
#include "g_admin.h"
#include "g_anticheat.h"
#include "g_ban.h"
#include "g_checkvar.h"
#include "g_client.h"
#include "g_cmd.h"
#include "g_crypto.h"
#include "g_disable.h"
#include "g_flood.h"
#include "g_http.h"
#include "g_init.h"
#include "g_log.h"
#include "g_lrcon.h"
#include "g_queue.h"
#include "g_spawn.h"
#include "g_timer.h"
#include "g_util.h"
#include "g_vote.h"
#include "g_vpn.h"
#include "g_whois.h"
#include "profile.h"

#define CFGFILE         "q2admin.cfg"

#define NAME(x)         (proxyinfo[x].name)
#define VALIDCLIENT(i)  (i >= 0 || i < (int)maxclients->value) // proxyinfo index

extern game_import_t gi;        // server access from inside game lib
extern game_export_t ge;        // game access from inside server
extern game_export_t *ge_mod;   // real game access from inside proxy game lib
extern edict_t *g_edicts;

#define getEntOffset(ent)   (((char *)ent - (char *)ge.edicts) / ge.edict_size)
#define getEnt(entnum)      (edict_t *)((char *)ge.edicts + (ge.edict_size * entnum))

#define PRIVATE_COMMANDS               8
#define ALLOWED_MAXCMDS                50
#define ALLOWED_MAXCMDS_SAFETY         45

// maximum length of random strings used to check for hacked quake2.exe
#define RANDOM_STRING_LENGTH           20

#define MAX_VERSION_CHARS              100

// protocol bytes that can be directly added to messages
#define SVC_STUFFTEXT                  11

// variable server FPS
#if USE_FPS
  #define HZ            game.framerate
  #define FRAMETIME     game.frametime
  #define FRAMEDIV      game.framediv
  #define FRAMESYNC     !(level.framenum % game.framediv)
#else
  #define HZ            BASE_FRAMERATE
  #define FRAMETIME     BASE_FRAMETIME_1000
  #define FRAMEDIV      1
  #define FRAMESYNC     1
#endif

#define SECS_TO_FRAMES(seconds)        (int)((seconds) * HZ)
#define FRAMES_TO_SECS(frames)         (int)((frames) * FRAMETIME)

// memory tags to allow dynamic memory to be cleaned up
#define TAG_GAME                       765  // clear when unloading the dll
#define TAG_LEVEL                      766  // clear when loading a new level

#define G_Malloc(x)                    (gi.TagMalloc(x, TAG_GAME))
#define G_Free(x)                      (gi.TagFree(x))

// Contains the map entities string. This string is potentially a different
// size than the original entities string due to cvar-based substitutions.
extern char *finalentities;

#define random()        ((rand() & 0x7fff) / ((float)0x7fff)) // between 0 and 1
#define RANDCHAR()      (random() < 0.3) ? '0' + (int)(9.9 * random()) : 'A' + (int)(26.9 * random())

#define DEFAULTLOCKOUTMSG       "This server is currently locked."

typedef struct {
    bool admin;                     // player authed with !admin command
    unsigned char retries;
    unsigned char rbotretries;
    cmd_queue_t cmdQueue[ALLOWED_MAXCMDS];
    int maxCmds;                    // current command count
    unsigned long clientcommand;    // internal proxy commands
    char teststr[9];
    int charindex;                  // teststr index AND cheat type for logging
    int logfilenum;                 // for displaying log files
    char buffer[256];               // log buffer
    byte impulse;                   // player's ucmd->impulse
    byte inuse;                     // used by active player
    char name[16];                  // player name
    baninfo_t *baninfo;             // the ban entry that matched this player
    long chattimeout;
    int chatcount;
    FILE *stuffFile;                // fp for if "!stuff FILE" is used
    int impulsesgenerated;          // cumulative total
    char lastcmd[8192];             // the latest command sent including args
    struct chatflood_s floodinfo;
    aimbot_t aim_assist;            // for checking if player has an aimbot
    int votescast;                  // cumulative total votes proposed
    int votetimeout;                // can't propose until ltime is greater

    // used to test the alias (and connect) command with random strings
    char hack_teststring1[RANDOM_STRING_LENGTH + 1];
    char hack_teststring2[RANDOM_STRING_LENGTH + 1];
    char hack_teststring3[RANDOM_STRING_LENGTH + 1];
    char hack_timescale[RANDOM_STRING_LENGTH + 1];
    int hacked_disconnect;          // hack detected, disconnect the client
    netadr_t hacked_disconnect_addr;
    int checked_hacked_exe;         // used in `rate` check

    // used to test the variables check list
    char hack_checkvar[RANDOM_STRING_LENGTH + 1];
    int checkvar_idx;

    char gl_driver[256];            // original GL drive reported
    int gl_driver_changes;          // cumulative total
    int pmodver;                    // p_modified check (ltime related)
    int pmod;                       // seemingly unused, checked but not set
    int pmod_noreply_count;
    int pcmd_noreply_count;
    int pver;                       // seemingly unused, set but never checked
    int admin_level;                // admin level granted
    int bypass_level;               // bypass level granted
    int frames_count;               // used in ClientThink for FPS display
    player_msec_t msec;
    int done_server_and_blocklist;
    int private_command;            // ltime while waiting for responses
    int timescale;                  // player's current timescale (from userinfo)
    bool show_fps;              // display fps to client every 0.5 sec
    bool vid_restart;           // if client already forced to vid_restart
    bool private_command_got[PRIVATE_COMMANDS]; // if we received a response
    char serverip[16];              // have player reconnect here instead of proxy
    char cmdlist_stored[256];
    int cmdlist;
    int cmdlist_timeout;
    int userid;                     // unique ID for whois stuff
    int newcmd_timeout;
    timers_t timers[TIMERS_MAX];
    int blocklist;
    int speedfreeze;                // level time, hold until ltime is greater
    int enteredgame;                // level time entered (seconds)
    edict_t *ent;                   // the associated player entity
    int remote_reported;            // unused, remove later
    int next_report;                // unused, remove later
    int stifle_frame;               // frames
    int stifle_length;              // frames
    download_t dl;
    vpn_t vpn;                      // vpn status for player
    netadr_t address;               // player's IP address (IPv4 or IPv6)
    netadr_t network;               // address' parent CIDR range
    char auton_sys_num[10];         // the ASN announcing this network
    char address_str[135];          // unused, remove later
    char version_test[6];           // random chars to force sending version
    char client_version[MAX_VERSION_CHARS];   // build string
    chatpest_t pest;                // tracking annoying chat behavior
    freeze_t freeze;                // used to freeze a player in place
    userinfo_t userinfo;
} proxyinfo_t;

typedef struct {
    byte inuse;
    char name[16];
} proxyreconnectinfo_t;

#define MAXDETECTRETRIES   3

#define CCMD_STARTUPTEST            BIT(0)
#define CCMD_ZPROXYCHECK2           BIT(1)
#define CCMD_ZBOTDETECTED           BIT(2)
#define CCMD_BANNED                 BIT(3)
#define CCMD_NCSILENCE              BIT(4)  // name change silence
#define CCMD_KICKED                 BIT(5)
#define CCMD_SELECTED               BIT(6)  // player targeted in cmd
#define CCMD_CSILENCE               BIT(7)  // temporarily muted
#define CCMD_PCSILENCE              BIT(8)  // permanently muted
#define CCMD_VOTED                  BIT(9)
#define CCMD_VOTEYES                BIT(10)
#define CCMD_NITRO2PROXY            BIT(11)
#define CCMD_RATBOTDETECT           BIT(12)
#define CCMD_RATBOTDETECTNAME       BIT(13)
#define CCMD_ZBOTCLEAR              BIT(14)
#define CCMD_RBOTCLEAR              BIT(15)
#define CCMD_SCSILENCE              BIT(16)  // skin change silence
#define CCMD_RECONNECT              BIT(17)
#define CCMD_ALIASCHECKSTARTED      BIT(18)
#define CCMD_WAITFORALIASREPLY1     BIT(19)
#define CCMD_WAITFORALIASREPLY2     BIT(20)
#define CCMD_WAITFORCONNECTREPLY    BIT(21)
#define CCMD_REMEMBERHACK           BIT(22)
#define CCMD_CLIENTOVERFLOWED       BIT(23)
#define CCMD_STIFLED                BIT(24)  // half-muted
#define CCMD_WAITFORVERSION         BIT(25)

#define LEVELCHANGE_KEEP (CCMD_SCSILENCE | CCMD_CSILENCE | CCMD_PCSILENCE | \
                          CCMD_ZBOTDETECTED | CCMD_KICKED | CCMD_NITRO2PROXY | \
                          CCMD_ZBOTCLEAR | CCMD_RBOTCLEAR | CCMD_BANNED | \
                          CCMD_RECONNECT | CCMD_REMEMBERHACK)

/**
 * These are commands that the server should do for or to the client. Clients
 * have no say or influence on these commands.
 */
enum _commands {
    QCMD_STARTUP,
    QCMD_STARTUPTEST,
    QCMD_CLEAR,                 // clears the client's console/chat area
    QCMD_DISCONNECT,
    QCMD_CUSTOM,                // stuff customClientCmd
    QCMD_ZPROXYCHECK1,
    QCMD_ZPROXYCHECK2,
    QCMD_DISPLOGFILE,
    QCMD_DISPLOGFILELIST,
    QCMD_DISPLOGEVENTLIST,
    QCMD_CONNECTCMD,
    QCMD_LOGTOFILE1,
    QCMD_LOGTOFILE2,
    QCMD_LOGTOFILE3,
    QCMD_RESTART,
    QCMD_CLIPTOMAXRATE,
    QCMD_CLIPTOMINRATE,
    QCMD_SETUPMAXFPS,
    QCMD_FORCEUDATAUPDATE,
    QCMD_SETMAXFPS,
    QCMD_SETMINFPS,
    QCMD_DISPBANS,
    QCMD_DISPLRCONS,
    QCMD_DISPFLOOD,
    QCMD_DISPSPAWN,
    QCMD_DISPVOTE,
    QCMD_DISPDISABLE,
    QCMD_CHANGENAME,
    QCMD_CHANGESKIN,
    QCMD_BAN,
    QCMD_DISPCHATBANS,
    QCMD_STUFFCLIENT,
    QCMD_TESTADMIN,
    QCMD_TESTADMIN2,
    QCMD_TESTADMIN3,
    QCMD_RUNVOTECMD,
    QCMD_TESTRATBOT,
    QCMD_TESTRATBOT2,
    QCMD_TESTRATBOT3,
    QCMD_TESTRATBOT4,
    QCMD_LETRATBOTQUIT,
    QCMD_TESTTIMESCALE,
    QCMD_TESTSTANDARDPROXY,
    QCMD_TESTALIASCMD1,
    QCMD_TESTALIASCMD2,
    QCMD_SETUPCL_PITCHSPEED,
    QCMD_FORCEUDATAUPDATEPS,
    QCMD_SETUPCL_ANGLESPEEDKEY,
    QCMD_FORCEUDATAUPDATEAS,
    QCMD_RECONNECT,
    QCMD_KICK,
    QCMD_MSGDISCONNECT,
    QCMD_DISPCHECKVAR,
    QCMD_CHECKVARTESTS,
    QCMD_AUTH,
    QCMD_PMODVERTIMEOUT,
    QCMD_PMODVERTIMEOUT_INGAME,
    QCMD_SHOWMOTD,
    QCMD_EXECMAPCFG,
    QCMD_PRIVATECOMMAND,
    QCMD_GL_CHECK,
    QCMD_SETUPTIMESCALE,
    QCMD_SETTIMESCALE,
    QCMD_SPAMBYPASS,
    QCMD_GETCMDQUEUE,
    QCMD_TESTCMDQUEUE,
    QCMD_CLIENTVERSION,
    QCMD_FREEZEPLAYER,
    QCMD_UNFREEZEPLAYER
};

// Internal Warnings for logging
#define IW_UNEXCEPTEDCMD        1
#define IW_UNKNOWNCMD           2
#define IW_ZBOTDETECT           3
#define IW_STARTUP              4
#define IW_STARTUPTEST          5
#define IW_ZBOTTEST             6
#define IW_OVERFLOWDETECT       7
#define IW_STARTUPFAIL          8
#define IW_Q2ADMINCFGLOAD       9
#define IW_LOGSETUPLOAD         10
#define IW_BANSETUPLOAD         11
#define IW_LRCONSETUPLOAD       12
#define IW_FLOODSETUPLOAD       13
#define IW_SPAWNSETUPLOAD       14
#define IW_VOTESETUPLOAD        15
#define IW_ZBCHECK              16
#define IW_DISABLESETUPLOAD     17
#define IW_CHECKVARSETUPLOAD    18
#define IW_INVALIDIPADDRESS     19

#define MINIMUMTIMEOUT  5
#define MAXSTARTTRY     500

extern cvar_t *rcon_password,
              *gamedir,
              *maxclients,
              *logfile,
              *rconpassword,
              *port,
              *serverbindip,
              *q2aconfig,
              *q2adminbantxt,
              *q2adminbanremotetxt,
              *q2adminbanremotetxt_enable,
              *q2adminanticheat_enable,
              *q2adminanticheat_file,
              *q2adminhashlist_enable,
              *q2adminhashlist_dir,
              *tune_spawn_railgun,
              *tune_spawn_bfg,
              *tune_spawn_quad,
              *tune_spawn_invulnerability,
              *tune_spawn_powershield,
              *tune_spawn_megahealth,
              *tune_spawn_rocketlauncher,
              *tune_spawn_hyperblaster,
              *tune_spawn_grenadelauncher,
              *tune_spawn_chaingun,
              *tune_spawn_machinegun,
              *tune_spawn_supershotgun,
              *tune_spawn_shotgun,
              *tune_spawn_machinegun,
              *tune_spawn_grenades,
              *configfile_ban,
              *configfile_bypass,
              *configfile_cloud,
              *configfile_cvar,
              *configfile_disable,
              *configfile_flood,
              *configfile_login,
              *configfile_log,
              *configfile_rcon,
              *configfile_spawn,
              *configfile_vote;

extern char dllname[256];
extern char gamelibrary[MAX_QPATH];
extern char zbotuserdisplay[256];
extern char timescaleuserdisplay[256];
extern char hackuserdisplay[256];
extern char skincrashmsg[256];
extern char defaultreconnectmessage[256];
extern char moddir[256];
extern char version[256];

extern bool soloadlazy;
extern bool dllloaded;
extern bool quake2dirsupport;
extern bool zbotdetect;
extern bool displayzbotuser;
extern bool displaynamechange;
extern bool dopversion;
extern bool disconnectuserimpulse;
extern bool disconnectuser;
extern bool mapcfgexec;
extern bool checkClientIpAddress;
extern bool votecountnovotes;

extern int votepasspercent;
extern int voteminclients;
extern int clientMaxVoteTimeout;
extern int clientMaxVotes;
extern int numofdisplays;
extern int maximpulses;

extern int ip_limit;        // 0 for no limit
extern int ip_limit_vpn;    // 0 for no limit, per ASN

extern byte impulsesToKickOn[MAXIMPULSESTOTEST];
extern byte maxImpulses;

extern bool displayimpulses;
extern bool printmessageonplaycmds;
extern bool say_person_enable;
extern bool say_group_enable;
extern bool extendedsay_enable;
extern bool spawnentities_enable;
extern bool spawnentities_internal_enable;
extern bool vote_enable;
extern bool consolechat_disable;
extern bool gamemaptomap;
extern bool banOnConnect;
extern bool lockDownServer;
extern bool serverinfoenable;

extern int clientVoteTimeout;
extern int clientRemindTimeout;
extern int randomwaitreporttime;
extern int proxy_bwproxy;
extern int proxy_nitro2;
extern int maxMsgLevel;

extern bool consolelog_enable;
extern char consolelog_pattern[256];

extern char zbotmotd[256];
extern char motd[4096];
extern char clientVoteCommand[256];

extern int testcharslength;
extern int runmode;
extern int maxclientsperframe;
extern int framesperprocess;

extern char buffer[0x10000];
extern char buffer2[256];
extern char adminpassword[256];
extern char customServerCmd[256];
extern char customClientCmd[256];
extern char customClientCmdConnect[256];
extern char customServerCmdConnect[256];

extern bool rcon_insecure;
extern bool rcon_random_password;
extern bool zbc_enable;

extern int maxlrcon_cmds;
extern int lrcon_timeout;
extern int logfilecheckcount;

extern bool kickOnNameChange;
extern bool disablecmds_enable;
extern bool checkvarcmds_enable;
extern bool swap_attack_use;
extern bool timescaledetect;

extern char *currentBanMsg;

extern proxyinfo_t *proxyinfo;
extern proxyinfo_t *proxyinfoBase;
extern proxyreconnectinfo_t *reconnectproxyinfo;

extern int clientsidetimeout;
extern int zbotdetectactivetimeout;
extern int lframenum;

extern float ltime;

extern char *impulsemessages[];
extern char cmdpassedvote[2048];
extern char cl_pitchspeed_kickmsg[256];
extern char cl_anglespeedkey_kickmsg[256];

extern bool cl_pitchspeed_enable;
extern bool cl_pitchspeed_kick;
extern bool cl_pitchspeed_display;
extern bool cl_anglespeedkey_enable;
extern bool cl_anglespeedkey_kick;
extern bool cl_anglespeedkey_display;
extern bool filternonprintabletext;

extern char lockoutmsg[256];
extern char gmapname[MAX_QPATH];
extern char reconnect_address[256];
extern char serverip[256];
extern char lanip[256];

extern int reconnect_time;
extern int reconnect_checklevel;
extern int entity_classname_offset;
extern int checkvar_poll_time;

typedef struct {
    long reconnecttimeout;
    int retrylistidx;
    char userinfo[MAX_INFO_STRING + 45];
} reconnect_info;

typedef struct {
    long retry;
    char ip[MAX_INFO_STRING + 45];
} retrylist_info;

extern reconnect_info* reconnectlist;
extern retrylist_info* retrylist;
extern int maxReconnectList;
extern int maxretryList;

// zb_clib.c
#ifdef Q2ADMINCLIB
char *q2a_strcpy(char *strDestination, const char *strSource);
char *q2a_strncpy(char *strDest, const char *strSource, size_t count);
char *q2a_strcat(char *strDestination, const char *strSource);
char *q2a_strstr(const char *string, const char *strCharSet);
char *q2a_strchr(const char *string, int c);
int q2a_strcmp(const char *string1, const char *string2);
size_t q2a_strlen(const char *string);
int q2a_atoi(const char *string);
double q2a_atof(const char *string);
int q2a_memcmp(const void *buf1, const void *buf2, size_t count);
void *q2a_memcpy(void *dest, const void *src, size_t count);
void *q2a_memmove(void *dest, const void *src, size_t count);
void *q2a_memset(void *dest, int c, size_t count);
#else
#define q2a_strcpy  strcpy
#define q2a_strncpy strncpy
#define q2a_strcat  strcat
#define q2a_strcmp  strcmp
#define q2a_strstr  strstr
#define q2a_strchr  strchr
#define q2a_strlen  strlen
#define q2a_atoi    atoi
#define q2a_atof    atof
#define q2a_memcmp  memcmp
#define q2a_memcpy  memcpy
#define q2a_memmove memmove
#define q2a_memset  memset
#endif

const char *q2a_inet_ntop (int af, const void *src, char *dst, socklen_t size);

// should be set at build time in Makefile
#ifndef Q2A_COMMIT
#define Q2A_COMMIT      "00~000000"
#define Q2A_REVISION    0
#endif

#define DEFAULTQ2AMSG       "\nThis server requires %s anti cheat client.\n"
#define NOMATCH_KICK_MSG    "%s has not provided adequate authentication.\n"
#define MOD_KICK_MSG        "%s failed the pmodified check on this map, error %d.\n"
#define PRV_KICK_MSG        "%s is using a modified client.\n"
#define Q2A_MOD_KICK_MSG    "Failed the pmodified check on this map.\n"
#define Q2A_PRV_KICK_MSG    "Failed the private commands check.\n"
#define FRKQ2_KICK_MSG      "Failed the client authentication.\n"

extern int client_map_cfg;
extern bool do_franck_check;
extern bool q2a_command_check;
extern bool do_vid_restart;

extern int gl_driver_check;
extern int USERINFOCHANGE_TIME;
extern int USERINFOCHANGE_COUNT;
extern int gl_driver_max_changes;

typedef struct {
    char command[256];
} priv_t;

extern priv_t private_commands[PRIVATE_COMMANDS];
void stuff_private_commands(int client, edict_t *ent);
