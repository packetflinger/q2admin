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

#include "platform.h"
#include "shared.h"

// define GAME_INCLUDE so that game.h does not define the
// short, server-visible gclient_t and edict_t structures,
// because we define the full size ones in this file
#define GAME_INCLUDE

#include "game.h"
#include "g_cloud.h"
#include <ctype.h>
#include "regex.h"
#include "g_json.h"
#include "g_net.h"
#include "g_admin.h"
#include "g_anticheat.h"
#include "g_ban.h"
#include "g_base64.h"
#include "g_checkvar.h"
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

#define PRIVATE_COMMANDS               8
#define ALLOWED_MAXCMDS                50
#define ALLOWED_MAXCMDS_SAFETY         45

// maximum length of random strings used to check for hacked quake2.exe
#define RANDOM_STRING_LENGTH           20


#define G_Malloc(x)                    (gi.TagMalloc(x, TAG_GAME))

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

typedef enum {
    WEAPON_READY,
    WEAPON_ACTIVATING,
    WEAPON_DROPPING,
    WEAPON_FIRING
} weaponstate_t;

//deadflag
#define DEAD_NO                        0
#define DEAD_DYING                     1
#define DEAD_DEAD                      2
#define DEAD_RESPAWNABLE               3

// armor types
#define ARMOR_NONE                     0
#define ARMOR_JACKET                   1
#define ARMOR_COMBAT                   2
#define ARMOR_BODY                     3
#define ARMOR_SHARD                    4

// Contains the map entities string. This string is potentially a different
// size than the original entities string due to cvar-based substitutions.
extern char *finalentities;

extern game_import_t gi;
extern game_export_t ge;

extern edict_t *g_edicts;

#define random()                       ((rand () & 0x7fff) / ((float)0x7fff))

typedef struct {
    // fixed data
    vec3_t      start_origin;
    vec3_t      start_angles;
    vec3_t      end_origin;
    vec3_t      end_angles;

    int         sound_start;
    int         sound_middle;
    int         sound_end;

    float       accel;
    float       speed;
    float       decel;
    float       distance;

    float       wait;

    // state data
    int         state;
    vec3_t      dir;
    float       current_speed;
    float       move_speed;
    float       next_speed;
    float       remaining_distance;
    float       decel_distance;
    void        (*endfunc)(edict_t *);
} moveinfo_t;

typedef struct {
    void    (*aifunc)(edict_t *self, float dist);
    float   dist;
    void    (*thinkfunc)(edict_t *self);
} mframe_t;

typedef struct {
    int         firstframe;
    int         lastframe;
    mframe_t    *frame;
    void        (*endfunc)(edict_t *self);
} mmove_t;

typedef struct {
    mmove_t     *currentmove;
    int         aiflags;
    int         nextframe;
    float       scale;

    void        (*stand)(edict_t *self);
    void        (*idle)(edict_t *self);
    void        (*search)(edict_t *self);
    void        (*walk)(edict_t *self);
    void        (*run)(edict_t *self);
    void        (*dodge)(edict_t *self, edict_t *other, float eta);
    void        (*attack)(edict_t *self);
    void        (*melee)(edict_t *self);
    void        (*sight)(edict_t *self, edict_t *other);
    qboolean    (*checkattack)(edict_t *self);

    float       pausetime;
    float       attack_finished;

    vec3_t      saved_goal;
    float       search_time;
    float       trail_time;
    vec3_t      last_sighting;
    int         attack_state;
    int         lefty;
    float       idle_time;
    int         linkcount;

    int         power_armor_type;
    int         power_armor_power;
} monsterinfo_t;

typedef struct gitem_s {
    char        *classname; // spawning name
    qboolean    (*pickup)(struct edict_s *ent, struct edict_s *other);
    void        (*use)(struct edict_s *ent, struct gitem_s *item);
    void        (*drop)(struct edict_s *ent, struct gitem_s *item);
    void        (*weaponthink)(struct edict_s *ent);
    char        *pickup_sound;
    char        *world_model;
    int         world_model_flags;
    char        *view_model;

    // client side info
    char        *icon;
    char        *pickup_name;   // for printing on pickup
    int         count_width;    // number of digits to display by icon

    int         quantity;       // for ammo how much, for weapons how much is used per shot
    char        *ammo;          // for weapons
    int         flags;          // IT_* flags

    int         weapmodel;      // weapon model index (for weapons)

    void        *info;
    int         tag;

    char        *precaches;     // string of all models, sounds, and images this item will use
} gitem_t;


