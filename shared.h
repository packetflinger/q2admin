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

#ifdef _WIN32
  // unknown pragmas are SUPPOSED to be ignored, but....
  #pragma warning(disable : 4244)     // MIPS
  #pragma warning(disable : 4136)     // X86
  #pragma warning(disable : 4051)     // ALPHA
  #pragma warning(disable : 4018)     // signed/unsigned mismatch
  #pragma warning(disable : 4305)     // truncation from const double to float

  #define snprintf _snprintf
#endif

#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#if defined _M_IX86 && !defined C_ONLY
  #define id386 1
#else
  #define id386 0
#endif

#if defined _M_ALPHA && !defined C_ONLY
  #define idaxp 1
#else
  #define idaxp 0
#endif

#define BIT(n)  (1U << (n))
typedef char byte;
typedef enum {
    qfalse,
    qtrue
} qboolean;

#ifndef min
  #define min(x,y) ((x) < (y)) ? (x) : (y)
#endif

#ifndef NULL
  #define NULL ((void *)0)
#endif

#define lengthof(arr)   (sizeof(arr) / sizeof(arr[0]))

//terminating strncpy
#define Q_strncpy(dst, src, len) \
do { \
    strncpy ((dst), (src), (len)); \
    (dst)[(len)] = 0; \
} while (0)

// angle indexes
#define PITCH   0  // up / down
#define YAW     1  // left / right
#define ROLL    2  // fall over

#define MAX_STRING_CHARS    1024    // max length of a string passed to Cmd_TokenizeString
#define MAX_STRING_TOKENS   80      // max tokens resulting from Cmd_TokenizeString
#define MAX_TOKEN_CHARS     128     // max length of an individual token
#define MAX_QPATH           64  // max length of a quake game pathname
#define MAX_OSPATH          128 // max length of a filesystem pathname

// per-level limits
#define MAX_CLIENTS     256  // absolute limit
#define MAX_EDICTS      1024 // must change protocol to increase more
#define MAX_LIGHTSTYLES 256
#define MAX_MODELS      256  // these are sent over the net as bytes
#define MAX_SOUNDS      256  // so they cannot be blindly increased
#define MAX_IMAGES      256
#define MAX_ITEMS       256

// game print flags
#define PRINT_LOW       0  // pickup messages
#define PRINT_MEDIUM    1  // death messages
#define PRINT_HIGH      2  // critical messages
#define PRINT_CHAT      3  // chat messages
#define PRINT_ALL		0
#define PRINT_DEVELOPER	1  // only print when "developer 1"
#define PRINT_ALERT		2

#define BASE_FRAMERATE          10
#define BASE_FRAMETIME          100
#define BASE_1_FRAMETIME        0.01f   // 1/BASE_FRAMETIME
#define BASE_FRAMETIME_1000     0.1f    // BASE_FRAMETIME/1000

// destination class for gi.multicast()

typedef enum {
    MULTICAST_ALL,
    MULTICAST_PHS,
    MULTICAST_PVS,
    MULTICAST_ALL_R,
    MULTICAST_PHS_R,
    MULTICAST_PVS_R
} multicast_t;

typedef float vec_t;
typedef vec_t vec3_t[3];
typedef vec_t vec5_t[5];
typedef int fixed4_t;
typedef int fixed8_t;
typedef int fixed16_t;

extern vec3_t vec3_origin;

#ifndef M_PI
  #define M_PI  3.14159265358979323846 // matches value in gcc v2 math.h
#endif

struct cplane_s;

#define nanmask (255<<23)
#define IS_NAN(x) (((*(int *)&x)&nanmask)==nanmask)

#if !defined C_ONLY
  extern long Q_ftol(float f);
#else
  #define Q_ftol( f ) ( long ) (f)
#endif

