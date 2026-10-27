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

typedef struct {
    bool        used;
    netadr_t    addr;      // base address this entry is keyed on (port ignored)
    time_t      expires;   // absolute time this entry stops being valid
    vpn_t result;
} vpn_cache_entry_t;

extern int vpn_cache_probe; // max linear-probe distance before an insert evicts
extern int vpn_cache_size;  // slots, only read at server start, 0 disables the cache
extern int vpn_cache_ttl;   // seconds a cached VPN check result stays valid

void displayNextVPNCache(edict_t *ent, int client, unsigned long slot);
void vpnCacheFree(void);
bool vpnCacheGet(netadr_t *addr, vpn_t *out);
void vpnCacheInit(void);
void vpnCachePruneRun(int startarg, edict_t *ent, int client);
void vpnCacheRun(int startarg, edict_t *ent, int client);
void vpnCacheSet(netadr_t *addr, const vpn_t *result);
