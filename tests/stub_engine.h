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

/* Bitmask of the signals raiseSignal() was called with for that client. */
unsigned int stub_signals_for(int slot);

#endif /* STUB_ENGINE_H */