#define DotProduct(x,y)   (x[0]*y[0]+x[1]*y[1]+x[2]*y[2])
#define VectorSubtract(a,b,c) (c[0]=a[0]-b[0],c[1]=a[1]-b[1],c[2]=a[2]-b[2])
#define VectorAdd(a,b,c)  (c[0]=a[0]+b[0],c[1]=a[1]+b[1],c[2]=a[2]+b[2])
#define VectorCopy(a,b)   (b[0]=a[0],b[1]=a[1],b[2]=a[2])
#define VectorClear(a)   (a[0]=a[1]=a[2]=0)
#define VectorNegate(a,b)  (b[0]=-a[0],b[1]=-a[1],b[2]=-a[2])
#define VectorSet(v, x, y, z) (v[0]=(x), v[1]=(y), v[2]=(z))

#define Q_isupper(c)    ((c) >= 'A' && (c) <= 'Z')
#define Q_islower(c)    ((c) >= 'a' && (c) <= 'z')
#define Q_isdigit(c)    ((c) >= '0' && (c) <= '9')
#define Q_isalpha(c)    (Q_isupper(c) || Q_islower(c))
#define Q_isalnum(c)    (Q_isalpha(c) || Q_isdigit(c))
#define Q_isprint(c)    ((c) >= 32 && (c) < 127)
#define Q_isgraph(c)    ((c) > 32 && (c) < 127)
#define Q_isspace(c)    (c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v')
#define Q_ispath(c)     (Q_isalnum(c) || (c) == '_' || (c) == '-')
#define Q_isspecial(c) ((c) == '\r' || (c) == '\n' || (c) == 127)

static inline int Q_tolower(int c) {
    if (Q_isupper(c)) {
        c += ('a' - 'A');
    }
    return c;
}

static inline int Q_toupper(int c) {
    if (Q_islower(c)) {
        c -= ('a' - 'A');
    }
    return c;
}

// make string lower case
static inline char *Q_strlwr(char *s) {
    char *p = s;
    while (*p) {
        *p = Q_tolower(*p);
        p++;
    }
    return s;
}

// make string upper case
static inline char *Q_strupr(char *s) {
    char *p = s;
    while (*p) {
        *p = Q_toupper(*p);
        p++;
    }
    return s;
}

// char dec to hex
static inline int Q_charhex(int c) {
    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    return -1;
}

// converts quake char to ASCII equivalent
static inline int Q_charascii(int c) {
    if (Q_isspace(c)) {
        // white-space chars are output as-is
        return c;
    }
    c &= 127; // strip high bits
    if (Q_isprint(c)) {
        return c;
    }
    switch (c) {
        // handle bold brackets
        case 16: return '[';
        case 17: return ']';
    }
    return '.'; // don't output control chars, etc
}

// key / value info strings
#define MAX_INFO_KEY    64
#define MAX_INFO_VALUE  64
#define MAX_INFO_STRING 512

#ifndef CVAR
  #define CVAR
  #define CVAR_GENERAL      0
  #define CVAR_ARCHIVE      1   // set to cause it to be saved to vars.rc
  #define CVAR_USERINFO     2   // added to userinfo  when changed
  #define CVAR_SERVERINFO   4   // added to serverinfo when changed
  #define CVAR_NOSET        8   // don't allow change from console at all,
                                // but can be set from the command line
  #define CVAR_LATCH        16  // save changes until server restart

  // nothing outside the Cvar_*() functions should modify these fields!
  typedef struct cvar_s {
      char *name;
      char *string;
      char *latched_string;
      int flags;
      qboolean modified; // set each time the cvar is changed
      float value;
      struct cvar_s *next;
  } cvar_t;
#endif  // CVAR