// client data that stays across multiple level loads
typedef struct {
    char        userinfo[MAX_INFO_STRING];
    char        netname[16];
    int         hand;

    qboolean    connected;  // a loadgame will leave valid entities that
                            // just don't have a connection yet

    // values saved and restored from edicts when changing levels
    int         health;
    int         max_health;
    int         savedFlags;

    int         selected_item;
    int         inventory[MAX_ITEMS];

    // ammo capacities
    int         max_bullets;
    int         max_shells;
    int         max_rockets;
    int         max_grenades;
    int         max_cells;
    int         max_slugs;

    gitem_t     *weapon;
    gitem_t     *lastweapon;

    int         power_cubes;    // used for tracking the cubes in coop games
    int         score;          // for calculating total unit score in coop games

    int         game_helpchanged;
    int         helpchanged;

    qboolean    spectator;          // client is a spectator
} client_persistant_t;


// client data that stays across deathmatch respawns
typedef struct {
    client_persistant_t coop_respawn;   // what to set client->pers to on a respawn
    int         enterframe;         // level.framenum the client entered the game
    int         score;              // frags, etc
    vec3_t      cmd_angles;         // angles sent over in the last command

    qboolean    spectator;          // client is a spectator
} client_respawn_t;


// this structure is cleared on each PutClientInServer(),
// except for 'client->pers'

struct gclient_s {
    // known to server
    player_state_t ps; // communicated by server to clients
    int ping;
	
	// private to game
    client_persistant_t pers;
    client_respawn_t    resp;
    pmove_state_t       old_pmove;  // for detecting out-of-pmove changes

    qboolean    showscores;         // set layout stat
    qboolean    showinventory;      // set layout stat
    qboolean    showhelp;
    qboolean    showhelpicon;

    int         ammo_index;

    int         buttons;
    int         oldbuttons;
    int         latched_buttons;

    qboolean    weapon_thunk;

    gitem_t     *newweapon;

    // sum up damage over an entire frame, so
    // shotgun blasts give a single big kick
    int         damage_armor;       // damage absorbed by armor
    int         damage_parmor;      // damage absorbed by power armor
    int         damage_blood;       // damage taken out of health
    int         damage_knockback;   // impact damage
    vec3_t      damage_from;        // origin for vector calculation

    float       killer_yaw;         // when dead, look at killer

    weaponstate_t   weaponstate;
    vec3_t      kick_angles;    // weapon kicks
    vec3_t      kick_origin;
    float       v_dmg_roll, v_dmg_pitch, v_dmg_time;    // damage kicks
    float       fall_time, fall_value;      // for view drop on fall
    float       damage_alpha;
    float       bonus_alpha;
    vec3_t      damage_blend;
    vec3_t      v_angle;            // aiming direction
    float       bobtime;            // so off-ground doesn't change it
    vec3_t      oldviewangles;
    vec3_t      oldvelocity;

    float       next_drown_time;
    int         old_waterlevel;
    int         breather_sound;

    int         machinegun_shots;   // for weapon raising

	// animation vars
    int         anim_end;
    int         anim_priority;
    qboolean    anim_duck;
    qboolean    anim_run;

    // powerup timers
    int         quad_framenum;
    int         invincible_framenum;
    int         breather_framenum;
    int         enviro_framenum;

    qboolean    grenade_blew_up;
    float       grenade_time;
    int         silencer_shots;
    int         weapon_sound;

    float       pickup_msg_time;

    float       flood_locktill;     // locked from talking
    float       flood_when[10];     // when messages were said
    int         flood_whenhead;     // head pointer for when said

    float       respawn_time;       // can respawn when time > this

    edict_t     *chase_target;      // player we are chasing
    qboolean    update_chase;       // need to update chase info?
};

struct edict_s {
    entity_state_t s;
    struct gclient_s *client; // NULL if not a player
    // the server expects the first part
    // of gclient_s to be a player_state_t
    // but the rest of it is opaque
    qboolean inuse;
    int linkcount;

