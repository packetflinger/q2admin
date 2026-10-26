/**
 * Copyright (C) The Q2Admin contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

// Network protocol versions, as negotiated at connect time and handed to us
// in the extra userinfo "major" key. See ClientConnect().
#define PROTOCOL_VANILLA    34   // id's original, every client speaks this
#define PROTOCOL_R1Q2       35
#define PROTOCOL_Q2PRO      36

// max_protocol for an engine nobody has researched yet. Such a client is
// still identified by name, but is never treated as downgraded, so an
// unverified entry can't invent a signal. Prefer this over guessing high:
// guessing low only costs a missed detection, guessing high accuses a player.
#define PROTOCOL_UNKNOWN    0

/**
 * Engines a client might be running. Several engines answer to more than one
 * version string, so this is not one-to-one with the rows of clientDefs[].
 */
typedef enum {
    CLIENT_UNKNOWN,
    CLIENT_VANILLA,
    CLIENT_Q2PRO,
    CLIENT_R1Q2,
    CLIENT_Q2RTX,
    CLIENT_AQTION,
    CLIENT_YAMAGI,
    CLIENT_VKQUAKE2,
    CLIENT_KMQUAKE2,
    CLIENT_APRQ2,
    CLIENT_EGL,
    CLIENT_BERSERKER,
    CLIENT_QUAKE2MAXX,
    CLIENT_QUAKE2XP,
    CLIENT_JAKE2,
} client_id_t;

/**
 * One way of recognising one engine.
 *
 * pattern is matched against a lowercased copy of the client's version
 * string, so it must itself be lowercase. It is a tiny-regex pattern, which
 * means no alternation and no grouping - an engine that needs two spellings
 * gets two rows sharing an id.
 */
typedef struct {
    client_id_t id;
    const char *name;         // for display and logging
    int max_protocol;         // best this engine can speak, or PROTOCOL_UNKNOWN
    const char *pattern;
} client_def_t;

const client_def_t *identifyClient(const char *version);
void checkProtocolDowngrade(int client);