// lower bits are stronger, and will eat weaker brushes completely
#define CONTENTS_SOLID          BIT(0)  // an eye is never valid in a solid
#define CONTENTS_WINDOW         BIT(1)  // translucent, but not watery
#define CONTENTS_AUX            BIT(2)
#define CONTENTS_LAVA           BIT(3)
#define CONTENTS_SLIME          BIT(4)
#define CONTENTS_WATER          BIT(5)
#define CONTENTS_MIST           BIT(6)
#define LAST_VISIBLE_CONTENTS   BIT(6)
#define CONTENTS_AREAPORTAL     BIT(14)
#define CONTENTS_PLAYERCLIP     BIT(15)
#define CONTENTS_MONSTERCLIP    BIT(16)
#define CONTENTS_CURRENT_0      BIT(17)
#define CONTENTS_CURRENT_90     BIT(18)
#define CONTENTS_CURRENT_180    BIT(19)
#define CONTENTS_CURRENT_270    BIT(20)
#define CONTENTS_CURRENT_UP     BIT(21)
#define CONTENTS_CURRENT_DOWN   BIT(22)
#define CONTENTS_ORIGIN         BIT(23) // removed before bsping an entity
#define CONTENTS_MONSTER        BIT(24) // should never be on a brush, only game
#define CONTENTS_DEADMONSTER    BIT(25)
#define CONTENTS_DETAIL         BIT(26) // brushes to be added after vis leafs
#define CONTENTS_TRANSLUCENT    BIT(27) // auto set if any surface has trans
#define CONTENTS_LADDER         BIT(28)

// content masks
#define MASK_ALL    (-1)
#define MASK_SOLID    (CONTENTS_SOLID|CONTENTS_WINDOW)
#define MASK_PLAYERSOLID  (CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_WINDOW|CONTENTS_MONSTER)
#define MASK_DEADSOLID   (CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_WINDOW)
#define MASK_MONSTERSOLID  (CONTENTS_SOLID|CONTENTS_MONSTERCLIP|CONTENTS_WINDOW|CONTENTS_MONSTER)
#define MASK_WATER    (CONTENTS_WATER|CONTENTS_LAVA|CONTENTS_SLIME)
#define MASK_OPAQUE    (CONTENTS_SOLID|CONTENTS_SLIME|CONTENTS_LAVA)
#define MASK_SHOT    (CONTENTS_SOLID|CONTENTS_MONSTER|CONTENTS_WINDOW|CONTENTS_DEADMONSTER)
#define MASK_CURRENT   (CONTENTS_CURRENT_0|CONTENTS_CURRENT_90|CONTENTS_CURRENT_180|CONTENTS_CURRENT_270|CONTENTS_CURRENT_UP|CONTENTS_CURRENT_DOWN)

// plane_t structure
// !!! if this is changed, it must be changed in asm code too !!!
typedef struct cplane_s {
    vec3_t normal;
    float dist;
    byte type; // for fast side tests
    byte signbits; // signx + (signy<<1) + (signz<<1)
    byte pad[2];
} cplane_t;

// structure offset for asm code
#define CPLANE_NORMAL_X     0
#define CPLANE_NORMAL_Y     4
#define CPLANE_NORMAL_Z     8
#define CPLANE_DIST         12
#define CPLANE_TYPE         16
#define CPLANE_SIGNBITS     17
#define CPLANE_PAD0         18
#define CPLANE_PAD1         19

typedef struct cmodel_s {
    vec3_t mins, maxs;
    vec3_t origin; // for sounds or lights
    int headnode;
} cmodel_t;

typedef struct csurface_s {
    char name[16];
    int flags;
    int value;
} csurface_t;

// a trace is returned when a box is swept through the world
typedef struct {
    qboolean allsolid;      // if true, plane is not valid
    qboolean startsolid;    // if true, the initial point was in a solid area
    float fraction;         // time completed, 1.0 = didn't hit anything
    vec3_t endpos;          // final position
    cplane_t plane;         // surface normal at impact
    csurface_t *surface;    // surface hit
    int contents;           // contents on other side of surface hit
    struct edict_s *ent;    // not set by CM_*() functions
} trace_t;

// pmove_state_t is the information necessary for client side movement
// prediction
typedef enum {
    // can accelerate and turn
    PM_NORMAL,
    PM_SPECTATOR,
    // no acceleration or turning
    PM_DEAD,
    PM_GIB, // different bounding box
    PM_FREEZE
} pmtype_t;

