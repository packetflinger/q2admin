/**
 * A fake Quake 2 engine, just big enough to run q2admin code under test.
 *
 * q2admin normally gets everything from the engine and the wrapped mod. All
 * of that arrives as plain globals, so a test can simply supply its own:
 *
 *   gi          the engine's function table - a struct of function
 *               pointers, so "mocking the engine" means assigning fields
 *   proxyinfo   q2admin's per-client state; point it at our own array
 *   maxclients  a cvar_t *, so point it at our own cvar_t
 *   ltime       the server clock, in seconds; set it to whatever a test needs
 *
 * IMPORTANT: this file includes g_local.h so proxyinfo_t has the real
 * layout. Do not be tempted to redeclare a smaller version of the struct in
 * a test - the field offsets then disagree with the compiled code under
 * test, which reads the wrong members and quietly passes or fails for the
 * wrong reason.
 *
 * gi is zeroed apart from the three print calls, which error paths hit
 * incidentally; without them a test trips over a NULL function pointer and
 * segfaults with no clue why. Fill in more fields as tests need them.
 *
 * ge.edicts points at a fake edict array, so getEnt() and getEntOffset()
 * work and code taking an edict_t * can be tested. ge_mod is NULL: nothing
 * here can forward to a real game mod.
 */

#include "g_local.h"
#include "stub_engine.h"

#define STUB_MAXCLIENTS 8

/* --- the globals q2admin expects the engine and mod to have set --- */

static proxyinfo_t stub_clients[STUB_MAXCLIENTS + 1];
proxyinfo_t *proxyinfo;

static cvar_t stub_maxclients;
cvar_t *maxclients = &stub_maxclients;

/* Slot 0 is the worldspawn entity, so clients start at 1 - the same
 * off-by-one getEnt(client + 1) assumes in production. */
static edict_t stub_edicts[STUB_MAXCLIENTS + 1];
static gclient_t stub_gclients[STUB_MAXCLIENTS + 1];

game_import_t gi;
game_export_t ge;
game_export_t *ge_mod;

float ltime;
int lframenum;
int server_protocol = PROTOCOL_Q2PRO;
int q2a_developer;
char moddir[256] = "baseq2";
char buffer2[256];

/* What raiseSignal() was asked to set, per client, for tests to assert on. */
unsigned int stub_signals[STUB_MAXCLIENTS + 1];

/* g_ban.o reads these. The cvars stay zeroed: nothing under test reads a
 * ban file, and a NULL ->string would fault loudly if something tried. */
bool filternonprintabletext = true;
static cvar_t stub_ban_cvars[3];
cvar_t *configfile_ban = &stub_ban_cvars[0];
cvar_t *q2adminbanremotetxt = &stub_ban_cvars[1];
cvar_t *q2adminbanremotetxt_enable = &stub_ban_cvars[2];

/* g_log.o reads these. buffer is q2admin's shared scratch string. */
char buffer[0x10000];
static cvar_t stub_configfile_log;
cvar_t *configfile_log = &stub_configfile_log;
bool consolelog_enable;
char consolelog_pattern[256] = "[q2a] %s\n";
int signal_score_threshold = 50;

/* Settable by a test, so "#x" and "#X" have a known value to format. */
int stub_signal_score;

/* --- stubbed q2admin functions --------------------------------------
 *
 * Stubbing raiseSignal() rather than linking the real g_signal.o keeps the
 * dependency list short, and recording the request is exactly what a test
 * wants to check anyway. g_signal.o needs a fuller gi, logEvent() and
 * addCmdQueue(); link it once something needs the real scoring.
 */

void raiseSignal(int client, unsigned int signal)
{
    if (client >= 0 && client < STUB_MAXCLIENTS) {
        stub_signals[client] |= signal;
    }
}

/* Deferred work: tests check the formatted output, not the queue. */
void addCmdQueue(int client, byte command, float timeout, unsigned long data,
                 char *str)
{
    (void)client; (void)command; (void)timeout; (void)data; (void)str;
}

uint16_t getport(void)
{
    return 27910;
}

/* logEvent() is not stubbed: g_log.o is linked, and the real one returns
 * straight away because every logtypes[] entry starts disabled. */

size_t httpGetFile(generic_file_t *output, const char *url)
{
    (void)output; (void)url;
    return 0;
}

int signalScore(int client)
{
    (void)client;
    return stub_signal_score;
}

/* One fixed definition, so the "#w" format has something deterministic to
 * print. Linking the real g_signal.o instead would clash with this and with
 * raiseSignal() above. */
signal_def_t *findSignalDefFromID(const int id)
{
    static signal_def_t def = { SIGNAL_CHATFLOOD, 15, "chatflood" };

    return (unsigned int)id == def.bit ? &def : NULL;
}

/* --- fake engine print calls --- */

static void stub_dprintf(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    printf("    [gi.dprintf] ");
    vprintf(fmt, ap);
    va_end(ap);
}

static void stub_cprintf(edict_t *ent, int level, char *fmt, ...)
{
    va_list ap;
    (void)ent;
    (void)level;
    va_start(ap, fmt);
    printf("    [gi.cprintf] ");
    vprintf(fmt, ap);
    va_end(ap);
}

static void stub_bprintf(int level, char *fmt, ...)
{
    va_list ap;
    (void)level;
    va_start(ap, fmt);
    printf("    [gi.bprintf] ");
    vprintf(fmt, ap);
    va_end(ap);
}

/* --- test-facing setup ---------------------------------------------- */

void stub_engine_init(void)
{
    memset(stub_clients, 0, sizeof(stub_clients));
    memset(stub_signals, 0, sizeof(stub_signals));
    memset(&gi, 0, sizeof(gi));

    gi.dprintf = stub_dprintf;
    gi.cprintf = stub_cprintf;
    gi.bprintf = stub_bprintf;

    /* proxyinfo is offset by one in the real server: slot -1 is the
     * "not a player" entry. Mirror that so indexes match production. */
    proxyinfo = stub_clients + 1;
    proxyinfo[-1].inuse = 1;

    memset(stub_edicts, 0, sizeof(stub_edicts));
    memset(stub_gclients, 0, sizeof(stub_gclients));
    ge.edicts = stub_edicts;
    ge.edict_size = sizeof(edict_t);
    ge.max_edicts = STUB_MAXCLIENTS + 1;
    ge_mod = NULL;
    stub_signal_score = 0;

    stub_maxclients.value = STUB_MAXCLIENTS;
    ltime = 100.0f;
    lframenum = 1000;
    server_protocol = PROTOCOL_Q2PRO;
}

proxyinfo_t *stub_add_client(int slot, const char *name,
                             const char *version, int protocol)
{
    proxyinfo_t *cl = &proxyinfo[slot];

    memset(cl, 0, sizeof(*cl));
    cl->inuse = 1;
    cl->enteredgame = ltime;
    if (name) {
        snprintf(cl->name, sizeof(cl->name), "%s", name);
    }
    if (version) {
        snprintf(cl->client_version, sizeof(cl->client_version), "%s", version);
    }
    cl->protocol_major = protocol;
    stub_signals[slot] = 0;

    /* Give it an entity too, so code taking an edict_t * can be called. */
    cl->ent = stub_ent(slot);
    cl->ent->inuse = true;
    cl->ent->client = &stub_gclients[slot + 1];
    return cl;
}

edict_t *stub_ent(int slot)
{
    return &stub_edicts[slot + 1];
}

unsigned int stub_signals_for(int slot)
{
    return stub_signals[slot];
}
