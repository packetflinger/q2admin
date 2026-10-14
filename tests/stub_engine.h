/* Test-facing half of the fake engine. See stub_engine.c.
 * Include g_local.h before this. */

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