// this structure needs to be communicated bit-accurate
// from the server to the client to guarantee that
// prediction stays in sync, so no floats are used.
// if any part of the game code modifies this struct, it
// will result in a prediction error of some degree.
typedef struct {
    pmtype_t pm_type;
    short origin[3];        // 12.3
    short velocity[3];      // 12.3
    byte pm_flags;          // Ducked, jump_held, etc
    byte pm_time;           // Each unit = 8 ms
    short gravity;
    short delta_angles[3];  // Add to command angles to get view direction
                            // changed by spawns, rotating objects, and
                            // teleporters
} pmove_state_t;

// button bits
#define BUTTON_ATTACK   1
#define BUTTON_USE      2
#define BUTTON_ANY      128

// usercmd_t is sent to the server each client frame
typedef struct usercmd_s {
    byte msec;
    byte buttons;
    short angles[3];
    short forwardmove, sidemove, upmove;
    byte impulse; // remove?
    byte lightlevel; // light level the player is standing on
} usercmd_t;

#define MAXTOUCH 32

typedef struct {
    pmove_state_t s;
    usercmd_t cmd;          // in
    qboolean snapinitial;   // if s has been changed outside pmove
    int numtouch;
    struct edict_s *touchents[MAXTOUCH];
    vec3_t viewangles;      // clamped
    float viewheight;
    vec3_t mins, maxs;      // bounding box size
    struct edict_s *groundentity;
    int watertype;
    int waterlevel;
    trace_t(*trace) (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end);
    int (*pointcontents) (vec3_t point);
} pmove_t;

#define MAX_STATS    32

// config strings are a general means of communication from
// the server to all connected clients.
// Each config string can be at most MAX_QPATH characters.
#define CS_NAME             0
#define CS_CDTRACK          1
#define CS_SKY              2
#define CS_SKYAXIS          3  // %f %f %f format
#define CS_SKYROTATE        4
#define CS_STATUSBAR        5  // display program string
#define CS_MAXCLIENTS       30
#define CS_MAPCHECKSUM      31  // for catching cheater maps
#define CS_MODELS           32
#define CS_SOUNDS           (CS_MODELS+MAX_MODELS)
#define CS_IMAGES           (CS_SOUNDS+MAX_SOUNDS)
#define CS_LIGHTS           (CS_IMAGES+MAX_IMAGES)
#define CS_ITEMS            (CS_LIGHTS+MAX_LIGHTSTYLES)
#define CS_PLAYERSKINS      (CS_ITEMS+MAX_ITEMS)
#define MAX_CONFIGSTRINGS   (CS_PLAYERSKINS+MAX_CLIENTS)

// entity_state_t is the information conveyed from the server
// in an update message about entities that the client will
// need to render in some way
typedef struct entity_state_s {
    int number; // edict index
    vec3_t origin;
    vec3_t angles;
    vec3_t old_origin; // for lerping
    int modelindex;
    int modelindex2, modelindex3, modelindex4; // weapons, CTF flags, etc
    int frame;
    int skinnum;
    int effects;
    int renderfx;
    int solid;      // for client side prediction, 8*(bits 0-4) is x/y radius
                    // 8*(bits 5-9) is z down distance, 8(bits10-15) is z up
                    // gi.linkentity sets this properly
    int sound;      // for looping sounds, to guarantee shutoff
    int event;      // impulse events -- muzzle flashes, footsteps, etc
                    // events only go out for a single frame, they
                    // are automatically cleared each frame
} entity_state_t;

// player_state_t is the information needed in addition to pmove_state_t
// to rendered a view.  There will only be 10 player_state_t sent each second,
// but the number of pmove_state_t changes will be reletive to client
// frame rates
typedef struct {
    pmove_state_t pmove;    // for prediction

    // these fields do not need to be communicated bit-precise
    vec3_t viewangles;      // for fixed views
    vec3_t viewoffset;      // add to pmovestate->origin
    vec3_t kick_angles;     // add to view direction to get render angles
                            // set by weapon kicks, pain effects, etc
    vec3_t gunangles;
    vec3_t gunoffset;
    int gunindex;
    int gunframe;
    float blend[4];         // rgba full screen effect
    float fov;              // horizontal field of view
    int rdflags;            // refdef flags
    short stats[MAX_STATS]; // fast status bar updates
} player_state_t;