    // FIXME: move these fields to a server private sv_entity_t
    link_t area; // linked to a division node or leaf

    int num_clusters; // if -1, use headnode instead
    int clusternums[MAX_ENT_CLUSTERS];
    int headnode; // unused if num_clusters != -1
    int areanum, areanum2;

    //================================

    int svflags;
    vec3_t mins, maxs;
    vec3_t absmin, absmax, size;
    solid_t solid;
    int clipmask;
    edict_t *owner;

    // DO NOT MODIFY ANYTHING ABOVE THIS, THE SERVER
    // EXPECTS THE FIELDS IN THAT ORDER!
    //================================
	    //================================
    int         movetype;
    int         flags;

    char        *model;
    float       freetime;           // sv.time when the object was freed

    //
    // only used locally in game, not by server
    //
    char        *message;
    char        *classname;
    int         spawnflags;

    float       timestamp;

    float       angle;          // set in qe3, -1 = up, -2 = down
    char        *target;
    char        *targetname;
    char        *killtarget;
    char        *team;
    char        *pathtarget;
    char        *deathtarget;
    char        *combattarget;
    edict_t     *target_ent;

    float       speed, accel, decel;
    vec3_t      movedir;
    vec3_t      pos1, pos2;

    vec3_t      velocity;
    vec3_t      avelocity;
    int         mass;
    float       air_finished;
    float       gravity;        // per entity gravity multiplier (1.0 is normal)
                                // use for lowgrav artifact, flares

    edict_t     *goalentity;
    edict_t     *movetarget;
    float       yaw_speed;
    float       ideal_yaw;

    float       nextthink;
    void        (*prethink)(edict_t *ent);
    void        (*think)(edict_t *self);
    void        (*blocked)(edict_t *self, edict_t *other);         // move to moveinfo?
    void        (*touch)(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf);
    void        (*use)(edict_t *self, edict_t *other, edict_t *activator);
    void        (*pain)(edict_t *self, edict_t *other, float kick, int damage);
    void        (*die)(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point);

    float       touch_debounce_time;        // are all these legit?  do we need more/less of them?
    float       pain_debounce_time;
    float       damage_debounce_time;
    float       fly_sound_debounce_time;    // move to clientinfo
    float       last_move_time;

    int         health;
    int         max_health;
    int         gib_health;
    int         deadflag;
    qboolean    show_hostile;

    float       powerarmor_time;

    char        *map;           // target_changelevel

    int         viewheight;     // height above origin where eyesight is determined
    int         takedamage;
    int         dmg;
    int         radius_dmg;
    float       dmg_radius;
    int         sounds;         // make this a spawntemp var?
    int         count;

    edict_t     *chain;
    edict_t     *enemy;
    edict_t     *oldenemy;
    edict_t     *activator;
    edict_t     *groundentity;
    int         groundentity_linkcount;
    edict_t     *teamchain;
    edict_t     *teammaster;
	
    edict_t     *mynoise;       // can go in client only
    edict_t     *mynoise2;

    int         noise_index;
    int         noise_index2;
    float       volume;
    float       attenuation;

    // timing variables
    float       wait;
    float       delay;          // before firing targets
    float       random;

    float       teleport_time;

    int         watertype;
    int         waterlevel;

    vec3_t      move_origin;
    vec3_t      move_angles;

    // move this to clientinfo?
    int         light_level;

    int         style;          // also used as areaportal number

    gitem_t     *item;          // for bonus items

    // common data blocks
    moveinfo_t      moveinfo;
    monsterinfo_t   monsterinfo;
};


#define MAXIMPULSESTOTEST 256

#define RANDCHAR()      (random() < 0.3) ? '0' + (int)(9.9 * random()) : 'A' + (int)(26.9 * random())

#define CFGFILE         "q2admin.cfg"

#define DEFAULTRECONNECTMSG     "Please wait to be reconnected to the server - this is normal for this level of bot protection.\nThe fastest way to do this is not to change any client info e.g. your name or skin."

