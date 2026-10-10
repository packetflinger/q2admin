# Life of a Quake 2 Client

*A client's whole visit to a q2admin-protected server, from connecting to
disconnecting, as q2admin sees it.*

This document follows one player through a visit to a Quake 2 server running
q2admin, from the moment their client asks to connect until they leave. It
describes what q2admin is doing behind the scenes at each step:

- which engine events it intercepts;
- what it sends to the player's client and what it expects back;
- what it checks;
- what happens when a check fails.

It's written for server admins who want to understand why q2admin behaves the
way it does (why a player was kicked, why a signal was raised, why a
connection took a few seconds to settle), and for developers working on
q2admin itself. Function names are given where they help, so you can find the
code; you don't need to read code to follow the story.

Unless stated otherwise:

- the server runs at the stock 10 frames per second;
- every setting has its default value (see the README's
  [Configuration reference](README.md#configuration-reference));
- the document describes the code as of revision r1215 (`6605847`).

- [Background: how q2admin sees a player](#background-how-q2admin-sees-a-player)
- [The timeline at a glance](#the-timeline-at-a-glance)
- [Phase 0: The handshake q2admin doesn't see](#phase-0-the-handshake-q2admin-doesnt-see)
- [Phase 1: ClientConnect, the gatekeeper](#phase-1-clientconnect-the-gatekeeper)
- [Phase 2: Loading in](#phase-2-loading-in)
- [Phase 3: ClientBegin, entering the world](#phase-3-clientbegin-entering-the-world)
- [Phase 4: The probe gauntlet](#phase-4-the-probe-gauntlet)
- [Phase 5: Playing](#phase-5-playing)
- [Phase 6: Map changes](#phase-6-map-changes)
- [Phase 7: Leaving](#phase-7-leaving)
- [Appendix A: A clean client's conversation, line by line](#appendix-a-a-clean-clients-conversation-line-by-line)
- [Appendix B: What each failed check costs](#appendix-b-what-each-failed-check-costs)
- [Appendix C: Dormant checks](#appendix-c-dormant-checks)


## Background: how q2admin sees a player

### Sitting in the middle

The Quake 2 engine runs gameplay through a *game library* (the mod). q2admin
pretends to be that library: the engine loads q2admin, and q2admin loads the
real mod. Every call the engine makes about a player goes to q2admin first,
and q2admin decides whether to pass it on to the mod, change it, or swallow
it. Every call the mod makes back to the engine (printing a message, running
a server command, moving a player) also passes through q2admin. Neither side
knows q2admin is there.

These are the engine events q2admin hooks for each player:

| Engine event | When it happens | q2admin's handler |
| --- | --- | --- |
| `ClientConnect` | The client has passed the engine's checks and wants a player slot. | `ClientConnect()`, [g_init.c](src/g_init.c) |
| `ClientUserinfoChanged` | The client's userinfo (name, skin, rate, and so on) changes. | `ClientUserinfoChanged()`, [g_init.c](src/g_init.c) |
| `ClientBegin` | The client has finished loading the map and enters the world. This happens again on every map change. | `ClientBegin()`, [g_init.c](src/g_init.c) |
| `ClientCommand` | The client sends a command the engine doesn't handle itself. | `ClientCommand()`/`doClientCommand()`, [g_cmd.c](src/g_cmd.c) |
| `ClientThink` | The client sends a movement packet (a *usercmd*), many times a second. | `ClientThink()`, [g_client.c](src/g_client.c) |
| `RunFrame` | Every server frame (100 ms). | `G_RunFrame()`, [g_main.c](src/g_main.c) |
| `SpawnEntities` | A new map loads. | `SpawnEntities()`, [g_init.c](src/g_init.c) |
| `ClientDisconnect` | The player leaves, for any reason. | `ClientDisconnect()`, [g_init.c](src/g_init.c) |

Two of the mod's calls back to the engine matter here too:

- **Print functions.** The mod's `bprintf` (to everyone) and `cprintf` (to one
  player) land in `bprintf_internal()`/`cprintf_internal()`. That's how
  q2admin sees and filters chat.
- **Server commands.** The mod's `AddCommandString` lands in
  `AddCommandString_internal()`.

### Talking to the client: stuffing

The server can't read a client's settings directly. What it *can* do is send a
`svc_stufftext` message: a line of text the client runs in its own console as
if the player had typed it. q2admin calls this *stuffing*, and its helper is
`stuffPlayer()`.

Almost every q2admin probe relies on one behaviour of a genuine Quake 2
client. **When the client's console sees a command it doesn't recognize, it
forwards that command to the server**, which delivers it to q2admin's
`ClientCommand` handler. Combine that with the console's normal features:

- **Variable expansion:** `$version` is replaced with the value of the
  `version` cvar.
- **Aliases:** `alias foo bar` makes `foo` run `bar`.

With those, q2admin can ask questions and get answers back. For example, to
learn the client's version, q2admin stuffs:

```
kQ3vZ8pLm2xR7cT1wB9y $version
```

`kQ3vZ8pLm2xR7cT1wB9y` is a random 20-character string, so it's not a known
command. The client expands `$version` and forwards the line, and q2admin
receives:

```
kQ3vZ8pLm2xR7cT1wB9y q2pro r2160~5ad3a40 Oct  3 2026 Linux x86_64
```

Because the first word is random and changes every time, a cheat can't
answer the question in advance or recognize it to filter it out. Because the
answer must arrive within about a second, a proxy that holds it up for
inspection gets noticed.

A probe fails when the client:

- answers wrongly;
- doesn't answer at all;
- answers too late; or
- answers in a way a genuine client never would.

### The per-player command queue

Most of what q2admin does to a player is scheduled rather than immediate.
Each player slot has a small queue of pending jobs (`QCMD_*` values, added
with `addCmdQueue()` in [g_queue.c](src/g_queue.c)). Each job has a due time on
q2admin's own clock, `ltime`, which advances 0.1 seconds per server frame.

On every frame, `G_RunFrame()` visits each player slot. For each slot it:

1. runs any player timers;
2. takes **at most one** due job off that player's queue and runs it;
3. if a job ran and `enforce_deadlines` is on, checks whether any outstanding
   probe has missed its deadline (`checkClientDeadlines()`).

Because only one job runs per player per frame, several jobs that fall due
at the same moment run on consecutive frames, 100 ms apart. If a player's
queue ever reaches 45 entries, q2admin assumes the player is flooding the
server and kicks them immediately.

### Player state

q2admin keeps a record for each player slot (`proxyinfo_t` in
[g_local.h](src/g_local.h)). Among other things, it holds:

- the player's name, IP address, userinfo and client version;
- the outstanding random probe strings and their deadlines;
- the player's signals and score;
- their flood and mute counters;
- a set of flags (`clientcommand`) recording which tests are in progress and
  which verdicts have been reached.

Two flags matter throughout:

- **`inuse`** means the player has completed the first step of the startup
  handshake. Most probes wait until it's set.
- **`BANCHECK`** is set when the player is banned or waiting to be redirected
  for the reconnect check. While it's set, q2admin keeps the player *away
  from the mod entirely*. The mod is never told about their connection,
  movement, commands or userinfo, so the player sits frozen in limbo until
  q2admin removes them.

### Signals and score

Many checks don't kick anyone directly. Instead they *raise a signal*, a
named, weighted mark on the player (see `raiseSignal()` in
[g_signal.c](src/g_signal.c)). Every time a signal is raised, q2admin adds up the
player's score. When it reaches `signal_score_threshold` (50), the player is
told "You exceeded the server's signal threshold", the event is logged with
the list of signals, and a disconnect is queued. Confirmed cheats are
weighted at 1,000,000 so a single hit removes the player; weaker hints (a
missed deadline, a VPN, an odd aim pattern) only remove someone when several
add up.


## The timeline at a glance

All times are approximate and use the defaults. "B" is the moment the player
enters the world (ClientBegin).

```
CLIENT                                 ENGINE                     Q2ADMIN
  |-- getchallenge / connect ---------->|                            |
  |                                     |-- ClientConnect --------->| Phase 1: gatekeeping
  |                                     |                            |  userinfo sanity, proxy keys,
  |                                     |                            |  connect flood, lockdown, IP,
  |                                     |                            |  bans, reconnect
  |                                     |                            |  check, forward to mod, start
  |                                     |                            |  VPN lookups, whois
  |<-- serverdata, configstrings -------|                            |
  |   (loading the map)                 |-- ClientUserinfoChanged ->| Phase 2: name/skin/rate/
  |                                     |                            |  timescale/fps checks
  |-- begin ---------------------------->|-- ClientBegin ----------->| Phase 3: queue the probes
B+0.0                                    |                            |  (MOTD shown)
B+2.0  <======== q2startNN =================================(stuff)| startup handshake; inuse=1
B+2.x  ========= q2startNN ================================>(reply) | -> schedule bot tests
B+2.x  <======== random $version, auth, timescale probes ===(stuff)| (one per frame)
B+3    <======== zbot character test x23 rounds ============(stuff)| ~0.2 s per round
B+3    <======== .please.disconnect.all.bots x3 ============(stuff)|
B+3..4 <======== alias test, two steps =====================(stuff)|
B+7    <======== exec cfg/all.cfg, cfg/<map>.cfg ===========(stuff)| per-map client configs
B+12   <======== private commands (if configured) ==========(stuff)|
B+14   ......... "ratbot Detect Test" printed ..............(print)|
B+16   <======== name pwsnskle;wait;wait;name <you> ========(stuff)| ratbot name test
B+15, 30, 45...  timescale re-probe every 15 s
B+60, 120...     one client variable checked every 60 s
                 every movement packet: msec, impulse and aim checks
                 every command: probe replies, disables, chat bans, floods
  |-- disconnect / kicked / timeout --->|-- ClientDisconnect ------>| Phase 7: tear down
```


## Phase 0: The handshake q2admin doesn't see

Before q2admin hears about a player, the engine has already handled the
connection itself:

1. The client sends `getchallenge` and gets a challenge number back.
2. The client sends `connect`, carrying its protocol version, `qport`, the
   challenge and its *userinfo* string (for example
   `\name\claire\skin\female/athena\rate\25000\msg\1\hand\2`).
3. The engine validates the challenge and checks its own ban lists, password
   and slot count. It then adds the `ip` key to the userinfo.
4. On R1Q2 and Q2Pro, if the game asked for "extra userinfo"
   (`GMF_EXTRA_USERINFO`), the engine adds a second block after the userinfo's
   terminating NUL: `challenge`, `ip`, `major`, `minor`, `netchan`,
   `packetlen`, `qport` and `zlib`.

A player rejected at this stage never reaches q2admin, so none of q2admin's
logging or checks apply to them.


## Phase 1: ClientConnect, the gatekeeper

The engine calls `ClientConnect(ent, userinfo)` and waits for a yes or no.
Returning *no* rejects the connection, and whatever q2admin wrote into the
userinfo's `rejmsg` key is shown to the player. Everything in this phase
happens synchronously, inside that one call.

### 1. Assembling the userinfo

If extra userinfo is in use, q2admin locates the second block.

- If the block is missing, the connection is rejected with
  `Error: wonky userinfo.`
- Otherwise the two blocks are joined into one combined string for q2admin's
  own checks.

q2admin also decides whether this is **Q2Pro's MVD recording client** (a fake
spectator the server creates to record demos). It decides by looking for the
`mvdspec` key in the part of the userinfo that only the engine can write.
That client gets a few exemptions, described below.

### 2. Housekeeping

Two things happen next:

- **Reconnect list cleanup.** Expired entries are removed from the list of
  players waiting to reconnect (see step 8).
- **A clean slate for the slot.** The player's record is wiped and the slot
  is stamped with the current time.

With extra userinfo, the challenge, protocol version, packet size, qport and
zlib support are copied into the record.

### 3. Address and proxy fingerprints (`UpdateInternalClientInfo()`)

q2admin reads the `ip` key into the player's address. The value `loopback`
becomes `127.0.0.1`.

It then looks for userinfo keys added by known client-side proxies:

- `Nitro2`
- `bwproxy`

If one is found and the matching `proxy_nitro2`/`proxy_bwproxy` setting
allows it, the player is only flagged as a Nitro2-style proxy, which changes
how later tests treat them. If it isn't allowed, the player is treated as a
proxy user:

- with `banonconnect`, the connection is rejected with
  `rejected: proxy/bot signature found`;
- otherwise they're marked banned and removed after entering (see step 7).

### 4. Userinfo validation

- **Format.** The userinfo must be well formed (`Info_Validate()`), or the
  connection is rejected with `rejected: invalid client detected`.
- **Required keys.** `name`, `rate` and `skin` must all be present and
  non-empty, or the connection is rejected with
  `rejected: userinfo missing required value`. Every genuine client sends all
  three. The MVD recording client is exempt.
- **MVD imposters.** A real player whose userinfo contains `mvdspec` is
  pretending to be the recording client. That raises `mvd-imposter`, which
  removes them.

### 5. Recording identity

The player's name and raw userinfo are stored.

If the `skin` value is longer than the allowed maximum (a classic way to
crash servers and clients), q2admin:

- logs it;
- raises `skin-overflow`;
- replaces the skin with `female/jezebel`.

### 6. The connect flood check (`checkConnectFlood()`)

This check is aimed at players who connect, disconnect and reconnect over and
over. The classic case is a muted player who rejoins under a new name each
time to talk through their name. It runs before the lockdown and ban checks,
so a banned player retrying in a loop is counted too. The MVD recording
client is never counted.

q2admin keeps a count of connections per IP address. The count isn't stored
with the player slot, so it survives disconnects. With the default
`connectfloodprotect "3 60 300"`:

1. **If the address is already being held off,** the connection is refused
   with `connectfloodprotectmsg` and nothing else happens.
2. **If this is the reconnect q2admin asked for** (the second half of the
   reconnect check in step 8, recognized by its userinfo matching the saved
   entry), it isn't counted.
3. **Otherwise the connection is counted.** If the address's last count
   started more than 60 seconds ago, counting starts again from this
   connection.
4. **The third connection within the 60 seconds trips the limit.** It's
   refused, the event is printed to the console and logged under the `BAN`
   log type, and `connectfloodcmd` runs on the server with `%i` replaced by
   the address (for example `addblackhole 192.0.2.7`). From then on, the
   engine can drop the address before it reaches q2admin. If it doesn't,
   because no command is configured, q2admin refuses every attempt itself
   (case 1).

The hold lasts for the cooldown (300 seconds). It's lifted by the frame loop
(Phase 5), not by anything the player does.

### 7. The admission decision

Four checks run in order. The first one that applies decides the outcome.

1. **Lockdown.** If the server is locked (`lock`), the player is let in only
   if their name is on the list of players who left during the lockdown. Each
   entry on that list can be used once.
2. **No usable IP.** With `checkclientipaddress` on, a player whose address
   couldn't be parsed is refused, and an `INVALIDIP` event is logged.
3. **Ban list.** `checkIfBanned()` checks the allow (`+`) rules first, then
   the deny (`-`) rules. In each set the newest rule is checked first, and
   the first match wins.
   - A `PASSWORD` rule compares against the player's `pw` userinfo key.
   - A `MAX` rule counts how many players it has let in.
   - A `FLOOD` rule gives the player custom chat-flood limits.
   - A `SCORE` rule raises `ban-entry` with that score.

   **At this point the client's version isn't known yet and the VPN lookup
   hasn't started, so `VERSION`, `IP VPN` and `ASN` rules can't match.**
   Those rules are checked again later, once the client reports its version
   (Phase 4).
4. **Otherwise** the player is let in.

When one of the first three refuses the player, what happens next depends on
`banonconnect`:

- **On:** `ClientConnect` returns *no* with a `rejmsg` (for example
  `banned: You are banned from this server!`). The player never uses a slot.
- **Off:** the player is marked `BANNED`, which is part of `BANCHECK`. They're
  allowed to finish connecting so they can be *shown* the ban message, but the
  mod is never told about them. They're removed shortly after entering
  (Phase 3).

### 8. The reconnect check (only when `reconnect_address` is set)

This check is a defence against proxies. Every new player is made to
disconnect and reconnect directly to the server's real address; a proxy sitting
in the middle is left behind.

q2admin keeps a short list of players it has told to reconnect, saving the
address (without its port) and the userinfo each one had. When a player
connects, q2admin compares them with that list. The address always has to
match. Then:

- **With `reconnect_checklevel 0`:** the whole userinfo must be identical,
  apart from the keys the engine adds itself (`ip`, and with extra userinfo
  `challenge`, `qport` and the other connection details). Several of those
  change on every connection, so they can't be compared.
- **Otherwise:** only the name and skin must match.

The outcomes:

- **A match** means this is the reconnect q2admin asked for. The entry is
  removed and the player proceeds normally.
- **No match** marks the player `RECONNECT`, which is part of `BANCHECK`, so
  the mod isn't told about them. Phase 3 will send them away to reconnect.
  Any older entry from the same IP address is removed so that a proxy can't
  leave stale entries behind.

### 9. Handing over to the mod

If the player passed, q2admin calls the mod's own `ClientConnect`.

- **Which userinfo the mod gets:** a mod that asked for extra userinfo itself
  receives the engine's original two-block userinfo. A mod that didn't, while
  q2admin's `userinfo_proxy` is on, receives the combined string, so it can
  read keys like `qport` normally.
- **If the mod refuses the player,** its `rejmsg` is copied back to the
  engine so the player sees the reason.

### 10. Background lookups

These run whether or not the mod accepted the player:

- **Whois:** if `whois_active`, the player's address is matched to a whois
  record and their *last seen* time is updated.
- **VPN lookup (vpnapi.io):** if `vpn_enable`, an HTTPS request is started in
  the background.
- **VPN lookup (IPLogs):** if `iplogs_enable`, a lookup is started, unless the
  address is in `iplogs_ignorelist` or a cached result less than
  `iplogs_cache_ttl` old exists.

Both lookups finish on later frames ([Phase 5](#asynchronous-results-vpn-lookups)).

### 11. Logging

If the connection is accepted, a `CLIENTCONNECT` event is logged. If the
userinfo had overflowed, a warning is recorded and the player is told about
it later.


## Phase 2: Loading in

The engine now sends the client the server data and map information, and the
client loads the map. q2admin is mostly idle during this time. However, the
engine calls `ClientUserinfoChanged` whenever the client's userinfo changes:
when it's first set up, when the player changes a setting, and whenever
q2admin itself forces a change. All the checks in this phase run on **every**
userinfo change for the rest of the visit, not just while loading.

`ClientUserinfoChanged()` does the following, in order:

1. **Logging.** The new userinfo is logged (`CLIENTUSERINFO`).
2. **MVD imposters.** A real player whose userinfo contains `mvdspec` raises
   `mvd-imposter`.
3. **Known cheat signature.** A `\skon\` key in the userinfo, the mark of an
   old cheat (zgh_frk), gets the player kicked with an announcement that they
   were caught cheating.
4. **Userinfo flooding.** Every change counts against the limit unless the
   only difference is `cl_maxfps` and `fpsfloodexempt` is on. More than
   `userinfochange_count` (40) changes within `userinfochange_time` (60 s)
   gets the player kicked with "tried to flood the server (2)".
5. **Name change** (`checkForNameChange()`):
   - The first name seen is simply recorded.
   - A change to the special name `pwsnskle` is the reply to the ratbot test
     (Phase 4). It marks the test as passed and is *not* passed on, so nobody
     else ever sees that name.
   - If the player is silenced for name-change flooding, the change is refused
     and their old name is stuffed back to them, along with a message saying
     how many seconds of silence are left.
   - Otherwise the whois record gets the new name and **the ban list is
     checked again with the new name**. If the new name is banned, the change
     is reverted, or the player is kicked if `kickonnamechange` is on.
   - Accepted changes are logged (`NAMECHANGE`), announced if
     `displaynamechange` is on, and counted against `namechangefloodprotect`.
     Going over the limit kicks the player (silence 0) or silences their name
     changes for a while.
6. **Skin change** (`checkForSkinChange()`): the same flood logic, using
   `skinchangefloodprotect` and the `SKINCHANGE` log type. Oversized skins
   are replaced and raise `skin-overflow`.
7. **Forwarding to the mod.** If neither check vetoed the change and the
   player isn't in `BANCHECK` limbo, the userinfo is passed to the mod.
8. **Rate.** If `rate` is outside `minrate`/`maxrate`, q2admin stuffs a
   corrected `rate N` to the client.
9. **Timescale.** Timescale speeds up the client's own simulation, which makes
   it a speed cheat.
   - **If the userinfo has no `timescale` key,** q2admin stuffs
     `set timescale $timescale u`. The `u` flag adds the cvar to the userinfo,
     so q2admin will see its real value on the next change.
   - **If it's present and reads as `0`,** the player is kicked as a cheater.
     The value is read as a whole number, so `0.5` also counts as 0. Genuine
     clients never report a value below 1.
   - **If it's present, not 1, and `timescaledetect` is on,** q2admin stuffs
     `set timescale 1`.
10. **Frame rate (`cl_maxfps`).** It works the same way.
    - **If the key is missing** and `maxfps` or `minfps` is set, q2admin
      stuffs `set cl_maxfps $cl_maxfps u`. It then stuffs
      `set rate <rate+1>` followed by `set rate <rate>`, which forces the
      client to send a fresh userinfo immediately.
    - **If it's present and equals `0`,** the player is kicked as using a
      modified client.
    - **If it's above `maxfps` or below `minfps`,** the corrected value is
      stuffed.
11. **`cl_pitchspeed` and `cl_anglespeedkey`** (when enabled). These control
    turning speed with the keyboard. If missing, they're added to the userinfo
    the same way. Once a value is known, any later change logs a `ZBOT`
    event, is announced if configured, and kicks the player if configured.
    The exceptions are a change to 150 or 1.5, which Action Quake 2 makes by
    itself.
12. **Storing.** The userinfo is stored for the next comparison and reported
    to Cloud Admin.


## Phase 3: ClientBegin, entering the world

The client has finished loading and the engine calls `ClientBegin`. **This
happens again after every map change**, so everything below repeats on each
map.

### 1. Mod or limbo

If the player isn't in `BANCHECK` limbo, the mod's `ClientBegin` runs and they
appear in the world. If they are in limbo, the mod never hears about them, and
q2admin only narrows their field of view so the engine has a valid view to
send.

### 2. Resetting the per-map state

These are reset on every `ClientBegin`:

- userinfo flood counters;
- impulse and vote counters;
- probe retry counters and the zbot test position;
- all probe deadlines;
- chat statistics;
- the msec window.

`inuse` is cleared too, so the startup handshake runs again on every map.
Admin logins and frame-rate display state are only reset if the slot wasn't
already in use.

### 3. The address limit

With `ip_limit` greater than 0, players are counted by address. If this
player brings the number from their address over the limit, they're told
"Too many connections from the same IP address" and a disconnect is queued.

### 4. The fork

What happens next depends on the player's flags:

- **`RECONNECT` set:** a reconnect job is queued for 1 second later (see
  [the reconnect check](#the-reconnect-check)).
- **`BANNED` set:** the ban reason is printed in a banner and a disconnect is
  queued for 1 second later.
- **Kicked:** a disconnect is queued.
- **Q2Pro's MVD recording client:** the slot is marked in use and no probes
  are queued. The server creates this client itself and, by default, ignores
  stuffed commands, so it could never answer a probe. Signals are never
  raised against it, and it's left out of the address limit and the VPN
  lookups.
- **Otherwise,** the probe battery is queued (all due immediately unless a
  delay is listed):

  | Job | Purpose |
  | --- | --- |
  | `QCMD_STARTUP` | Begin the startup handshake. |
  | `QCMD_CLIENTVERSION` | Ask for the client's version. |
  | `QCMD_AUTHADMINPASS` | If `adminpassword` is set: try the player's saved admin password. |
  | `QCMD_AUTHADMIN` | If `q2a_login.cfg` has accounts: try the player's saved admin login. |
  | `QCMD_AUTHBYPASS` | If `q2a_bypass.cfg` has accounts: try the player's saved bypass login. |
  | `QCMD_CONNECTCMD` | If `customclientcmdconnect` or `customservercmdconnect` is set: run them. |
  | `QCMD_TESTTIMESCALE` | If `timescaledetect` is on: probe timescale. |
  | `QCMD_CHECKVARTESTS` | Due in `checkvar_poll_time` (60 s): start polling client variables. |

  If the userinfo had overflowed, the player is warned to restart Quake 2.
  If a message of the day is set, it's shown in the center of their screen.

`QCMD_SPAMBYPASS` is also queued (60–70 s) whenever bypass accounts exist, but
it currently does nothing.

### 5. Logging

A `CLIENTBEGIN` event is logged.


## Phase 4: The probe gauntlet

This is where q2admin tests whether the client is a genuine Quake 2 client
talking directly to the server. **Until the startup handshake sets `inuse`,
every other queued job is put back at the end of the queue**, so the version,
auth and timescale probes all wait for it.

### The startup handshake (B+0 to B+2 s)

1. **`QCMD_STARTUP`** sets the *startup test* flag and queues
   `QCMD_STARTUPTEST` for 2 seconds later.
2. **`QCMD_STARTUPTEST`, first run (B+2 s):**
   - Queues `QCMD_EXECMAPCFG` for 5 s later and `QCMD_SHOWMOTD`.
   - With `do_franck_check` on, stuffs
     `riconnect; roconnect; connect; set frkq2 disconnect; set quake2frk disconnect; set q2frk disconnect`.
     This neutralizes commands added by an old cheat. q2admin also refuses to
     forward any client command that mentions `riconnect` or `roconnect`.
   - With `do_vid_restart` on, stuffs `vid_restart` (once per connection),
     which unloads some wallhacks.
   - **Sets `inuse`.**
   - Stuffs `q2startNN`. The two digits are randomized on every map.
   - Queues itself again for 5 s later.
3. **The reply.** A genuine client doesn't recognize `q2startNN`, so it
   forwards it, and `doClientCommand()` receives it. If the startup test flag
   is set, q2admin clears it, removes the pending `QCMD_STARTUPTEST`, resets
   the retry counters and, if `zbotdetect` is on, schedules the bot tests:

   | Job | Delay | Skipped if |
   | --- | --- | --- |
   | `QCMD_RESTART` (starts the zbot character test) | 1 s | |
   | `QCMD_LETRATBOTQUIT` | 1 s | |
   | `QCMD_TESTALIASCMD1` | 1 s | the alias test has already caught this client on this map |
   | `QCMD_TESTSTANDARDPROXY` | 10 s | the player was flagged as a Nitro2 proxy |
   | `QCMD_TESTRATBOT` | 12 s | the ratbot test has already passed |

4. **No reply.** If no reply arrives, `QCMD_STARTUPTEST` fires again every
   5 seconds and stuffs `q2startNN` again. A genuine client answers the first
   one: it's a reliable message, which the engine retransmits until it
   arrives. Once it has been sent `startup_attempts` times (default 3, at
   about B+2, B+7 and B+12 s) without an answer, a startup failure is logged
   at the next check (about B+17 s) and the player is put through the zbot
   verdict described below. This matters because a client that never
   answers also never gets the zbot, ratbot or alias tests.

### The client version probe (immediately after `inuse`)

`QCMD_CLIENTVERSION` stuffs `<random20> $version` and sets a 1-second
deadline. When the answer arrives, `doClientCommand()`:

1. stores the version string and logs `CLIENTVERSION`;
2. runs the **protocol downgrade check** (`checkProtocolDowngrade()`). If the
   version names an engine q2admin knows, and the protocol the client
   connected with is older than that engine supports, it raises
   `protocol-downgrade`. Cheats sometimes force an older protocol;
3. **checks the ban list a second time.** Now the version is known, and the
   VPN lookup has usually returned, so `VERSION`, `IP VPN` and `ASN` rules can
   finally match. A match prints the ban message in a banner and queues a
   disconnect for 1 second later;
4. tells Cloud Admin about the player.

If no answer arrives within the second, `version-probe` (weight 20) is raised.

### Automatic logins (immediately after `inuse`)

q2admin stuffs commands that use the player's own cvars:

| Login | Stuffed command |
| --- | --- |
| Shared admin password | `!setadmin $q2adminpassword` |
| Admin account | `!admin $q2adminuser $q2adminpass` |
| Bypass account | `!bypass $clientuser $clientpass` |

A player who keeps their credentials in `autoexec.cfg` is logged in
automatically. If the cvars are empty, the command arrives with missing or
wrong values and nothing happens. Successful logins are logged (`ADMINLOG`),
and admin-account holders are shown the commands their level allows.

### Connect commands (immediately after `inuse`)

- `customclientcmdconnect` is stuffed to the client.
- `customservercmdconnect` runs on the server, with `%c` replaced by the
  player's slot number.

### The timescale probe (immediately, then every 15 s)

`QCMD_TESTTIMESCALE`:

1. stuffs `<random20> $timescale`;
2. sets a 1-second deadline;
3. queues itself again for 15 seconds later, for the whole visit.

The reply must be exactly `1`. Anything else (`2`, `0.5`, `1.5`) is a
timescale cheat. q2admin then logs a `ZBOT` event, announces it
`numofdisplays` times, runs `customclientcmd`, and raises
`timescale-modified` (1,000,000, an immediate removal). A missed deadline
raises `timescale-probe` (20).

### The zbot character test (from B+3 s)

This is q2admin's oldest test, aimed at the zbot family of aim proxies. Those
proxies filter or rewrite the commands a client sends, and q2admin exploits
that.

The test works through 23 punctuation characters, `!@#%^&*()_=|?.>,<[{]}':`,
one per round. Each round:

1. **`QCMD_ZPROXYCHECK1`** builds a test string. It's the current character,
   then `FU`, then two digits randomized per map, then two random characters,
   for example `!FU37xq`. q2admin stuffs:
   ```
   !FU37xq
   q2eNN
   ```
   It then sets the *checking* flag and queues `QCMD_ZPROXYCHECK2` as a
   timeout, `clientsidetimeout` (30 s) later.
2. **A genuine client** forwards both lines in order:
   - When the test string arrives, q2admin counts the round as passed, moves
     to the next character, clears the flag and cancels the timeout. For
     players flagged as Nitro2 proxies, `.` and `,` are skipped, because those
     proxies legitimately use them.
   - When `q2eNN` arrives with the flag already clear, q2admin starts the next
     round (or, after the last character, marks the test passed for good).
3. **A proxy that swallows or changes the test string** sends only `q2eNN`.
   q2admin sees it while the *checking* flag is still set. It clears the
   player's screen and retries the round 2–5 seconds later. On the fourth
   failure the verdict is reached:
   - the `ZBOT` event is logged and `customservercmd` runs;
   - the final judgement is scheduled after `zbotdetectactivetimeout`
     (default 0). A value of -1 picks a random 5–60 seconds, so a cheater
     can't easily work out which action triggered it.
4. **No reply at all** before the 30-second timeout counts the same as a
   failure: up to three retries, then the verdict.

When the judgement fires:

- the detection is announced `numofdisplays` times with `zbotuserdisplay`;
- `customclientcmd` is stuffed;
- if `disconnectuser` is on, `zbot-detected` (1,000,000) is raised, which
  removes the player.

A clean client goes through all 23 rounds in roughly 3–5 seconds.

### Letting bots quit (B+3 s)

`QCMD_LETRATBOTQUIT` stuffs `.please.disconnect.all.bots` three times.
Bot software that honors this request leaves on its own. A genuine client
forwards the line as an unknown command and q2admin quietly discards it.

### The alias test (B+3 to B+4 s)

This test checks that the client really runs a Quake 2 console.

1. **`QCMD_TESTALIASCMD1`** picks two random strings, *A* and *B*, and stuffs
   `alias A B`. A genuine client defines the alias silently. A client or proxy
   that doesn't support `alias` forwards the whole line to the server; q2admin
   sees a command called `alias` and raises `alias-unsupported` (1,000,000).
2. **One second later, `QCMD_TESTALIASCMD2`** stuffs `A` and sets a 1-second
   deadline:
   - a genuine client expands the alias and forwards `B`, which passes;
   - forwarding `A` itself means the alias wasn't expanded, which raises
     `alias-unsupported`;
   - no reply in time raises `alias-probe` (20).

While the alias test is waiting, the *first* command q2admin sees from the
player also triggers two side checks:

- **The `rate` key.** If the stored userinfo has no `rate` key, the client
  doesn't behave like a standard one, so `wonky-userinfo` (15) is raised. This
  check runs once per map and is skipped while the stored userinfo is empty.
- **A failed reconnect.** If the player failed the reconnect check on an
  earlier visit to this slot from the same address, `bad-client` (1,000,000)
  is raised.

### Private commands (B+12 s, if configured)

Admins can set up to four `private_command` and four `inverted_command`
strings that only they know. `QCMD_TESTSTANDARDPROXY` stuffs all of them,
opens a 10-second window, and queues a review for 10 seconds later. During
the window, any command from the player that exactly matches one of the
private strings is recorded as received and not passed on.

When the window closes:

- a `private_command` that **never** came back is a failure;
- an `inverted_command` that **did** come back is a failure.

Each failure logs a `PRIVATELOG` event. With `private_command_kick` on, the
player is also announced as using a modified client and disconnected.

### Per-map client configs (B+7 s)

`QCMD_EXECMAPCFG` stuffs, depending on `client_map_cfg` (default 6):

- `set map_name <map>` (bit value 1);
- `exec cfg/all.cfg` (bit value 4);
- `exec cfg/<map>.cfg` (bit value 2).

`QCMD_SHOWMOTD` shows the message of the day again.

### The ratbot test (B+14 s to about B+46 s)

Ratbot was another aim proxy, and it gets two tests:

1. **The print test (B+14 s).** `QCMD_TESTRATBOT` *prints* (rather than
   stuffs) `ratbot Detect Test ( rbkck &%trf .disconnect )` to the player.
   Ratbots read this text and answer in chat with
   `Please help me : What is a Bot ??` or
   `Yeah !!! I am a R A T B O T !!!!! ??`. Either reply raises
   `ratbot-detected` (1,000,000) straight away. A genuine client only shows
   the text.
2. **The name test (B+16 s).** Two seconds after the print test,
   `QCMD_TESTRATBOT3` stuffs
   `name pwsnskle;wait;wait;name "<current name>"`. A genuine client sends a
   userinfo update for the temporary name, which passes the test (Phase 2,
   step 5), and then one for the real name. A ratbot blocks name changes, so
   the temporary name never arrives. After `clientsidetimeout` (30 s) without
   it, the name test is retried 2–5 seconds later. After three retries, q2admin
   logs the detection, announces it, and raises `ratbot-detected` if
   `disconnectuser` is on.

### Client variable polling (B+60 s, then every 60 s)

With `checkvarcmds_enable` on and rules in `q2a_cvar.cfg`, each run of
`QCMD_CHECKVARTESTS` does the following:

1. Stuffs `<random20> $<cvar>` for the **next** rule in the list, cycling
   through the list.
2. Sets a 1-second deadline.
3. Queues itself again for `checkvar_poll_time` later.

When the answer arrives:

- a `CT` rule whose value doesn't match exactly has the correct value stuffed;
- an `RG` rule whose value is below the minimum or above the maximum has the
  nearest limit stuffed.

A missed deadline raises `checkvar-probe` (20). With five rules and the
default 60-second poll time, each variable is checked every five minutes.

### The reconnect check

This only applies to players marked `RECONNECT` in Phase 1, who are sitting in
limbo. One second after they enter, the reconnect job:

1. saves their userinfo to the reconnect list, valid for `reconnect_time`
   seconds;
2. keeps a per-address retry count. On the sixth attempt from the same
   address, q2admin stuffs `disconnect` and gives up;
3. prints `defaultreconnectmessage`;
4. stuffs commands that store the server address and the word `connect` in
   two random cvars, *C1* and *C2*;
5. stuffs `alias connect <random T>`, then `alias <random S> $C2 $C1`, then
   `S`.

When the `alias S` line is defined, the variables expand, so `S` becomes
`connect <server address>`. Quake 2 checks built-in commands *before*
aliases, so a genuine client runs the real `connect` and reconnects directly.
Phase 1 then finds the matching entry and lets the player in properly.

A client that checks aliases first runs the decoy alias instead, and sends
*T* back to the server. q2admin records the failure and the player's address.
If the same address reappears in this slot, `bad-client` is raised (see
[the alias test](#the-alias-test-b3-to-b4-s)).


## Phase 5: Playing

Once the gauntlet is passed, q2admin keeps watching on several fronts at
once.

### Every movement packet (`ClientThink()`)

A client sends a *usercmd* for every frame it renders: view angles, buttons,
movement, an `msec` value (how much game time the packet covers) and an
optional *impulse*. q2admin handles each one in this order:

1. **Freeze.** A player frozen by an admin has `msec` set to 0, which stops
   them moving. A timed freeze ends by itself.
2. **The msec window.** Every `msec_timespan` (5 s), q2admin compares the
   total msec used with the limits:
   - more than `msec_max_allowed` (5600, which is 5 seconds plus 12% slack)
     counts as a violation; after `msec_max_violations` (2) violations,
     `msec-overrun` (20) is raised;
   - with `msec_max_violations` set to 0, the player is instead frozen for 3
     seconds (as long as they've been in the game for at least 5 seconds);
   - less than `msec_min_required` (off by default) works the same way and
     raises `msec-underrun`.

   The previous window's total is kept for `!dumpmsec`, and the frame counter
   for `showfps` is reset.
3. **Chat bot statistics.** Distance moved and shots fired are accumulated
   and decay with a 2-minute half-life. Together with the words the player
   chats, these give *words per mile* and *words per shot*, which
   `chat_stats` uses to tell a chat bot from a talkative player.
4. **Speed freeze.** While a speed freeze is active, `msec` is forced to 0.
5. **Impulses.**
   - Every impulse is logged.
   - Impulses 169–175 (the zbot's menu controls) are logged as zbot impulses
     with a description.
   - With `displayimpulses` on, the impulse is announced to everyone.
   - Impulses listed in `impulsestokickon` raise `impulse-sent`, which is
     worth 10 per impulse.
6. **`swap_attack_use`** swaps the attack and use buttons when enabled.
7. **Aim analysis.** This only runs if the player isn't in limbo and hasn't
   already been caught:
   - **Jitter** (`checkForAimbot()`, `zbc_*`): a large view jump that
     immediately snaps back is the mark of an aimbot. More than
     `zbc_jittermax` (4) such hits in a row is a confirmed bot, and the player
     goes straight to the zbot verdict. Fewer raise `aimbot-jitter` (15 per
     hit), which clears after `zbc_jittertime` (10 s) without another hit.
   - **Snap-fire** (`checkForSnapFire()`): the view jumps at least
     `snapfire_min_snap_deg` (30°) in one frame onto an opponent who was more
     than `snapfire_off_crosshair_deg` (40°) off the crosshair, while the
     player is firing. This raises `snap-fire` (15 per hit).
   - **Tracking** (`checkForTracking()`): the crosshair stays within
     `track_tight_deg` (4°) of a visible enemy while that enemy moves at least
     `track_min_motion_deg` (15°) across the screen. Humans can't hold a
     target that steadily, so this raises `aim-track` (30).
8. **Forwarding to the mod.** If the player isn't in limbo, the usercmd is
   passed to the mod, which moves the player.

### Every command (`ClientCommand()`)

Anything the client sends that the engine doesn't handle itself (`say`,
`kill`, `vote`, mod commands, and the replies to q2admin's probes) goes
through `doClientCommand()`. It runs these steps in order, and stops at the
first step that handles the command:

1. **Rcon password leaks.** A command containing the rcon password is logged
   as an `EXPLOIT` and dropped. Some cheats try to grab it.
2. **Probe replies.** The startup handshake, version, zbot test, timescale,
   client variable, ratbot, alias and reconnect replies described in Phase 4
   are recognized and handled. They're never passed to the mod.
3. **Leftover test strings.** Old-style or stale test strings from an earlier
   map are ignored. Malformed ones are logged as internal warnings.
4. **Recording the command** as the player's *last command*, which is used to
   match chat lines back to the player who sent them.
5. **Disabled commands.** A match in `q2a_disable.cfg` logs a `DISABLECMD`
   event and drops the command.
6. **Referee commands.** Commands such as `admin`, `ref` and `referee` are
   logged under `ADMINLOG`.
7. **Private command replies** inside their 10-second window are recorded
   (Phase 4).
8. **Chat** (`say`, `say_team`):
   - lines with more than five `%` characters are dropped;
   - chat from Nitro2 or XANIA proxies flags or catches the player, depending
     on `proxy_nitro2`;
   - **muted** players are blocked (see
     [Every chat line](#every-chat-line-print-hooks));
   - with `extendedsay_enable`, `say !p` and `say !g` become private messages.
9. **Flood-listed commands** (`q2a_flood.cfg`) are blocked if the player is
   muted. Otherwise they're marked for a flood check after they run.
10. **Cloud Admin commands** (`!teleport`, `!invite`).
11. **`!` commands:**
    - level-based commands for logged-in admin accounts (`!boot`,
      `!changemap`, and so on);
    - the `!admin`, `!bypass`, `!setadmin` and `!version` logins;
    - for players logged in with the shared admin password, any q2admin
      console command.
12. **q2admin's own player commands:** `say_person`, `say_group`, `lrcon`,
    the vote command, `showfps`, `whois`, `timer_start`, `timer_stop` and
    `motd`.
13. **Chat bans.** If the command text matches a chat-ban rule, the player
    sees the ban message, a `CHATBAN` event is logged, and the command is
    dropped.
14. **Forwarding.** Anything else is logged (`CLIENTCMDS`) and passed to the
    mod, unless the player is in limbo. Flood-listed commands are then
    counted against the chat flood limit.

### Every chat line (print hooks)

When the mod prints a chat line, q2admin's print hooks (`bprintf_internal()`,
`cprintf_internal()`) work out which player said it. They know who sent the
last command, or they can match the line against each player's name and last
command. Then:

1. **Mute check.** If the speaker is muted (by an admin, by a flood penalty,
   or stifled), the line isn't printed, and the speaker is told how many
   seconds are left. A stifled player can say one line, and must then wait
   the stifle length before the next one.
2. **Console chat.** With `consolechat_disable`, chat typed into the console
   without a command (the "talk to server" style) is dropped.
3. **Filtering.** With `filternonprintabletext`, non-printable characters are
   replaced with spaces.
4. **Logging and statistics.** The line is logged (`CHAT`), and the player's
   chat statistics are updated: characters, characters per second, decaying
   word count, and the last four messages for `chat_stats`.
5. **Printing.** The line is printed and copied to Cloud Admin.
6. **Flood check** (`checkForFlood()`), if chat flood protection is on for
   this player or for the whole server. More than the allowed count within the
   window announces "*name* is making too much noise" and raises `chatflood`
   (15). Then, depending on the configured silence:
   - **0:** disconnect;
   - **negative:** mute for the rest of the visit;
   - **positive:** mute for that many seconds.

   The signal clears when a window passes quietly.

### Every userinfo change

All of Phase 2 runs again: name and skin checks with a ban re-check, the rate,
timescale and frame-rate limits, and turning-speed monitoring.

### Asynchronous results: VPN lookups

The HTTPS requests started in Phase 1 complete a few frames later, and their
results are processed then:

- **vpnapi.io** marks the player as VPN-positive if the address is a VPN,
  proxy, Tor exit or relay, and records the network and ASN. Then:
  - the player is shown in the server console with their network and ASN;
  - a positive result raises `vpn-detected` (40). That's below the default
    threshold of 50, so a VPN alone doesn't remove anyone; it adds to the
    player's score like any other suspicion;
  - with `ip_limit_vpn` set, VPN players from the same ASN are counted and the
    extra ones are disconnected with "Too many connections from the same VPN
    provider".
- **IPLogs** returns a confidence score and a verdict, which are cached. The
  verdict decides the signal: `suspicious` raises `vpn-suspicious` (20),
  `vpn_likely` raises `vpn-likely` (30), and `vpn_detected` raises
  `vpn-detected` (40). A clean verdict raises nothing.

The VPN and ASN results also feed the second ban check, which runs when the
version reply arrives.

### Signals adding up

Every raised signal triggers a recount. Signals stay until something clears
them; for example, chat flood and aim jitter clear after a quiet period. An
admin can inspect them (`!signals`), nudge the score (`!signaladd`) or
re-weight a signal (`!signal_weight`). When the total reaches the threshold,
the player is removed (Phase 7).

### Periodic and admin-driven events

- **Connect flood releases:** every frame, q2admin checks for addresses whose
  connect flood cooldown has ended. For each one, it prints a release message
  and runs `connectfloodreleasecmd` with the address (for example
  `delblackhole 192.0.2.7`), and the address starts again with a clean count.
  This happens whether or not that player ever comes back.
- **Recurring checks:** the timescale probe (every 15 s) and client variable
  polling (every `checkvar_poll_time`) continue for the whole visit.
- **Player timers** (`timer_start`) are checked every frame. When one
  expires, its command is stuffed to the player.
- **Votes:** while a vote is open, players are reminded every
  `clientremindtimeout` seconds. When it passes, the command runs 5 seconds
  later.
- **Admin actions** such as `!mute`, `!stifle`, `!freeze`, `!stuff`,
  `!ban ... ` (which checks everyone currently connected) and `!kick` act on
  the player directly, or through the same queue.


## Phase 6: Map changes

When the map changes, the player stays connected, but q2admin resets a lot
about them.

1. **`SpawnEntities` runs before anyone re-enters.** For each player who is
   still in use:
   - Every flag is cleared except a small set that's kept on purpose: chat,
     name and skin silences, the *caught* verdict, kicked, Nitro2-proxy,
     tests already passed, banned and reconnect.
   - Their **job queue is emptied**.
   - Their flood counters, retry counters, zbot test position, turning-speed
     baselines and vote counters are reset.

   In addition, these apply to the server as a whole:
   - The `q2startNN`/`q2eNN` digits and the zbot test digits are
     **re-randomized**, so a cheat can't learn them on one map and answer
     automatically on the next.
   - Disabled entities and item swaps are applied to the new map.
   - The ban, lrcon, flood, vote, disable and client variable lists are
     reloaded from disk, along with the anti-cheat lists.
2. **The player's client reloads the map and the engine calls `ClientBegin`
   again.** Phase 3 runs in full, `inuse` is cleared, and **the whole of
   Phase 4 runs again**: startup handshake, version probe (with its second
   ban check), zbot character test, alias test, ratbot tests, private
   commands and so on.

**`ClientConnect` is *not* called again on a map change**, so the Phase 1
checks (lockdown, IP and the first ban check) don't repeat. Bans still catch
the player through the version-reply ban check on every map, the name-change
re-check, and the immediate check when an admin adds a ban.

A player's signals and score survive the map change; only the per-map
counters are reset. Connect flood counts and holds aren't tied to a map
either, so they carry on as normal. They're only lost when the game library
itself is unloaded (a full `map` restart rather than `gamemap`, or a server
shutdown). At that point, every temporary hold still active is released
early by running `connectfloodreleasecmd`, so the engine isn't left blocking
an address forever.


## Phase 7: Leaving

### How a player leaves

| Cause | What q2admin does first |
| --- | --- |
| The player types `disconnect` or `quit`, or their connection times out. | Nothing; the engine notices and calls `ClientDisconnect`. |
| A queued disconnect (ban, signal threshold, flood, VPN limit, IP limit, failed private command, and so on). | The `QCMD_DISCONNECT` job marks the player as kicked, logs a `CLIENTKICK` event with the reason, tells the player "You have been kicked", and runs `kick <slot>` on the server. |
| An immediate kick (userinfo flood, the `\skon\` signature, a queue flood, `!kick`, `!boot`). | `kick <slot>` runs straight away. |
| A rejection in `ClientConnect`. | The player never entered, so `ClientDisconnect` isn't called for them. |

Every queued disconnect also prints "*name* is being disconnected: *reason*"
to the server console when it's queued, so the log shows why a player went
even when the kick itself happens a second later.

### `ClientDisconnect()`

1. **Cloud Admin** is told the player left.
2. **The mod** is told, unless the player was in `BANCHECK` limbo; the mod
   never knew about them, so it isn't told they left.
3. **Lockdown.** If the server is locked, the player's name is added to the
   rejoin list. Players in limbo don't get a place, so banned players can't
   use a lockdown to get back in.
4. **Logging.** A `CLIENTDISCONNECT` event is logged.
5. **Ban slot release.** If the player was let in by a ban rule with `MAX`,
   that rule's count drops by one.
6. **Cleanup.** Any file being stuffed to the player is closed.
7. **The record is wiped:** flags, admin and bypass logins, the job queue,
   retry counters, probe deadlines, VPN and IPLogs results, the address,
   userinfo and msec state. The slot is ready for the next player.

The player's whois record stays in memory and is saved to `whois.dat` when
the whois data is written. Their IPLogs result stays in the cache for
`iplogs_cache_ttl`, so a quick reconnect from the same address doesn't cost
another lookup.


## Appendix A: A clean client's conversation, line by line

Below is a typical first map for a genuine, unmodified client with default
settings. Random strings are shortened for readability, and `q2start37` and
`q2e81` stand in for the per-map randomized markers. **→** is q2admin sending
to the client (stuffed unless marked *print*); **←** is the client's reply
arriving as a command.

```
B+0.0  (ClientBegin)   MOTD shown; probe jobs queued
B+2.0  → q2start37
B+2.1  ← q2start37                          handshake passed, inuse set
B+2.1  → kQ3v… $version
B+2.2  ← kQ3v… q2pro r2160~5ad3a40 …        version stored, bans re-checked
B+2.2  → !setadmin $q2adminpassword         (only if adminpassword is set)
B+2.3  ← !setadmin                           empty password, ignored
B+2.3  → Tm9x… $timescale
B+2.4  ← Tm9x… 1                             timescale OK
B+3.1  → !FU52ab  /  q2e81                   zbot test round 1 of 23
B+3.2  ← !FU52ab                             round passed
B+3.2  ← q2e81                               start round 2
B+3.3  → @FU52cd  /  q2e81
       …                                     23 rounds, about 0.2 s each
B+3.2  → .please.disconnect.all.bots  (x3)
B+3.3  ← .please.disconnect.all.bots  (x3)   discarded
B+3.3  → alias Ra7… Zq1…
B+4.3  → Ra7…
B+4.4  ← Zq1…                                alias test passed
B+7.0  → exec cfg/all.cfg / exec cfg/q2dm1.cfg
B+14   → (print) ratbot Detect Test ( rbkck &%trf .disconnect )
B+17   → Pq0… $timescale       ← Pq0… 1      every 15 s from here on
B+16   → name pwsnskle;wait;wait;name "claire"
B+16   ← (userinfo update: name pwsnskle)    ratbot name test passed, hidden
B+16   ← (userinfo update: name claire)      unchanged, ignored
B+62   → Lw2… $gl_modulate     ← Lw2… 1      first client variable check
```

The exact timing varies with the player count (each player gets at most one
job per frame) and with the client's ping.


## Appendix B: What each failed check costs

| Check | Failure | Consequence |
| --- | --- | --- |
| Extra userinfo present | Second block missing | Rejected at connect. |
| Required userinfo keys | `name`, `rate` or `skin` missing | Rejected at connect. |
| Userinfo format | Malformed | Rejected at connect. |
| Proxy userinfo keys | `Nitro2`/`bwproxy` not allowed | Rejected or banned. |
| MVD imposter | `mvdspec` from a real player | `mvd-imposter` (removal). |
| Skin length | Too long | Skin replaced, `skin-overflow` (25). |
| Lockdown | Locked and not on the rejoin list | Rejected or banned. |
| IP address | Unparseable (with `checkclientipaddress`) | Rejected or banned. |
| Connect flood | Third connection from one address within 60 s | Refused, `connectfloodcmd` runs, and the address is held off for 300 s. |
| Ban list at connect | Matches a deny rule | Rejected, or shown the message and removed. |
| Ban list after version | Matches a deny rule | Shown the message and removed. |
| Address limit | Over `ip_limit` | Disconnected. |
| Startup handshake | No `q2startNN` after `startup_attempts` (3) sends | Zbot verdict, about 17 s after entering. |
| Version probe | No reply within 1 s | `version-probe` (20). |
| Protocol | Older than the client's engine supports | `protocol-downgrade` (10). |
| Timescale probe | Reply isn't `1` | `timescale-modified` (removal). |
| Timescale probe | No reply within 1 s | `timescale-probe` (20). |
| Timescale userinfo | `0` | Kicked. |
| `cl_maxfps` userinfo | `0` | Kicked. |
| Zbot character test | Test string lost 4 times, or no reply | `zbot-detected` (removal, if `disconnectuser`). |
| Aim jitter | More than `zbc_jittermax` in a row | Zbot verdict. |
| Aim jitter | Isolated hits | `aimbot-jitter` (15 each). |
| Snap-fire | Snap onto a target while firing | `snap-fire` (15 each). |
| Tracking | Too steady on a moving target | `aim-track` (30). |
| Alias test | `alias` or *A* forwarded unexpanded | `alias-unsupported` (removal). |
| Alias test | No reply within 1 s | `alias-probe` (20). |
| `rate` in stored userinfo | Missing | `wonky-userinfo` (15). |
| Reconnect check | Decoy alias run | `bad-client` (removal) when next seen. |
| Ratbot print test | Bot reply phrase | `ratbot-detected` (removal). |
| Ratbot name test | Temporary name never arrives (4 tries) | `ratbot-detected` (removal, if `disconnectuser`). |
| Private commands | Missing reply, or inverted reply received | `PRIVATELOG`; kicked if `private_command_kick`. |
| Client variable | Out of range | Corrected value stuffed. |
| Client variable | No reply within 1 s | `checkvar-probe` (20). |
| msec | Over the limit, repeatedly | `msec-overrun` (20). |
| msec | Under the minimum, repeatedly | `msec-underrun` (20). |
| Impulses | Listed impulse | `impulse-sent` (10 each). |
| Rate or frame rate | Outside the limits | Corrected value stuffed. |
| `cl_pitchspeed`/`cl_anglespeedkey` | Changed | Logged, announced, optionally kicked. |
| Name change | To a banned name | Reverted, or kicked with `kickonnamechange`. |
| Name, skin or chat floods | Over the limit | Silenced or kicked; chat also raises `chatflood` (15). |
| Userinfo flood | More than 40 changes in 60 s | Kicked. |
| `\skon\` in userinfo | Present | Kicked. |
| Command queue | 45 pending jobs | Kicked. |
| VPN (vpnapi.io) | Positive | `vpn-detected` (40); limited by `ip_limit_vpn`. |
| VPN (IPLogs) | Verdict of `suspicious`, `vpn_likely` or `vpn_detected` | `vpn-suspicious`, `vpn-likely` or `vpn-detected`. |
| Disabled command | Used | Dropped and logged. |
| Chat ban | Matched | Dropped, logged, and the player is told. |
| Rcon password in a command | Present | Dropped and logged as an exploit. |

The weights shown are the defaults. Anything that adds to the score removes
the player once their total reaches `signal_score_threshold`.


## Appendix C: Dormant checks

Some machinery in the code is wired up but doesn't currently act on players.
It's listed here so nobody goes looking for behaviour that won't happen:

- **`dopversion`:** the `p_version` proxy query has an empty branch in
  `QCMD_TESTSTANDARDPROXY`, so nothing is sent.
- **The p_modified, GL driver and command-queue checks**
  (`PMOD_TimerCheck()`, `gl_driver_check`, `q2a_command_check`,
  `QCMD_GETCMDQUEUE`): the code that handles the replies exists, but nothing
  starts these checks.
- **`QCMD_SPAMBYPASS`, `QCMD_PMODVERTIMEOUT`, `QCMD_GL_CHECK`:** these jobs
  can be queued, but they have empty handlers.
- **`msec_action`:** any value other than 1 (*do nothing*) raises the msec
  signals. The difference between 0 (*legacy*) and 2 (*announce and kick*)
  isn't currently acted on; the signal threshold decides the outcome.
