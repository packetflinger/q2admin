/**
 * g_offsets.h
 *
 * This file houses byte offset definitions for common mod structures
 * for data retrieval.
 */

/**
 * BaseQ2
 *
 * STRUCTURE             SIZE
 *
 */
#define Q2_CL_WEAPON_OFFSET           0


/**
 * OpenTDM
 *
 * STRUCTURE             SIZE
 * edict_t               960
 * gclient_t             5680
 * player_state_t        184
 * client_persistent_t   3864
 * client_respawn_t      176
 * pmove_state_t         28
 * gitem_t               144
 * weaponstate_t         4
 * grenade_state_t       4
 * unsigned              4
 * qboolean              4
 * int                   4
 * vec3_t                12
 * float                  4
 */
#define OTDM_CL_WEAPON_OFFSET            5624
#define OTDM_CL_LASTWEAP_OFFSET          5824

