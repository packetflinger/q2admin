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

#define IPLOGS_CACHE_SIZE    1024   // slots; must be a power of 2
#define IPLOGS_CACHE_PROBE   8      // max linear-probe distance before an insert evicts

typedef struct {
    bool        used;
    netadr_t    addr;      // base address this entry is keyed on (port ignored)
    time_t      expires;   // absolute time this entry stops being valid
    iplogsvpn_t result;
} iplogs_cache_entry_t;

extern int iplogs_cache_ttl;   // seconds a cached IPLogs result stays valid

bool IPLogsCacheGet(netadr_t *addr, iplogsvpn_t *out);
void IPLogsCacheSet(netadr_t *addr, const iplogsvpn_t *result);