#define DEFAULTUSERDISPLAY      "%s is using a client side proxy."
#define DEFAULTTSDISPLAY        "%s is using a speed cheat."
#define DEFAULTHACKDISPLAY      "%s is using a modified client."
#define DEFAULTSKINCRASHMSG     "%s tried to crash the server."
#define DEFAULTCL_PITCHSPEED_KICKMSG    "cl_pitchspeed changes not allowed on this server."
#define DEFAULTCL_ANGLESPEEDKEY_KICKMSG "cl_anglespeedkey changes not allowed on this server."
#define DEFAULTBANMSG           "You are banned from this server!"
#define DEFAULTLOCKOUTMSG       "This server is currently locked."

typedef struct {
    byte command;
    float timeout;
    unsigned long data;
    char *str;
} cmd_queue_t;

// not used yet
struct ip_cache_s {
    byte ip[16];                // enough room for ipv6
    int masklen;                // cidr len
    int ttl;                    // seconds this entry is valid
    qboolean v6;                // is it an IPv6 address?
    struct ip_cache_s *prev;
    struct ip_cache_s *next;
};

typedef struct ip_cache_s ip_cache_t;
extern ip_cache_t *ipcache;

typedef struct {
    qboolean admin;
    unsigned char retries;
    unsigned char rbotretries;
    cmd_queue_t cmdQueue[ALLOWED_MAXCMDS]; // command queue
    int maxCmds;
    unsigned long clientcommand; // internal proxy commands
    char teststr[9];
    int charindex;
    //long   logfilereadpos;
    int logfilenum;
    long logfilecheckpos;
    char buffer[256]; // log buffer
    byte impulse;
    byte inuse;
    char name[16];
    char skin[40]; // skin/model information.
    int rate;
    int maxfps;
    int cl_pitchspeed;
    float cl_anglespeedkey;
    baninfo_t *baninfo;
    long namechangetimeout;
    int namechangecount;
    long skinchangetimeout;
    int skinchangecount;
    long chattimeout;
    int chatcount;
    char userinfo[MAX_INFO_STRING + 45];
    FILE *stuffFile;
    int impulsesgenerated;
    char lastcmd[8192];
    struct chatflood_s floodinfo;
    short zbc_angles[2][2];
    int zbc_tog;
    int zbc_jitter;
    float zbc_jitter_time;
    float zbc_jitter_last;
    int votescast;
    int votetimeout;
    int msg;

    // used to test the alias (and connect) command with random strings
    char hack_teststring1[RANDOM_STRING_LENGTH + 1];
    char hack_teststring2[RANDOM_STRING_LENGTH + 1];
    char hack_teststring3[RANDOM_STRING_LENGTH + 1];
    char hack_timescale[RANDOM_STRING_LENGTH + 1];
    int hacked_disconnect;
    netadr_t hacked_disconnect_addr;
    int checked_hacked_exe;

    // used to test the variables check list
    char hack_checkvar[RANDOM_STRING_LENGTH + 1];
    int checkvar_idx;

    char gl_driver[256];
    int gl_driver_changes;
    int pmodver;
    int pmod;
    int pmod_noreply_count;
    int pcmd_noreply_count;
    int pver;
    int q2a_admin;
    int q2a_bypass;
    int msec_count;
    int msec_last;
    int frames_count;
    int msec_bad;
    float msec_start;
    int done_server_and_blocklist;
    int userinfo_changed_count;
    int userinfo_changed_start;
    int private_command;
    int timescale;
    qboolean show_fps;
    qboolean vid_restart;
    qboolean private_command_got[PRIVATE_COMMANDS];
    char serverip[16];
    char cmdlist_stored[256];
    int cmdlist;
    int cmdlist_timeout;
    int userid;
    int newcmd_timeout;
    timers_t timers[TIMERS_MAX];
    int blocklist;
    int speedfreeze;
    int enteredgame;
    edict_t *ent;	// the actual entity
    int remote_reported;
    int next_report;
    int stifle_frame;   // frames
    int stifle_length;  // frames
    download_t dl;
    vpn_t vpn;
    netadr_t address;
    char address_str[135]; // string rep, ipv4/ipv6:port
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
#define CCMD_SELECTED               BIT(6)
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

#define STIFLE_TIME                 SECS_TO_FRAMES(60)

#define LEVELCHANGE_KEEP   (CCMD_SCSILENCE | CCMD_CSILENCE | CCMD_PCSILENCE | CCMD_ZBOTDETECTED | CCMD_KICKED | CCMD_NITRO2PROXY | CCMD_ZBOTCLEAR | CCMD_RBOTCLEAR | CCMD_BANNED | CCMD_RECONNECT | CCMD_REMEMBERHACK )
#define BANCHECK     (CCMD_BANNED | CCMD_RECONNECT)

enum _commands {
    QCMD_STARTUP,
    QCMD_STARTUPTEST,
    QCMD_CLEAR,
    QCMD_DISCONNECT,
    QCMD_CUSTOM,
    QCMD_ZPROXYCHECK1,
    QCMD_ZPROXYCHECK2,
    QCMD_DISPLOGFILE,
    QCMD_DISPLOGFILELIST,
    QCMD_DISPLOGEVENTLIST,
    QCMD_CONNECTCMD,
    QCMD_LOGTOFILE1,
    QCMD_LOGTOFILE2,
    QCMD_LOGTOFILE3,
    QCMD_GETIPALT,
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
    QCMD_TESTCMDQUEUE
};

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

#define getEntOffset(ent)   (((char *)ent - (char *)ge.edicts) / ge.edict_size)
#define getEnt(entnum)      (edict_t *)((char *)ge.edicts + (ge.edict_size * entnum))

// where the command can't be run?
#define CMDWHERE_CFGFILE        BIT(0)
#define CMDWHERE_CLIENTCONSOLE  BIT(1)
#define CMDWHERE_SERVERCONSOLE  BIT(2)

// type of command
#define CMDTYPE_NONE        0
#define CMDTYPE_LOGICAL     1   // boolean
#define CMDTYPE_NUMBER      2
#define CMDTYPE_STRING      3

typedef void CMDRUNFUNC(int startarg, edict_t *ent, int client);
typedef void CMDINITFUNC(char *arg);

typedef struct {
    char *cmdname;
    byte cmdwhere;
    byte cmdtype;
    void *datapoint;
    CMDRUNFUNC *runfunc;
    CMDINITFUNC *initfunc;
} q2acmd_t;

extern game_import_t gi;        // server access from inside game lib
extern game_export_t ge;        // game access from inside server
extern game_export_t *ge_mod;   // real game access from inside proxy game lib

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

extern qboolean soloadlazy;
extern qboolean dllloaded;
extern qboolean quake2dirsupport;
extern qboolean zbotdetect;
extern qboolean displayzbotuser;
extern qboolean displaynamechange;
extern qboolean dopversion;
extern qboolean disconnectuserimpulse;
extern qboolean disconnectuser;
extern qboolean mapcfgexec;
extern qboolean checkClientIpAddress;
extern qboolean votecountnovotes;

extern int votepasspercent;
extern int voteminclients;
extern int clientMaxVoteTimeout;
extern int clientMaxVotes;
extern int numofdisplays;
extern int maximpulses;

// cloud admin config
extern qboolean cloud_enabled;
extern char     cloud_address[256];
extern int      cloud_port;
extern qboolean cloud_encryption;
extern char     cloud_privatekey[256];
extern char     cloud_publickey[256];
extern char     cloud_serverkey[256];
extern char     cloud_uuid[37];
extern char     cloud_dns[3];
extern int      cloud_flags;
extern char     cloud_cmd_teleport[25];
extern char     cloud_cmd_invite[25];
extern char     cloud_cmd_seen[25];
extern char     cloud_cmd_whois[25];

// VPN detection
extern qboolean vpn_kick;
extern qboolean vpn_enable;
extern char     vpn_api_key[33];
extern char     vpn_host[50];

// 0 no limit, positive normal limit, negative vpn limit
extern int      ip_limit;

extern char     http_cacert_path[256];
extern qboolean http_debug;
extern qboolean http_enable;
extern qboolean http_verifyssl;


extern byte impulsesToKickOn[MAXIMPULSESTOTEST];
extern byte maxImpulses;

extern qboolean displayimpulses;

//r1ch 2005-01-26 disable hugely buggy commands BEGIN
/*extern qboolean   play_team_enable;
extern qboolean   play_all_enable;
extern qboolean   play_person_enable;*/
//r1ch 2005-01-26 disable hugely buggy commands END

extern qboolean printmessageonplaycmds;
extern qboolean say_person_enable;
extern qboolean say_group_enable;
extern qboolean extendedsay_enable;
extern qboolean spawnentities_enable;
extern qboolean spawnentities_internal_enable;
extern qboolean vote_enable;
extern qboolean consolechat_disable;
extern qboolean gamemaptomap;
extern qboolean banOnConnect;
extern qboolean lockDownServer;
extern qboolean serverinfoenable;

extern int clientVoteTimeout;
extern int clientRemindTimeout;
extern int randomwaitreporttime;
extern int proxy_bwproxy;
extern int proxy_nitro2;
extern int q2adminrunmode;
extern int maxMsgLevel;

extern char zbotmotd[256];
extern char motd[4096];
extern char clientVoteCommand[256];

extern int maxrateallowed;
extern int minrateallowed;
extern int maxfpsallowed;
extern int minfpsallowed;
extern int zbc_jittermax;
extern int zbc_jittertime;
extern int zbc_jittermove;

#define ZBOT_TESTSTRING1            "q2startxx\n"

#define ZBOT_TESTSTRING_TEST1       "q2startxx"
#define ZBOT_TESTSTRING_TEST2       "q2exx"
#define ZBOT_TESTSTRING_TEST3       ".please.disconnect.all.bots"

#define ZBOT_TESTSTRING_TEST1_OLD   "q2start"
#define ZBOT_TESTSTRING_TEST2_OLD   "q2e"

extern char zbot_teststring1[];
extern char zbot_teststring_test1[];
extern char zbot_teststring_test2[];
extern char zbot_teststring_test3[];
extern char zbot_testchar1;
extern char zbot_testchar2;
extern char testchars[];

extern int testcharslength;
extern int q2adminrunmode;
extern int maxclientsperframe;
extern int framesperprocess;

extern char buffer[0x10000];
extern char buffer2[256];
extern char adminpassword[256];
extern char customServerCmd[256];
extern char customClientCmd[256];
extern char customClientCmdConnect[256];
extern char customServerCmdConnect[256];

extern qboolean rcon_insecure;
extern qboolean rcon_random_password;
extern qboolean zbc_enable;
extern qboolean nameChangeFloodProtect;
extern qboolean skinChangeFloodProtect;

extern char nameChangeFloodProtectMsg[256];
extern char skinChangeFloodProtectMsg[256];
extern char chatFloodProtectMsg[256];

extern int maxlrcon_cmds;
extern int lrcon_timeout;
extern int logfilecheckcount;
extern int nameChangeFloodProtectNum;
extern int nameChangeFloodProtectSec;
extern int nameChangeFloodProtectSilence;
extern int skinChangeFloodProtectNum;
extern int skinChangeFloodProtectSec;
extern int skinChangeFloodProtectSilence;

extern struct chatflood_s floodinfo;

extern baninfo_t *banhead;
extern chatbaninfo_t *cbanhead;

extern qboolean IPBanning_Enable;
extern qboolean NickBanning_Enable;
extern qboolean ChatBanning_Enable;
extern qboolean kickOnNameChange;
extern qboolean disablecmds_enable;
extern qboolean checkvarcmds_enable;
extern qboolean swap_attack_use;
extern qboolean timescaledetect;
extern qboolean fpsFloodExempt;

extern char defaultBanMsg[256];
extern char defaultChatBanMsg[256];
extern char *currentBanMsg;

extern proxyinfo_t *proxyinfo;
extern proxyinfo_t *proxyinfoBase;
extern proxyreconnectinfo_t *reconnectproxyinfo;
extern q2acmd_t q2aCommands[];

extern int clientsidetimeout;
extern int zbotdetectactivetimeout;
extern int lframenum;

extern float ltime;

extern char *impulsemessages[];
extern char cmdpassedvote[2048];
extern char cl_pitchspeed_kickmsg[256];
extern char cl_anglespeedkey_kickmsg[256];

extern qboolean cl_pitchspeed_enable;
extern qboolean cl_pitchspeed_kick;
extern qboolean cl_pitchspeed_display;
extern qboolean cl_anglespeedkey_enable;
extern qboolean cl_anglespeedkey_kick;
extern qboolean cl_anglespeedkey_display;
extern qboolean filternonprintabletext;

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

#ifndef FALSE
#define FALSE   0
#endif
#ifndef TRUE
#define TRUE   1
#endif

#define SKIPBLANK(str) \
{\
    while(*str == ' ' || *str == '\t') \
    { \
        str++; \
    } \
}

#define RATBOT_CHANGENAMETEST "pwsnskle"
#define BOTDETECT_CHAR1   'F'
#define BOTDETECT_CHAR2   'U'

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

// zb_cmd.c
void readCfgFiles(void);
qboolean readCfgFile(char *cfgfilename);
void ClientCommand(edict_t *ent);
void ServerCommand(void);
void dprintf_internal(char *fmt, ...);
void cprintf_internal(edict_t *ent, int printlevel, char *fmt, ...);
void bprintf_internal(int printlevel, char *fmt, ...);
void AddCommandString_internal(char *text);
void stuffNextLine(edict_t *ent, int client);
char *getArgs(void);
int getClientsFromArg(int client, edict_t *ent, char *cp, char **text);
edict_t *getClientFromArg(int client, edict_t *ent, int *cleintret, char *cp, char **text);
void Cmd_Teleport_f(edict_t *ent);

// zb_zbot.c
int checkForOverflows(edict_t *ent, int client);
void serverLogZBot(edict_t *ent, int client);
void ClientThink(edict_t *ent, usercmd_t *ucmd);
void G_RunFrame(void);
void Pmove_internal(pmove_t *pmove);
void readIpFromLog(int client, edict_t *ent);

// zb_zbotcheck.c
qboolean zbc_ZbotCheck(int client, usercmd_t *ucmd);

extern char client_msg[256];
extern qboolean private_command_kick;
extern int msec_kick_on_bad;
extern int msec_max;
extern int speedbot_check_type;
extern int max_pmod_noreply;
extern int msec_int;

// should be set at build time in Makefile
#ifndef Q2A_COMMIT
#define Q2A_COMMIT      "00~000000"
#define Q2A_REVISION    0
#endif

#define DEFAULTQ2AMSG   "\nThis server requires %s anti cheat client.\n"

#define MAX_BLOCK_MODELS    26

#define VIRUS_KICK_MSG      "%s has not provided adequate authentication, this may be due to a virus.\n"
#define NOMATCH_KICK_MSG    "%s has not provided adequate authentication.\n"
#define OUTOFTIME_KICK_MSG  "%s failed to authenticate.\n"
#define MOD_KICK_MSG        "%s failed the pmodified check on this map, error %d.\n"
#define PRV_KICK_MSG        "%s is using a modified client.\n"

#define Q2A_VIRUS_KICK_MSG      "Inadequate authentication, may be a virus.\n"
#define Q2A_NOMATCH_KICK_MSG    "Inadequate authentication.\n"
#define Q2A_OUTOFTIME_KICK_MSG  "Failed to respond.\n"
#define Q2A_MOD_KICK_MSG        "Failed the pmodified check on this map.\n"
#define Q2A_PRV_KICK_MSG        "Failed the private commands check.\n"
#define FRKQ2_KICK_MSG          "Failed the client authentication.\n"

extern admin_type admin_pass[MAX_ADMINS];
extern admin_type q2a_bypass_pass[MAX_ADMINS];
extern int num_admins;
extern int num_q2a_admins;

extern int client_map_cfg;
extern qboolean do_franck_check;
extern qboolean q2a_command_check;
extern qboolean do_vid_restart;

extern int gl_driver_check;
extern int USERINFOCHANGE_TIME;
extern int USERINFOCHANGE_COUNT;
extern int gl_driver_max_changes;

typedef struct {
    char command[256];
} priv_t;

extern priv_t private_commands[PRIVATE_COMMANDS];
void stuff_private_commands(int client, edict_t *ent);

extern int WHOIS_COUNT;
extern int whois_active;
extern user_details* whois_details;
extern qboolean timers_active;
extern int timers_min_seconds;
extern int timers_max_seconds;


typedef struct {
    char *model_name;
} block_model;

extern block_model block_models[MAX_BLOCK_MODELS];

