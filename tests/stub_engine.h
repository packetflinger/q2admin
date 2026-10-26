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

#ifndef STUB_ENGINE_H
#define STUB_ENGINE_H

/* Call once at the top of main(): clears all clients and signals, and sets
 * gi, proxyinfo, maxclients and ltime to usable values. */
void stub_engine_init(void);

/* Put a connected client in a slot. name/version may be NULL. Returns the
 * slot so a test can set further fields on it. */
proxyinfo_t *stub_add_client(int slot, const char *name,
                             const char *version, int protocol);

/* The fake entity for a slot, same one stub_add_client() attaches. */
edict_t *stub_ent(int slot);

/* Bitmask of the signals raiseSignal() was called with for that client. */
unsigned int stub_signals_for(int slot);

/* What the stubbed signalScore() returns, for the "#x"/"#X" log formats. */
extern int stub_signal_score;

#endif /* STUB_ENGINE_H */
