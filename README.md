# Q2Admin

Q2Admin is a server-side administration and anti-cheat layer for Quake 2
servers. It sits between the Quake 2 server and whatever game mod you run
(baseq2 deathmatch, OpenTDM, CTF, Rocket Arena 2, Jump, and so on), and adds
cheat detection, banning, flood control, voting, logging, client variable
enforcement and many other admin tools. The mod itself doesn't need to be
modified.

- [How it works](#how-it-works)
- [Why you need it](#why-you-need-it)
- [Installation](#installation)
- [Configuration basics](#configuration-basics)
- [Features](#features)
- [Configuration reference](#configuration-reference)
- [Commands](#commands)
- [Compiling](#compiling)
- [Dependencies](#dependencies)


## How it works

A Quake 2 server runs its gameplay from a *game library*: `gamex86_64.so` on
64-bit Linux, `gamex86.dll` on 32-bit Windows, and so on. The engine calls into
that library every time a player connects, types a command, changes their
userinfo, moves, or a new map starts.

Q2Admin is itself a game library. You install it under the name the engine
expects, and point it at the real mod library. When the engine loads q2admin,
q2admin loads the real mod and forwards every call to it:

```
 Quake 2 server  ──calls──▶  q2admin  ──forwards──▶  real mod (gamex86_64.real.so)
 (q2pro, r1q2,   ◀──────────           ◀────────────
  yquake2, ...)
```

Because every interaction between the server and the mod passes through it,
q2admin can inspect and modify the traffic on the fly:

- **Connections:** it can reject banned players before the mod ever sees them.
- **Commands and chat:** it can filter, mute, flood-limit or disable them.
- **Movement:** it can measure each player's movement packets (usercmds) for
  speed hacks and aim assistance.
- **Map entities:** it can strip or substitute items before the mod spawns them.
- **Players' clients:** it can send commands to a player's client ("stuff"
  them) to probe for proxies, modified clients and illegal client variables.

## Why you need it

Quake 2 was released in 1997, and most of its mods were written with no
thought for hostile players. Running a public server without protection
invites problems the engine and mods do nothing about:

- **Cheating clients.** Aim bots (zbot, ratbot) and client-side proxies
  that aim for the player have existed since the late 90s, alongside modified
  clients that run speed hacks, timescale cheats or wallhacks. The
  game mod can't see any of this; q2admin actively probes for it and analyzes
  player input for the signatures these cheats leave.
- **Abuse.** Chat spam, name-change and skin-change spam, offensive names,
  players who come back on a VPN after being kicked, and players who try to
  crash the server with malformed userinfo.
- **Missing admin tools.** Most mods have no ban list, no mute, no voting and
  no way to delegate limited control to moderators without handing out the
  full rcon password.
- **No record of what happened.** The stock server log is thin. q2admin can
  log connections, chat, name changes, kicks, bans and detections to
  separate, custom-formatted files.

Q2Admin solves these problems in one place, for any mod, without changing the
mod's code.


## Installation

1. **Build or download** the q2admin library for your platform (see
   [Compiling](#compiling)). The server, q2admin and the mod must all use the
   same architecture: a 64-bit server needs a 64-bit q2admin and a 64-bit mod.
   Many old mods only exist as 32-bit binaries, so the mod you want to run
   often decides the architecture.

2. **Rename the real mod library** so it doesn't collide with q2admin. Pick
   *one* of these ways to tell q2admin where the real mod is. Earlier entries
   take priority over later ones:
   - Start the server with `+set gamelib <filename>`.
   - Set `gamelibrary "<filename>"` in `q2admin.cfg`. **This is the
     recommended option.**
   - Name it `game<arch>.real.<ext>`, for example `gamex86_64.real.so`
     (64-bit Linux), `gamex86.real.dll` (32-bit Windows) or `gamei386.real.so`
     (old 32-bit Linux builds of R1Q2 and 3.20).

3. **Install q2admin** in the mod directory under the name the engine loads,
   for example `opentdm/gamex86_64.so`.

4. **Copy the example configs.** The [`runtime-config/`](runtime-config/)
   folder contains a fully commented example of every config file. Copy the
   `.cfg` files into your mod directory and edit them to suit your server.
   Start with `q2admin.cfg`.

5. **Start the server.** q2admin prints its version and the features it
   negotiated with the engine and the mod.


## Configuration basics

### Config files

| File | Purpose |
| --- | --- |
| `q2admin.cfg` | Main settings: everything in the [options table](#options). |
| `q2a_ban.cfg` | Ban and chat-ban rules, plus `INCLUDE:` of local files or URLs. |
| `q2a_cvar.cfg` | Client variables to enforce (constant values or ranges). |
| `q2a_disable.cfg` | Mod commands players aren't allowed to use. |
| `q2a_flood.cfg` | Extra commands that count toward flood protection. |
| `q2a_lrcon.cfg` | Limited-rcon passwords and the commands each one may run. |
| `q2a_vote.cfg` | Commands players are allowed to call votes on. |
| `q2a_spawn.cfg` | Map entities that should never spawn. |
| `q2a_log.cfg` | Log file definitions and which events go to which file. |
| `q2a_login.cfg` | In-game admin accounts: `name password level` per line. |
| `q2a_bypass.cfg` | Anti-cheat bypass accounts: `name password level` per line. |
| `q2a_cloud.cfg` | Cloud Admin connection settings (still under development). |

Each file is read from the Quake 2 root directory first and then from the mod
directory. Later values override earlier ones, so you can keep shared defaults
in the root and per-mod settings beside the mod. The file names can be changed
with the engine cvars listed under [Engine cvars](#engine-cvars).

Settings in `q2admin.cfg` use the format `option "value"`, one per line.
Lines starting with `;` are comments. Booleans accept `yes`/`no`/`1`/`0`
(any case). Unknown options are silently ignored, so check the spelling if
a setting doesn't seem to take effect. Lines are limited to 255 characters.

### Running commands

**From the server console (or over rcon),** prefix the command with `sv !`:

```
sv !kick CL 3
sv !ipbanning_enable no
rcon mypassword sv !mute claire 60
```

Typing a setting's name with no value shows its current value. Typing it with
a value changes it until the next restart. Change `q2admin.cfg` to make the
change permanent.

**From in-game,** an authenticated admin (see
[Admin access](#admin-access)) uses the same commands with just a `!` prefix,
such as `!kick CL 3`. Some commands only work from the server console; the
[Commands](#commands) section notes which.

Command and option names can be abbreviated to any unique prefix. If a prefix
matches more than one name, the first one in q2admin's internal table wins,
so it's safest to type names in full.

### Choosing players

Commands that act on a player accept a *player spec*:

| Spec | Meaning | Example |
| --- | --- | --- |
| `CL <n>` | Client slot number (as shown by `status`) | `CL 3` |
| `<name>` | Exact name, with `*` and `?` wildcards | `claire`, `"cl*"` |
| `LIKE <text>` | Name containing the text | `LIKE clai` |
| `RE <regex>` | Name matching a regular expression | `RE "^cl[a4]ire$"` |

A name pattern must match exactly one player. Commands that accept *several*
players (`kick`, `say_group`) also take `CL 1 + 4 + 7`.

### Match types

Rules in the disable, flood, lrcon, vote and spawn lists start with a match
type:

| Type | Matches when the text... |
| --- | --- |
| `EX` | equals the pattern exactly |
| `SW` | starts with the pattern |
| `RE` | matches the pattern as a regular expression (case-insensitive) |

In the config files they're written `EX:pattern`. On the console they're
written `EX "pattern"`.


## Features

### Proxy and modified-client detection

**What it is.** Detection of client-side aim proxies (zbot, ratbot, and
related tools) and of modified Quake 2 executables.

**Why it matters.** A proxy sits between a player's client and the server and
rewrites their aim and inputs. Nothing about it is visible to the game mod.

**How it works.** When a player connects, q2admin sends commands to their
client that have randomized names. A genuine client handles these in a
specific, predictable way; proxies and hacked clients don't. These probes
include:

- alias tests and `connect` tests;
- a `p_version` query, which proxies tend to answer;
- a client version request;
- a timescale check;
- optional admin-defined "private commands" whose responses the server waits
  for.

If a response is missing or wrong, the player is flagged, the event is
broadcast and logged, and they're kicked if configured to. Proxies leave
recognizable keys in userinfo (`Nitro2`, `bwproxy`); these are detected at
connect time. Known zbot control impulses (169–175) can also be flagged
and kicked.

With `enforce_deadlines` enabled, each probe has a deadline. A client that
never answers raises a signal (`version-probe`, `alias-probe`,
`timescale-probe`, `checkvar-probe`) instead of hanging around indefinitely.

The optional *reconnect* check (`reconnect_address`) makes every new player
disconnect and reconnect directly to the server's real address. A proxy
sitting in between is dropped in the process.

**Configuring it.**

```
; q2admin.cfg
zbotdetect "yes"
disconnectuser "yes"            ; kick detected proxy users
displayzbotuser "yes"           ; tell everyone
zbotuserdisplay "%s is using a client side proxy."
dopversion "yes"
timescaledetect "yes"
enforce_deadlines "yes"
impulsestokickon "169, 170, 171, 172, 173, 174, 175"
disconnectuserimpulse "yes"
private_command1 "mysecretcheck"
private_command_kick "yes"
```

From the console: `sv !zbotdetect yes`, `sv !disconnectuser no`, and so on.
Related options: `startup_attempts`, `clientsidetimeout`, `zbotdetectactivetimeout`,
`randomwaitreporttime`, `proxy_bwproxy`, `proxy_nitro2`, `customclientcmd`,
`customservercmd`, `hackuserdisplay`, `numofdisplays`, `do_franck_check`,
`do_vid_restart`, `inverted_command1-4`.

### Aim analysis

**What it is.** Three detectors that look at how a player's view angles move
from one movement packet to the next:

- **Jitter (`zbc_*`):** catches the characteristic view snapping of zbots and
  ratbots.
- **Snap-fire (`snapfire_*`):** catches a view that jumps onto a player who
  wasn't near the crosshair, at the exact moment the player fires. This is the
  fingerprint of a silent-aim lock-on.
- **Sustained tracking (`track_*`):** catches a crosshair that stays glued to
  a moving target for longer than a human plausibly could.

**Why it matters.** Modern aim assists don't announce themselves with probes;
they only show up in the input stream.

**How it works.** Every usercmd is compared with the previous one and with the
positions of visible enemies. Confirmed jitter counts as a cheat detection;
snap-fire and tracking raise the `snap-fire` and `aim-track`
[signals](#signals-and-scoring) so that several weak indicators add up rather
than one false positive kicking someone.

**Configuring it.**

```
zbc_enable "yes"
zbc_jittermax "4"
zbc_jittertime "10"
zbc_jittermove "500"
snapfire_enable "yes"
snapfire_min_snap_deg "30"
snapfire_off_crosshair_deg "40"
track_enable "yes"
track_tight_deg "4"
track_min_motion_deg "15"
```

All of these can be changed live, for example `sv !track_tight_deg 3`.

### Speed hack detection (msec)

**What it is.** A check on how much game time each player consumes.

**Why it matters.** Every movement packet carries an `msec` value saying how
much time it covers. A speed hack inflates it so the player moves faster than
everyone else. A subtler cheat under-reports it for a while and then spends
the banked time all at once.

**How it works.** q2admin adds up each player's msec over a window of
`msec_timespan` seconds:

- Going over `msec_max_allowed` raises `msec-overrun`.
- Going under `msec_min_required` raises `msec-underrun`.

After `msec_max_violations` violations, `msec_action` decides what happens.
Admins can see each player's recent totals with the in-game `!dumpmsec`
command, and can pin a player in place with `freeze`.

**Configuring it.**

```
msec_timespan "5"
msec_max_allowed "5600"      ; about (1000 * timespan) * 1.12
msec_min_required "0"        ; about (1000 * timespan) * 0.88 if you want it strict
msec_max_violations "2"
msec_action "2"              ; 0 = legacy kick, 1 = do nothing, 2 = announce and kick
```

Packet loss can make honest players look like they're under-reporting, so
leave `msec_min_required` at 0 unless you know your players' connections are
good.

### Signals and scoring

**What it is.** A shared scoring system for suspicious behaviour. Each
detector raises a named *signal* on a player, and every signal carries a
weight. When a player's total score reaches `signal_score_threshold`, they're
removed from the server.

**Why it matters.** Most individual heuristics (a VPN connection, a missed
probe, one unusual aim snap) are too weak to justify a kick on their own.
Scoring lets several of them add up, while confirmed cheats are weighted
heavily enough to remove a player straight away. Admins can see exactly why a
player is suspicious without anyone being kicked.

**How it works.** Signals stay on a player until the detector clears them, and
some decay over time. The built-in signals are listed below. A weight of
1000000 means a single hit removes the player.

| Signal | Default weight | Raised when |
| --- | --- | --- |
| `aimbot-jitter` | 15 per hit | aim jitter below the confirmed-bot threshold |
| `chatflood` | 15 | player is chat-flooding |
| `snap-fire` | 15 per hit | view snapped onto a target as they fired |
| `aim-track` | 30 | crosshair stayed on a moving target too tightly for too long |
| `skin-overflow` | 25 | player tried to use an oversized skin name |
| `version-probe` | 20 | client version probe went unanswered |
| `timescale-probe` | 20 | timescale probe went unanswered |
| `alias-probe` | 20 | alias probe went unanswered |
| `checkvar-probe` | 20 | client variable probe went unanswered |
| `msec-overrun` | 20 | used more msec than allowed |
| `msec-underrun` | 20 | used less msec than required |
| `impulse-sent` | 10 per impulse | sent a flagged impulse |
| `vpn-suspicious` | 20 | VPN check was 25–50% sure |
| `vpn-likely` | 30 | VPN check was 50–75% sure |
| `vpn-detected` | 40 | vpnapi.io reported a VPN, or IPLogs was 75–100% sure |
| `wonky-userinfo` | 15 | a standard userinfo key is missing |
| `protocol-downgrade` | 10 | client connected with an older protocol than its engine supports |
| `admin-adjustment` | set per player | an admin used `signaladd` |
| `ban-entry` | set per ban | a ban entry with a `SCORE` matched the player |
| `timescale-modified` | 1000000 | client timescale isn't 1 |
| `zbot-detected` | 1000000 | zbot confirmed |
| `ratbot-detected` | 1000000 | ratbot confirmed |
| `proxy-detected` | 1000000 | proxy confirmed |
| `alias-unsupported` | 1000000 | client mishandled the `alias` command |
| `bad-client` | 1000000 | client behaved in a way no genuine client does |
| `mvd-imposter` | 1000000 | a player's userinfo claims to be q2pro's MVD recording client |

**Configuring it.**

```
signal_score_threshold "50"      ; 0 = never remove, just track
signal_weight "snap-fire 25"     ; one line per signal you want to re-weight
signal_weight "vpn-detected 60"
```

From the console:

```
sv !signals claire                ; show a player's signals and score
sv !signal_weight snap-fire 25    ; re-weight live
sv !signaladd claire 30           ; add evidence by hand (or -30 to forgive)
```

### Banning

**What it is.** A rule-based ban list that can match on any combination of:

- player name (exact, contains, or regex);
- IP address or CIDR range (IPv4 and IPv6);
- VPN status;
- ASN, which covers an entire ISP;
- client version string.

**Why it matters.** Plain IP bans are easy to dodge and catch innocent players
on shared addresses. Combining attributes lets you be precise. Inclusive
rules let you *allow* specific players, so you can, for example, reserve a
clan tag for players who know a password.

**How it works.** Each rule is either exclusive (`-`, keep matching players
out; this is the default) or inclusive (`+`, let matching players in).
Rules are checked newest-first; in the file, that means from the bottom up.
The first rule that matches decides the outcome, so put exceptions *after*
the broader rule they override.

A rule can also:

- `PASSWORD`: require a password, which the player supplies by setting the
  `pw` userinfo key with `set pw "secret" u`;
- `MAX`: limit how many players may match it at once;
- `FLOOD`: apply custom chat-flood limits to matching players;
- `SCORE`: add a signal score to matching players;
- `MSG`: show a custom message;
- `TIME`: expire after a number of minutes.

Bans are checked when players connect (`banonconnect yes`, so banned players
never take a slot) or when they enter the game (`banonconnect no`, so they see
the ban message before being dropped). New bans are also checked against
everyone already playing unless you add `NOCHECK`.

Ban files can `INCLUDE:` other files or URLs, which lets several servers share
one list. Setting the engine cvar `q2adminbanremotetxt_enable 1` makes
q2admin also download a shared list from the URL in
`q2adminbanremotetxt_file`.

**Configuring it.**

```
; q2a_ban.cfg
BAN: NAME BLANK MSG "no blank names allowed"
BAN: IP 192.0.2.0/24 MSG "go away"
BAN: IP VPN MSG "VPNs aren't allowed here"
BAN: ASN as7843
BAN: + NAME RE "^\[VK\]" PASSWORD "duck" MSG "Name reserved for clan VK"
BAN: + ALL MAX 10
INCLUDE: "https://example.com/shared-bans.cfg"
```

```
; q2admin.cfg
banonconnect "yes"
ipbanning_enable "yes"
nickbanning_enable "yes"
versionbanning_enable "yes"
defaultbanmsg "You are banned from this server!"
```

From the console: `ban`, `listbans`, `delban`, `reloadbanfile` (see
[Commands](#commands)).

### Chat filtering

**What it is.** Blocking chat messages that contain banned words or patterns.

**Why it matters.** It keeps slurs, spam links and harassment off the server
without an admin having to be present.

**How it works.** Each chat message is compared with the chat-ban list, which
supports "contains" and regex matching. Blocked messages are never shown to
other players. The sender sees `defaultchatbanmsg`, or the rule's own `MSG`,
and the attempt is logged (`CHATBAN` log type). `filternonprintabletext`
also strips high-ASCII console characters from chat.

**Configuring it.**

```
; q2a_ban.cfg
CHATBAN: "badword"
CHATBAN: RE "f[a4]rt" MSG "potty talk isn't allowed"
```

```
; q2admin.cfg
chatbanning_enable "yes"
defaultchatbanmsg "Message banned."
```

From the console: `chatban`, `listchatbans`, `delchatban`.

### VPN detection

**What it is.** Looking up each connecting player's IP address to find out
whether it belongs to a VPN or hosting provider.

**Why it matters.** Kicked and banned players often come straight back on a
VPN. Knowing who is on one lets you add weight to their signal score, ban
VPNs entirely, or limit how many can join.

**How it works.** Two independent lookup services are available. Both run in
the background, so a slow lookup never stalls the server:

- **vpnapi.io** (`vpn_enable`): needs a free API key. It reports whether the
  address is a VPN, proxy, Tor exit or relay, and which network (ASN) it
  belongs to. A positive result raises the `vpn-detected` signal.
- **IPLogs** (`iplogs_enable`): no key needed. It returns a confidence score,
  which raises the `vpn-suspicious`, `vpn-likely` or `vpn-detected` signal.
  Results are cached per address for `iplogs_cache_ttl` seconds, and
  addresses in `iplogs_ignorelist` are never checked.

Both need `http_enable`. The vpnapi.io results drive `ip_limit_vpn`, which
limits how many VPN players can connect from the same provider (ASN)
separately from the per-address `ip_limit`. The `IP VPN` and `ASN`
[ban rules](#banning) also use those results.

A VPN on its own doesn't remove anyone: `vpn-detected` is worth 40 points,
below the default `signal_score_threshold` of 50, so it only removes a player
together with other signals. To keep VPN users out entirely, raise its weight
to the threshold (`signal_weight "vpn-detected 50"`) or add an `IP VPN` ban
rule, which also lets you show them a message.

**Configuring it.**

```
http_enable "yes"
vpn_enable "yes"
vpn_api_key "your-key-from-vpnapi.io"
iplogs_enable "yes"
iplogs_ignorelist "192.0.2.0/24, 2001:db8::/32"
ip_limit_vpn "2"
```

`sv !vpnusers` lists everyone currently connected through a VPN.

### Connection limits and server lockdown

**What it is.** Limits on how many players may connect from one address, a
check that connecting players have a valid IP address, and a "lock" switch
that refuses all new players.

**Why it matters.** It stops one person from filling the server with
duplicate clients, and lets you close the server temporarily (for
maintenance, or for a match in progress) without changing the password.

**Configuring it.**

```
ip_limit "3"                 ; max players from one IP, 0 = unlimited
checkclientipaddress "yes"
lockoutmsg "This server is currently locked."
```

From the console: `sv !lock yes` and `sv !lock no`.

Locking only keeps out *new* players. If a player who was already on the
server quits, times out or is disconnected while it's locked, they can
rejoin, so a dropped connection doesn't lock them out of their own match.
Some details:

- Players are recognized by name, so they must reconnect with the same name.
  Anyone using that name gets the place.
- Each departure allows one rejoin. A player who leaves again while the
  server is still locked gets a new place.
- Players who left before the server was locked aren't let back in.
- Kicking someone doesn't stop them from rejoining. To keep them out, ban
  them instead.
- Running `lock` again, whether to lock or unlock, clears the list of players
  waiting to rejoin.

### Flood protection

**What it is.** Rate limits on chat, name changes, skin changes, userinfo
changes and any mod command you choose.

**Why it matters.** Chat macros, name-cycling scripts and skin-change spam
make a server unplayable for everyone else, and some of them can also
overload slow clients.

**How it works.** Each kind of flood is set with three numbers: *count*,
*seconds* and *silence*. Making more than *count* changes within *seconds*
mutes or blocks the player for *silence* seconds; a *silence* of 0 kicks
them instead. Each player is tracked separately. Commands listed in
`q2a_flood.cfg` count toward the chat flood limit, which is useful for
sound-playing commands. Userinfo changes are limited by
`userinfochange_count` and `userinfochange_time`.

**Configuring it.**

```
; q2admin.cfg
chatfloodprotect "5 2 10"            ; or "disable"
chatfloodprotectmsg "%s is making too much noise."
namechangefloodprotect "5 2 10"
skinchangefloodprotect "5 2 10"
userinfochange_count "40"
userinfochange_time "60"
fpsfloodexempt "yes"                 ; don't count cl_maxfps changes (Jump mod)
```

```
; q2a_flood.cfg
SW:play_
```

From the console: `chatfloodprotect`, `namechangefloodprotect`,
`skinchangefloodprotect`, `clientchatfloodprotect` (per player), `floodcmd`,
`listfloods`, `flooddel`, `chat_stats`.

### Connect flood protection

**What it is.** Holding off an IP address that connects to the server over
and over in a short time.

**Why it matters.** A muted or stifled player can get around the mute by
using their *name* to talk: they disconnect, change their name to the next
part of their message, and reconnect, again and again. That also fills
everyone's screen with join and leave messages. Banned players retrying in a
loop cause the same noise.

**How it works.** q2admin counts connections from each IP address. It's
modelled on `chatfloodprotect` and set with three numbers: *count*, *seconds*
and *cooldown*. The connection that brings an address to *count* connections
within *seconds* trips the limit. Then:

1. That connection is refused with `connectfloodprotectmsg`, and the event is
   logged under the `BAN` log type.
2. `connectfloodcmd` runs on the server, with `%i` replaced by the address.
   Use it to have the engine drop the address itself, which is cheaper than
   q2admin refusing each attempt. Different engines name these commands
   differently, which is why they're configurable.
3. Until *cooldown* seconds have passed, q2admin refuses every connection
   from that address. This works even if no engine command is configured.
4. When the cooldown ends, `connectfloodreleasecmd` runs with the same
   address, and the address starts again with a clean count.

Some details:

- **Negative cooldown:** a negative *cooldown* holds the address off until the
  server restarts, and the release command never runs.
- **The reconnect check isn't counted:** when `reconnect_address` makes a new
  player reconnect, the second connection isn't counted, so one join counts
  once.
- **Banned players are caught too:** connections are counted before the ban
  and lockdown checks, so a banned player retrying in a loop is held off as
  well.
- **The recording client is exempt:** Q2Pro's MVD recording client is never
  counted.
- **Shared addresses:** everyone behind one address (a LAN party, a household,
  or carrier-grade NAT on mobile networks) shares one count. Three of them
  joining within a minute trips the default limit, so raise *count* if your
  players often share an address.
- **Blocks don't survive an unload:** q2admin's tracking is lost when the game
  library is unloaded (a full `map` restart rather than `gamemap`, or the
  server shutting down). So that engine blocks aren't left in place forever,
  any temporary block still active at that point is released early.

**Configuring it.**

```
; q2admin.cfg
connectfloodprotect "3 60 300"     ; 3 connections in 60 s = 5 minutes off; or "disable"
connectfloodprotectmsg "Too many connections, try again later."

; Q2Pro
connectfloodcmd "addblackhole %i"
connectfloodreleasecmd "delblackhole %i"

; R1Q2 (its blackholes are IPv4 only)
;connectfloodcmd "addhole %i"
;connectfloodreleasecmd "delhole %i"
```

From the console: `sv !connectfloodprotect 4 60 600`, or
`sv !connectfloodprotect disable`.

### Muting

**What it is.** Silencing a player's chat entirely (`mute`), or slowing them
down so they can only say one line every so often (`stifle`).

**Why it matters.** It deals with a disruptive player without kicking them.

```
sv !mute claire 300       ; mute for 5 minutes
sv !mute CL 3 PERM        ; mute until they disconnect
sv !stifle claire 60      ; one line, then 60 seconds of silence, repeatedly
sv !unstifle claire
```

### Client variable enforcement

**What it is.** Making sure players' client-side variables (cvars) stay
within limits you set.

**Why it matters.** Many visual and movement exploits are just cvar changes:
a very low `gl_modulate` for brighter players, a huge `cl_maxfps` for jump
tricks, or modified `cl_pitchspeed` for faster turning.

**How it works.** Each variable in `q2a_cvar.cfg` is polled from every player
in turn, one variable every `checkvar_poll_time` seconds. `CT` entries must
have an exact value and `RG` entries must be within a range. A player who is
out of bounds has the correct value forced onto them.

Separately, q2admin can enforce `cl_maxfps` (`maxfps` and `minfps`) and
`rate` (`maxrate` and `minrate`) limits, and can watch for changes to
`cl_pitchspeed` and `cl_anglespeedkey`.

**Configuring it.**

```
; q2a_cvar.cfg
CT: "gl_modulate" "1"
RG: "cl_maxpackets" "30" "100"
```

```
; q2admin.cfg
checkvarcmds_enable "yes"
checkvar_poll_time "60"
maxfps "125"
maxrate "25000"
cl_pitchspeed_enable "yes"
cl_pitchspeed_kick "yes"
```

From the console: `checkvarcmd`, `listcheckvar`, `checkvardel`,
`reloadcheckvarfile`.

### Command disabling

**What it is.** Blocking mod commands you don't want players to use.

**Why it matters.** Some mods ship commands that are buggy, abusable or just
don't suit your server. Disabling them doesn't require recompiling the mod.

**How it works.** Every command a player sends is compared with the disable
list before it reaches the mod. Engine-level commands (`name`, `rate`,
`cl_maxfps`, and so on) are handled by the engine itself and never reach
q2admin, so they can't be disabled this way.

```
; q2a_disable.cfg
EX:kill
SW:say_team
```

```
; q2admin.cfg
disablecmds_enable "yes"
```

From the console: `disablecmd`, `listdisable`, `disabledel`,
`reloaddisablefile`.

### Limited RCON (lrcon)

**What it is.** Extra rcon passwords that can only run specific commands.

**Why it matters.** It lets you hand moderators the power to change maps or
adjust the timelimit without giving them full control of the server. Access
can be revoked without changing the real rcon password.

**How it works.** A player runs `lrcon <password> <command>` from their
console. If a rule for that password matches the command, q2admin runs it.
With `rcon_random_password` on, the real rcon password is temporarily changed
to a random string while the command runs, so it can't be sniffed from the
traffic. It's restored after `lrcon_timeout` seconds. The server's
`rcon_password` must be set to *something* for lrcon to work.

```
; q2a_lrcon.cfg
SW:mapper map
EX:refpass timelimit 20
RE:refpass ^map q2dm[1-8]$
```

Players then use `lrcon refpass map q2dm1`. From the console: `lrcon`,
`listlrcons`, `lrcondel`, `reloadlrconfile`, `resetrcon`.

### Voting

**What it is.** Player voting on any server command you allow, in any mod,
including mods that have no voting of their own (Rocket Arena 2, for
example).

**How it works.** A player proposes a vote with the vote command (`vote` by
default, set with `clientvotecommand`), and everyone else answers
`vote yes` or `vote no`. The proposed command must match a rule in
`q2a_vote.cfg`. If enough players vote yes within `clientvotetimeout`
seconds, the command runs. Players are reminded about the open vote every
`clientremindtimeout` seconds.

```
; q2a_vote.cfg
RE:^map q2dm[1-8]$
SW:fraglimit
```

```
; q2admin.cfg
vote_enable "yes"
clientvotecommand "vote"
votepasspercent "50"
voteminclients "2"
votecountnovotes "yes"
```

From the console: `votecmd`, `listvotes`, `votedel`, `reloadvotefile`.

### Entity disabling and substitution

**What it is.** Stopping chosen items or entities from spawning, or replacing
one item with another across every map.

**Why it matters.** You can remove the BFG or quad damage, or turn power
shields into power screens, without editing any map files.

**How it works.** When a map loads, q2admin rewrites the map's entity string
before the mod sees it:

- Entities whose classname matches a rule in `q2a_spawn.cfg` are removed.
  Rules in `<moddir>/q2adminmaps/<mapname>.q2aspawn` apply to that map only.
- The `spawn_swap_*` engine cvars swap one item for another. Replacements
  must come from a fixed list of standard items, weapons, ammo and keys, so a
  typo can't crash the mod.

The world entity (`worldspawn`) is always protected.

```
; q2a_spawn.cfg
EX:weapon_bfg
SW:item_quad
```

```
; q2admin.cfg
spawnentities_enable "yes"
```

```
; server.cfg (engine cvars)
set spawn_swap_powershield "item_power_screen"
set spawn_swap_bfg "weapon_railgun"
```

From the console: `spawncmd`, `listspawns`, `spawndel`, `reloadspawnfile`.

### Private messaging

**What it is.** Players can send chat to one player (`say_person`) or a group
of players (`say_group`) instead of everyone. With `extendedsay_enable`, the
same works from normal `say` using `say !p <player> msg` and
`say !g <players> msg`.

```
say_person_enable "yes"
say_group_enable "yes"
extendedsay_enable "yes"
```

Admins can also message players from the console with `say_person`,
`say_person_low` and `say_group`.

### Admin access

Q2Admin has three ways of granting access in-game:

1. **Shared admin password.** Set `adminpassword` in `q2admin.cfg`. A player
   who runs `!setadmin <password>` can then use every q2admin command that's
   allowed in-game, with the `!` prefix (for example `!kick CL 3`).
2. **Named admin accounts with levels.** Each line of `q2a_login.cfg` is
   `name password level`. A player logs in with `!admin <name> <password>`.
   The level is a bitmask; add up the values for the commands you want to
   allow (511 grants everything):

   | Value | Grants |
   | --- | --- |
   | 1 | `!boot <slot>`: kick a player |
   | 2 | `!dumpmsec`: show players' recent msec totals |
   | 4 | `!changemap <map>` |
   | 8 | `!dumpuser <slot>` and `!dumpuser_any <slot>`: show a player's details |
   | 16 | `!auth` and `!gfx`: make every player announce their client version or renderer |
   | 32 | `!dostuff <slot\|all> <commands>`: send commands to players |
   | 128 | `!writewhois`: save the whois database |

3. **Bypass accounts.** Each line of `q2a_bypass.cfg` is
   `name password level`. Players log in with `!bypass <name> <password>`.
   These accounts are meant to exempt known players (for example, Linux or
   macOS players without an anti-cheat client) from client checks.

Players don't have to type these every time. When they connect, q2admin
sends a login command built from the player's own client variables, so a
player who adds these to their `autoexec.cfg` is logged in automatically:

| Login type | Client variables to set |
| --- | --- |
| Shared admin password | `q2adminpassword` |
| Admin account | `q2adminuser` and `q2adminpass` |
| Bypass account | `clientuser` and `clientpass` |

Every login and admin command is logged under the `ADMINLOG` log type.

### Logging

**What it is.** Up to 32 log files, each receiving whichever events you
choose, in a format you define.

**How it works.** `q2a_log.cfg` defines the log files with `LOGFILE:` lines,
then assigns event types to them, each with its own format string. The event
types are:

- **Cheat detection:** `ZBOT`, `ZBOTIMPULSES`, `IMPULSES`, `SIGNALRAISED`,
  `SIGNALCLEARED`.
- **Players:** `CLIENTCONNECT`, `CLIENTBEGIN`, `CLIENTDISCONNECT`,
  `CLIENTKICK`, `CLIENTUSERINFO`, `CLIENTVERSION`, `NAMECHANGE`,
  `SKINCHANGE`, `INVALIDIP`.
- **Commands and chat:** `CHAT`, `CHATBAN`, `CLIENTCMDS`, `CLIENTLRCON`,
  `DISABLECMD`, `PRIVATELOG`.
- **Admin and bans:** `BAN`, `ADMINLOG`.
- **Server:** `SERVERSTART`, `SERVERINIT`, `SERVEREND`, `INTERNALWARN`,
  `PERFORMANCEMONITOR`.
- **Entities:** `ENTITYCREATE`, `ENTITYDELETE`.
- **Cloud Admin:** `CLOUDCONNECT`, `CLOUDDISCONNECT`, `CLOUDERROR`.

Format strings can include placeholders such as `#n` (name), `#i` (IP), `#p`
(ping), `#t`/`#T` (date and time), `#m` (event message), `#w` (signal name)
and `#x`/`#X` (signal score). The full list is in
[`runtime-config/q2a_log.cfg`](runtime-config/q2a_log.cfg). A `%p` in a file
name is replaced with the server port, so several servers can share one mod
directory.

```
; q2a_log.cfg
LOGFILE: 1 MOD "q2admin%p.log"
LOGFILE: 2 MOD "chat%p.log"
CLIENTCONNECT: YES 1 "#T CONN #n #i"
SIGNALRAISED: YES 1 "#T SIGNAL #n #i #w #X"
CHAT: YES 2 "#T #m"
```

`consolelog_enable` also echoes q2admin events to the server console. From
the console: `logfile`, `logevent`, `clearlogfile`, `displaylogfile`,
`flush_logs`.

### Message of the day and per-map client configs

- **`setmotd`** names a text file, relative to the Quake 2 directory, that is
  shown to players when they join. Players can see it again with `motd`.
- **`client_map_cfg`** makes clients run `cfg/all.cfg` and
  `cfg/<mapname>.cfg` on each map, or set a `map_name` variable they can
  script against.
- **`mapcfgexec`** makes the *server* run `mapcfg/<map>-end.cfg` when a map
  ends, and `mapcfg/<map>-pre.cfg` and `mapcfg/<map>-post.cfg` around the next
  map's start.
- **`customclientcmdconnect` and `customservercmdconnect`** run a command on
  the client or the server whenever a player connects. In the server
  command, `%c` is replaced with the client number.

### Whois (alias tracking)

With `whois_active` set to the number of records to keep, q2admin remembers
the names each IP address has connected with and saves them to `whois.dat`.
Any player can type `whois <name or slot>` to see a player's other aliases.

### Player timers and FPS display

- **Timers.** With `timers_active`, players can run
  `timer_start <1-3> <seconds> "<command>"` to run a console command of their
  own after a delay, such as a respawn reminder, and `timer_stop <n>` to
  cancel one. Durations are limited to between `timers_min_seconds` and
  `timers_max_seconds`.
- **FPS display.** `showfps` toggles an on-screen display of the player's
  frame rate.

### Engine and protocol awareness

- **Feature negotiation.** At startup q2admin reads the features the engine
  supports (`sv_features`) and the mod requests (`g_features`). It adapts to
  variable server frame rates, IPv6 addresses, and R1Q2/Q2Pro's extra
  connection data ("extra userinfo").
- **Extra userinfo for older mods.** With `userinfo_proxy`, q2admin asks the
  engine for extra userinfo on the mod's behalf and merges it into the normal
  userinfo string. This gives older mods the challenge, port, protocol and
  zlib details without changing them.
- **Protocol downgrade check.** If a client connects with an older protocol
  than its engine is known to support, the `protocol-downgrade` signal is
  raised.
- **Q2Pro MVD recording client.** Q2Pro can add a server-side spectator
  client that records the game for demos (MVD). q2admin recognizes it from
  data only the engine can set, and excuses it from checks it can't pass. A
  player who tries to pretend to be this client raises `mvd-imposter`.

### Cloud Admin (in development)

Cloud Admin is a separate service, still under development, for managing
groups of servers from one place:

- status and logs across all your servers;
- remote commands;
- delegated admin access;
- shared bans;
- frag accounting;
- moving players between servers (`!teleport`, `!invite`).

Connections are authenticated with RSA keys and encrypted with AES. Settings
live in
`q2a_cloud.cfg`. The `cloud` console command shows the connection's status
and controls it.

### Remote anti-cheat lists

When the `q2adminhashlist_enable` engine cvar is `1`, q2admin downloads
anti-cheat cvar, hash and token lists from `q2adminhashlist_dir`. Reload them
with `reloadhashlist`. A local anti-cheat exception list (`ac.cfg`) is
reloaded with `reloadanticheatlist`.


## Configuration reference

### Engine cvars

Set these on the server's command line (`+set name value`) or in
`server.cfg`. The `q2a_*file` names take effect at startup.

| Cvar | Default | Purpose |
| --- | --- | --- |
| `gamelib` | `gamelibrary`, or `game<arch>.real.<ext>` | Real mod library to load. Overrides `gamelibrary`. |
| `q2aconfig` | `q2admin.cfg` | Main config file name. |
| `q2a_banfile` | `q2a_ban.cfg` | Ban file name. |
| `q2a_bypassfile` | `q2a_bypass.cfg` | Bypass accounts file name. |
| `q2a_cloudfile` | `q2a_cloud.cfg` | Cloud Admin config file name. |
| `q2a_cvarfile` | `q2a_cvar.cfg` | Client variable rules file name. |
| `q2a_disablefile` | `q2a_disable.cfg` | Disabled commands file name. |
| `q2a_floodfile` | `q2a_flood.cfg` | Flood commands file name. |
| `q2a_logfile` | `q2a_log.cfg` | Logging config file name. |
| `q2a_loginfile` | `q2a_login.cfg` | Admin accounts file name. |
| `q2a_rconfile` | `q2a_lrcon.cfg` | Limited-rcon file name. |
| `q2a_spawnfile` | `q2a_spawn.cfg` | Entity disable file name. |
| `q2a_votefile` | `q2a_vote.cfg` | Vote commands file name. |
| `q2adminbanremotetxt_enable` | `0` | `1` downloads a shared remote ban list at startup. |
| `q2adminbanremotetxt_file` | packetflinger.com list | URL of the remote ban list. |
| `q2adminhashlist_enable` | `0` | `1` downloads remote anti-cheat hash lists. |
| `q2adminhashlist_dir` | `https://q2admin.net/server` | Base URL for the hash lists. |
| `q2adminanticheat_enable` | `0` | `1` loads the anti-cheat exception list. |
| `q2adminanticheat_file` | packetflinger.com list | Location of the anti-cheat exception list. |
| `spawn_swap_bfg`, `spawn_swap_chaingun`, `spawn_swap_grenadelauncher`, `spawn_swap_grenades`, `spawn_swap_hyperblaster`, `spawn_swap_invulnerability`, `spawn_swap_machinegun`, `spawn_swap_megahealth`, `spawn_swap_powershield`, `spawn_swap_quad`, `spawn_swap_railgun`, `spawn_swap_rocketlauncher`, `spawn_swap_shotgun`, `spawn_swap_supershotgun` | empty | Classname to spawn in place of that item on every map. Empty leaves the item alone. |

### Options

These go in `q2admin.cfg` (or the file named in the table above). Unless
marked **cfg only**, they can also be viewed or changed from the server console
with `sv !<option> [value]`, and by in-game admins with `!<option> [value]`.
Changes made from the console last until the server restarts.

Types: **bool** is `yes`/`no`; **number** is an integer; **string** is
quoted text; **triple** is `"<count> <seconds> <silence>"` (or
`"<count> <seconds> <cooldown>"` for `connectfloodprotect`) or `"disable"`.

| Option | Type | Default | What it controls |
| --- | --- | --- | --- |
| `adminpassword` | string | empty | Password for `!setadmin`, which gives a player full in-game admin access. Empty disables it. Not settable in-game. |
| `banonconnect` | bool | yes | Reject banned players at connect (they never take a slot) instead of at map entry (they see the ban message first). |
| `chatbanning_enable` | bool | yes | Enable `CHATBAN` rules. |
| `chatfloodprotect` | triple | disable | Chat flood limit. A silence of 0 kicks the player. |
| `chatfloodprotectmsg` | string | `%s is making too much noise.` | Message broadcast when someone chat-floods. `%s` is the player's name. |
| `checkclientipaddress` | bool | yes | Reject players whose IP address can't be determined. |
| `checkvar_poll_time` | number | 60 | Seconds between client variable checks for each player. |
| `checkvarcmds_enable` | bool | no | Enforce the rules in `q2a_cvar.cfg`. |
| `cl_anglespeedkey_display` | bool | yes | Announce `cl_anglespeedkey` changes. |
| `cl_anglespeedkey_enable` | bool | no | Watch for `cl_anglespeedkey` changes. |
| `cl_anglespeedkey_kick` | bool | no | Kick players who change `cl_anglespeedkey`. |
| `cl_anglespeedkey_kickmsg` | string | `cl_anglespeedkey changes not allowed on this server.` | Kick message for the above. |
| `cl_pitchspeed_display` | bool | yes | Announce `cl_pitchspeed` changes. |
| `cl_pitchspeed_enable` | bool | no | Watch for `cl_pitchspeed` changes. |
| `cl_pitchspeed_kick` | bool | no | Kick players who change `cl_pitchspeed`. |
| `cl_pitchspeed_kickmsg` | string | `cl_pitchspeed changes not allowed on this server.` | Kick message for the above. |
| `client_map_cfg` | number | 6 | Bitmask of per-map client configs: 1 = set `map_name`, 2 = exec `cfg/<map>.cfg`, 4 = exec `cfg/all.cfg`. |
| `clientremindtimeout` | number | 10 | Seconds between reminders while a vote is open. |
| `clientsidetimeout` | number | 30 | Seconds to wait for clients to answer proxy probes (minimum 5). |
| `clientvotecommand` | string | `vote` | Command players use to propose and answer votes. |
| `clientvotetimeout` | number | 60 | Seconds a vote stays open. |
| `cloud_address` | string | `[::1]` | Cloud Admin server address (IPv4, IPv6 or host name). |
| `cloud_cmd_invite` | string | `!invite` | Player command to invite players from other servers. Not settable in-game. |
| `cloud_cmd_seen` | string | `!seen` | Player command to find when a player was last seen (not implemented yet). Not settable in-game. |
| `cloud_cmd_teleport` | string | `!teleport` | Player command to move to another server. Not settable in-game. |
| `cloud_cmd_whois` | string | `!whois` | Player command to look up aliases across servers (not implemented yet). Not settable in-game. |
| `cloud_dns` | string | `64` | Address lookup preference: `64` = IPv6 then IPv4, `46` = IPv4 then IPv6, `4` or `6` = only that family. Not settable in-game. |
| `cloud_enabled` | bool | no | Connect to a Cloud Admin server. |
| `cloud_encryption` | bool | yes | Encrypt Cloud Admin traffic. |
| `cloud_flags` | number | 4095 | Bitmask of Cloud Admin features: 1 = frags, 2 = chat, 4 = teleport, 8 = invite, 16 = find, 32 = whois, 1024 = debug. Not settable in-game. |
| `cloud_port` | number | 9988 | Cloud Admin server TCP port. |
| `cloud_privatekey` | string | `private.pem` | This server's private key. |
| `cloud_publickey` | string | `public.pem` | This server's public key, which is given to the Cloud Admin server. |
| `cloud_serverkey` | string | `server.pem` | The Cloud Admin server's public key. |
| `cloud_uuid` | string | all zeros | This server's identifier on the Cloud Admin server. |
| `connectfloodcmd` | string | empty | Server command run when an address trips connect flood protection. `%i` is the IP address, for example `addblackhole %i` (Q2Pro) or `addhole %i` (R1Q2). Not settable in-game. |
| `connectfloodprotect` | triple | `3 60 300` | Connect flood limit: `"<connections> <seconds> <cooldown>"`. The connection that brings one address to that many within the window trips it, and the address is refused for *cooldown* seconds (negative = until restart). `disable` turns it off. |
| `connectfloodprotectmsg` | string | `Too many connections, try again later.` | Message shown to a player refused for connect flooding. |
| `connectfloodreleasecmd` | string | empty | Server command run when an address's cooldown ends. `%i` is the IP address, for example `delblackhole %i` (Q2Pro) or `delhole %i` (R1Q2). Not settable in-game. |
| `consolechat_disable` | bool | no | Stop players from chatting by typing text into the console without a command. |
| `consolelog_enable` | bool | no | Echo logged q2admin events to the server console. |
| `consolelog_pattern` | string | `[q2a] %s\n` | Format for console-echoed events. |
| `customclientcmd` | string | empty | Command sent to a detected cheater's client before they're disconnected. |
| `customclientcmdconnect` | string | empty | Command sent to every player's client when they connect. |
| `customservercmd` | string | empty | Server command run when a cheat is detected. `%c` is the client number. |
| `customservercmdconnect` | string | empty | Server command run when a player connects. `%c` is the client number. |
| `defaultbanmsg` | string | `You are banned from this server!` | Message for ban rules without their own `MSG`. |
| `defaultchatbanmsg` | string | `Message banned.` | Message for chat-ban rules without their own `MSG`. |
| `defaultreconnectmessage` | string | (explains the reconnect) | Shown to players while the reconnect check runs. |
| `developer` | number | 0 | q2admin debug output level. `q2a_developer` is the same setting. |
| `disablecmds_enable` | bool | no | Enforce the rules in `q2a_disable.cfg`. |
| `disconnectuser` | bool | yes | Kick players caught using a proxy or bot. |
| `disconnectuserimpulse` | bool | no | Kick players who send impulses from `impulsestokickon`. |
| `displayimpulses` | bool | no | Announce when players send impulses. |
| `displaynamechange` | bool | yes | Announce name changes. |
| `displayzbotuser` | bool | yes | Announce detected proxy and bot users. |
| `do_franck_check` | bool | yes | Run the "franck" cheat check on connect. |
| `do_vid_restart` | bool | no | Make clients run `vid_restart` on connect, which unloads some wallhacks. |
| `dopversion` | bool | yes | Send the `p_version` proxy probe on connect. |
| `enforce_deadlines` | bool | yes | Raise a signal when a client doesn't answer a probe in time. Not settable in-game. |
| `entity_classname_offset` | number | 300 on 64-bit | Byte offset of `classname` in the mod's entity struct. Needed for `spawnentities_internal_enable` and the entity log types. **A wrong value can crash the server.** |
| `extendedsay_enable` | bool | no | Allow `say !p <player> msg` and `say !g <players> msg`. |
| `filternonprintabletext` | bool | no | Strip non-printable and high-ASCII characters from chat. |
| `fpsfloodexempt` | bool | no | Don't count `cl_maxfps` changes toward userinfo flooding (for Jump mod). |
| `framesperprocess` | number | 0 | Internal: server frames between q2admin processing passes. Leave at 0. |
| `gamelibrary` | string | empty | File name of the real mod library. Not settable in-game. |
| `gamemaptomap` | bool | no | Rewrite `gamemap` to `map`, which reloads the mod on every map. Not recommended. |
| `gl_driver_check` | number | 0 | Legacy GL driver check. The probe isn't currently sent. |
| `gl_driver_max_changes` | number | 3 | GL driver changes allowed before kicking (used with `gl_driver_check`). |
| `hackuserdisplay` | string | `%s is using a modified client.` | Broadcast when a modified client is detected. |
| `http_cacert_path` | string | `/etc/ssl/certs` | Directory of trusted CA certificates for HTTPS. |
| `http_debug` | bool | no | Verbose download logging. |
| `http_enable` | bool | yes | Allow HTTP(S) downloads, which VPN checks, remote ban lists and hash lists need. |
| `http_verifyssl` | bool | yes | Verify HTTPS certificates. |
| `impulsestokickon` | list | all impulses | Comma-separated impulses that count as bot control, for example `169, 170, 171, 172, 173, 174, 175`. |
| `inverted_command1` – `inverted_command4` | string | empty | Private probe commands that a genuine client should *not* answer. |
| `ip_limit` | number | 0 | Maximum players from one IP address, VPN or not. 0 means unlimited. Negative values are rejected. |
| `ip_limit_vpn` | number | 0 | Maximum VPN players from the same provider (ASN), separate from `ip_limit`. Needs `vpn_enable`. 0 means unlimited. Negative values are rejected. |
| `ipbanning_enable` | bool | yes | Enable IP-based ban rules. |
| `iplogs_cache_ttl` | number | 86400 | Seconds an IPLogs result is cached for each IP address. |
| `iplogs_enable` | bool | no | Run the IPLogs VPN check on connect. |
| `iplogs_ignorelist` | string | empty | CIDR ranges exempt from the IPLogs check, separated by spaces or commas. |
| `kickonnamechange` | bool | no | Kick players who change to a banned name, instead of just refusing the change. |
| `lanip` | string | empty | Accepted but currently has no effect. |
| `lock` | bool | no | Refuse new connections. Players who leave while the server is locked can still rejoin under the same name. **Console only**; can't be set in a config file. |
| `lockoutmsg` | string | `This server is currently locked.` | Message shown while the server is locked. |
| `lrcon_timeout` | number | 2 | Seconds before the real rcon password is restored after an lrcon command. |
| `mapcfgexec` | bool | no | Run the server-side `mapcfg/<map>-pre/-post/-end.cfg` files. |
| `max_pmod_noreply` | number | 2 | Unanswered private-command checks allowed before acting. |
| `maxclientsperframe` | number | 100 | Internal: players processed per server frame. Leave alone. |
| `maxfps` | number | 0 | Highest `cl_maxfps` allowed. 0 means no limit. |
| `maximpulses` | number | 1 | Flagged impulses allowed before kicking. |
| `maxmsglevel` | number | 3 | Accepted but currently has no effect. |
| `maxrate` | number | 0 | Highest `rate` allowed. 0 means no limit. |
| `minfps` | number | 0 | Lowest `cl_maxfps` allowed. 0 means no limit. |
| `minrate` | number | 0 | Lowest `rate` allowed. 0 means no limit. |
| `msec_action` | number | 2 | Action on msec violations: 0 = legacy kick, 1 = nothing, 2 = announce and kick. |
| `msec_max_allowed` | number | 5600 | Most msec allowed in each `msec_timespan`. |
| `msec_max_violations` | number | 2 | Violations allowed before `msec_action` applies. |
| `msec_min_required` | number | 0 | Least msec required in each `msec_timespan`. 0 disables the check. |
| `msec_timespan` | number | 5 | Length in seconds of the msec measurement window. |
| `namechangefloodprotect` | triple | disable | Name change flood limit. |
| `namechangefloodprotectmsg` | string | `%s changed names too many times.` | Broadcast when the name change limit is hit. |
| `nickbanning_enable` | bool | yes | Enable name-based ban rules. |
| `numofdisplays` | number | 4 | How many times a cheat detection is announced. |
| `printmessageonplaycmds` | bool | yes | Accepted but currently has no effect. |
| `private_command1` – `private_command4` | string | empty | Private probe commands that a genuine client must answer. |
| `private_command_kick` | bool | no | Kick players who don't answer the private commands. |
| `proxy_bwproxy` | number | 1 | BW-Proxy handling: 0 = treat as a zbot, 1 = detect, 2 = allow (also sets `proxy_nitro2` to 2). |
| `proxy_nitro2` | number | 1 | Nitro2/Xania proxy handling. Same values as `proxy_bwproxy`. |
| `q2a_command_check` | bool | no | Legacy client command-queue check. Not currently sent. |
| `q2adminrunmode` | number | 100 | 100 = fully active; 0 = pass everything straight to the mod. **cfg only.** |
| `quake2dirsupport` | bool | yes | Accepted but currently has no effect. **cfg only.** |
| `randomwaitreporttime` | number | 55 | Range of the random delay used when `zbotdetectactivetimeout` is -1. |
| `rcon_insecure` | bool | yes | Run lrcon commands from the client so the player sees the output. `no` runs them on the server, where the output isn't sent to the player. **cfg only.** |
| `rcon_random_password` | bool | yes | Randomize the real rcon password while lrcon commands run. **cfg only.** |
| `reconnect_address` | string | empty | Address that players are made to reconnect to. Empty disables the reconnect check. |
| `reconnect_checklevel` | number | 0 | How a returning player is recognized. Both levels require the same IP address. 0 = the whole userinfo must match, apart from the connection details the engine adds; 1 = only the name and skin must match. |
| `reconnect_time` | number | 60 | Seconds a player has to reconnect. |
| `say_group_enable` | bool | no | Allow players to use `say_group`. |
| `say_person_enable` | bool | no | Allow players to use `say_person`. |
| `serverinfoenable` | bool | yes | Show q2admin's version in the server info that server browsers display. |
| `serverip` | string | empty | The server's public IPv4 address, used by the proxy checks. |
| `setmotd` | string | empty | Message of the day file, relative to the Quake 2 directory. |
| `signal_score_threshold` | number | 50 | Signal score at which a player is removed. 0 disables removal. |
| `signal_weight` | string | (built-in) | `"<signal> <weight>"`. Overrides one signal's weight; use one line per signal. |
| `skinchangefloodprotect` | triple | disable | Skin change flood limit. |
| `skinchangefloodprotectmsg` | string | `%s changed skin too many times.` | Broadcast when the skin change limit is hit. |
| `skincrashmsg` | string | `%s tried to crash the server.` | Broadcast when someone uses a server-crashing skin. |
| `snapfire_enable` | bool | yes | Enable snap-fire aim detection. |
| `snapfire_min_snap_deg` | number | 30 | Smallest one-frame view change, in degrees, that counts as a snap. |
| `snapfire_off_crosshair_deg` | number | 40 | How far off the crosshair, in degrees, the target must have been before the snap. |
| `soloadlazy` | bool | no | Linux only: load the mod with lazy symbol binding, as a workaround for mods that won't load otherwise. |
| `spawnentities_enable` | bool | no | Enable entity disabling (`q2a_spawn.cfg`). |
| `spawnentities_internal_enable` | bool | no | Also intercept entities the mod spawns itself after the map loads. Needs a correct `entity_classname_offset`. |
| `speedbot_check_type` | number | 3 | Bit 2 announces when a speed-limited player is unfrozen. |
| `startup_attempts` | number | 3 | Times the startup handshake is sent, 5 seconds apart, before a client that hasn't answered gets the zbot verdict. Genuine clients answer the first one. Values below 1 count as 1. |
| `swap_attack_use` | bool | no | Swap `+attack` and `+use`. This breaks auto-aim proxies but confuses players. |
| `timers_active` | bool | no | Enable player timers (`timer_start` and `timer_stop`). |
| `timers_max_seconds` | number | 180 | Longest timer allowed. |
| `timers_min_seconds` | number | 10 | Shortest timer allowed. |
| `timescaledetect` | bool | yes | Detect timescale speed cheats. |
| `timescaleuserdisplay` | string | `%s is using a speed cheat.` | Broadcast when a timescale cheat is detected. |
| `track_enable` | bool | yes | Enable sustained-tracking aim detection. |
| `track_min_motion_deg` | number | 15 | How far, in degrees, the target must move during a tight tracking streak for it to count. |
| `track_tight_deg` | number | 4 | Crosshair error, in degrees, that counts as "on target". |
| `userinfo_proxy` | bool | no | Request extra userinfo for mods that don't, and merge it in. **cfg only**, read at startup. |
| `userinfochange_count` | number | 40 | Userinfo changes allowed within `userinfochange_time` before kicking. |
| `userinfochange_time` | number | 60 | Length in seconds of the userinfo flood window. |
| `versionbanning_enable` | bool | yes | Enable client-version ban rules. |
| `vote_enable` | bool | no | Enable voting. |
| `voteclientmaxvotes` | number | 0 | Votes one player may propose within `voteclientmaxvotetimeout`. 0 means unlimited. |
| `voteclientmaxvotetimeout` | number | 0 | Window in seconds for `voteclientmaxvotes`. 0 means the whole map. |
| `votecountnovotes` | bool | yes | `yes`: votes pass on yes ÷ all players. `no`: players who didn't vote are left out of the total. |
| `voteminclients` | number | 0 | Players needed before a vote can be proposed. |
| `votepasspercent` | number | 50 | Percentage of yes votes needed to pass. |
| `vpn_api_key` | string | empty | vpnapi.io API key. |
| `vpn_enable` | bool | no | Run the vpnapi.io VPN check on connect. |
| `whois_active` | number | 0 | Number of whois records to keep. 0 disables whois. **cfg only.** |
| `zbc_enable` | bool | yes | Enable aim jitter detection. |
| `zbc_jittermax` | number | 4 | Jitter hits within `zbc_jittertime` before a player counts as a bot user. |
| `zbc_jittermove` | number | 500 | View change between frames that counts as a jitter hit. |
| `zbc_jittertime` | number | 10 | Length in seconds of the jitter window. |
| `zbotdetect` | bool | yes | Enable proxy and bot detection. |
| `zbotdetectactivetimeout` | number | 0 | Seconds to wait before acting on a detection. -1 picks a random delay of 5 to 5 + `randomwaitreporttime` seconds. |
| `zbotuserdisplay` | string | `%s is using a client side proxy.` | Broadcast when a proxy user is detected. |


## Commands

Commands are run from the server console as `sv !<command>`, or in-game by an
authenticated admin as `!<command>`. Commands marked **console only** can't be
used in-game. A player argument accepts any
[player spec](#choosing-players).

### Players

#### `kick`
Disconnect one or more players.
`sv !kick <players>`
```
sv !kick CL 4
sv !kick claire
sv !kick CL 3 + 5 + 7
```

#### `mute`
Mute a player for a number of seconds, or until they disconnect with `PERM`.
Running it on a player who is already muted unmutes them.
`sv !mute <player> [seconds | PERM]`
```
sv !mute CL 3 30
sv !mute claire PERM
sv !mute claire          ; unmute
```

#### `stifle` / `unstifle`
Make a player wait the given number of seconds after each chat line before
they can say another, or lift the limit. A stifle lasts until it's lifted or
the player disconnects.
`sv !stifle <player> <seconds>` · `sv !unstifle <player>`
```
sv !stifle CL 0 120
sv !unstifle CL 0
```

#### `freeze` / `unfreeze` (console only)
Pin a player in place, optionally for a number of seconds. They can still look
around, shoot and talk.
`sv !freeze <player> [seconds]` · `sv !unfreeze <player>`
```
sv !freeze claire 30
sv !unfreeze claire
```

#### `stuff`
Run commands in a player's console as if they had typed them, either directly
or from a file in the mod directory.
`sv !stuff <player> "<commands>" | FILE <filename>`
```
sv !stuff claire "name newname"
sv !stuff CL 4 FILE "stuffcmds.txt"
```

#### `ip`
Show a player's IP address.
`sv !ip <player>`
```
sv !ip claire
```

#### `signals`
Show a player's current signals and score.
`sv !signals <player>`
```
sv !signals CL 2
```

#### `signaladd`
Add to, or subtract from, a player's signal score by hand. Repeated uses add
up. Pushing a player over the threshold removes them.
`sv !signaladd <player> <+/-amount>`
```
sv !signaladd claire 25
sv !signaladd claire -25
```

#### `chat_stats` (console only)
Show a player's chat statistics: words, distance moved, words per mile, words
per shot, characters per second and recent messages. These help tell a chat
bot from a talkative player.
`sv !chat_stats <player>`
```
sv !chat_stats claire
```

#### `vpnusers` (console only)
List players connected through a VPN.
```
sv !vpnusers
```

#### `testplayer` (console only)
Show which player a player spec resolves to. This is useful for testing
patterns before using them in other commands.
`sv !testplayer <player>`
```
sv !testplayer RE "^cl"
```

### Messaging (console only)

#### `say_person`
Send a private chat message to one player.
`sv !say_person <player> <message>`
```
sv !say_person CL 2 "please stop camping the quad"
```

#### `say_person_low`
Send a message to one player as a low-priority console print, with no chat
sound or highlighting.
`sv !say_person_low <player> <message>`
```
sv !say_person_low CL 3 "Server restarts in 5 minutes"
```

#### `say_group`
Send a private chat message to several players.
`sv !say_group <players> <message>`
```
sv !say_group CL 0 + 4 + 5 "team meeting after this map"
```

### Bans

#### `ban`
Add a ban rule. The syntax is the same as `BAN:` lines in `q2a_ban.cfg`.

```
sv !ban [+|-] [ALL | NAME [LIKE|RE] <name> | NAME %p <slot> | NAME BLANK]
        [IP VPN | IP <address>[/<prefix>] | IP %p <slot>]
        [ASN as<number>] [VERSION [LIKE|RE] <text>]
        [PASSWORD <pw>] [MAX <n>] [SCORE <n>]
        [FLOOD <count> <seconds> <silence>] [MSG <text>]
        [TIME <minutes>] [SAVE [MOD]] [NOCHECK]
```

The parameters are:

- `+` makes an inclusive (allow) rule; `-`, the default, makes an exclusive
  (deny) rule.
- `%p <slot>` uses the name or IP of the player in that slot.
- `MAX` limits how many matching players can be connected at once.
- `SCORE` adds a signal score to matching players. It only applies to `+`
  rules.
- `TIME` makes the rule expire after that many minutes.
- `SAVE` writes the rule to the ban file; `SAVE MOD` writes it to the copy in
  the mod directory.
- `NOCHECK` skips checking the new rule against players already connected.

Examples:
```
sv !ban NAME LIKE "badword" MSG "not allowed"
sv !ban NAME RE "^[Cc][Ll][Aa4][Ii1][Rr][Ee3]$" MSG "go away"
sv !ban IP 192.0.2.0/24 TIME 60
sv !ban IP 2001:db8:c0ff:ee::/64
sv !ban IP %p 5 SAVE
sv !ban IP VPN MSG "VPNs aren't allowed"
sv !ban ASN as7843 MSG "this ISP is banned"
sv !ban VERSION RE "^q2pro.*r1908"
sv !ban + NAME "claire" PASSWORD "secret"
sv !ban + IP 10.11.12.0/24 SCORE -50
```

#### `listbans` / `delban`
List ban rules with their numbers, or delete one by number.
```
sv !listbans
sv !delban 4
```

#### `chatban`
Add a chat-ban rule. Matching is "contains" (`LIKE`, the default) or a regular
expression (`RE`).
`sv !chatban [LIKE|RE] <text> [MSG <text>] [SAVE [MOD]]`
```
sv !chatban "badword"
sv !chatban RE "f[a4]rt" MSG "potty talk isn't allowed"
```

#### `listchatbans` / `delchatban`
List chat-ban rules, or delete one by number.
```
sv !listchatbans
sv !delchatban 2
```

#### `reloadbanfile`
Re-read every ban file, including included and remote ones, without
restarting. Includes are followed recursively, so a file that includes itself
loops forever.
```
sv !reloadbanfile
```

### Flood control

#### `chatfloodprotect`, `namechangefloodprotect`, `skinchangefloodprotect`
Set the server-wide chat, name change or skin change flood limit. A silence of
0 kicks the player.
`sv !chatfloodprotect <count> <seconds> <silence> | disable`
```
sv !chatfloodprotect 10 5 30
sv !namechangefloodprotect 5 30 300
sv !skinchangefloodprotect 3 10 300
sv !chatfloodprotect disable
```

#### `clientchatfloodprotect`
Override the chat flood limit for one player.
`sv !clientchatfloodprotect <player> <count> <seconds> <silence> | disable`
```
sv !clientchatfloodprotect CL 3 20 7 60
sv !clientchatfloodprotect claire disable
```

#### `connectfloodprotect`
Show or set the connect flood limit: the connection that brings one IP address
to *count* connections within *seconds* trips it, and that address is refused
for *cooldown* seconds (negative means until restart). See
[Connect flood protection](#connect-flood-protection).
`sv !connectfloodprotect [<count> <seconds> <cooldown> | disable]`
```
sv !connectfloodprotect
sv !connectfloodprotect 4 60 600
sv !connectfloodprotect disable
```

#### `floodcmd` / `listfloods` / `flooddel` / `reloadfloodfile`
Add a command to the flood list (only the first word of a command is
matched), list the entries, delete one by number, or reload the file. Don't
add chat commands such as `say` or `say_team`; chat is already covered.
`sv !floodcmd EX|SW|RE "<command>"`
```
sv !floodcmd SW "play_"
sv !listfloods
sv !flooddel 2
sv !reloadfloodfile
```

### Client variables

#### `checkvarcmd`
Add a client variable rule: `CT` for an exact value, `RG` for a numeric range.
`sv !checkvarcmd CT "<cvar>" "<value>"` · `sv !checkvarcmd RG "<cvar>" "<low>" "<high>"`
```
sv !checkvarcmd CT gl_modulate 1
sv !checkvarcmd RG cl_maxpackets 30 100
```

#### `listcheckvar` / `checkvardel` / `reloadcheckvarfile`
List the rules, delete one by number, or reload `q2a_cvar.cfg`.
```
sv !listcheckvar
sv !checkvardel 2
sv !reloadcheckvarfile
```

### Command disabling

#### `disablecmd` / `listdisable` / `disabledel` / `reloaddisablefile`
Disable a mod command, list the disabled commands, re-enable one by number, or
reload `q2a_disable.cfg`.
`sv !disablecmd EX|SW|RE "<command>"`
```
sv !disablecmd EX "kill"
sv !disablecmd SW "say_"
sv !listdisable
sv !disabledel 5
sv !reloaddisablefile
```

### Limited rcon

#### `lrcon` / `listlrcons` / `lrcondel` / `reloadlrconfile`
Add a limited-rcon rule (the whole command, including its arguments, is
matched), list the rules, delete one by number, or reload `q2a_lrcon.cfg`.
`sv !lrcon EX|SW|RE "<password>" "<command>"`
```
sv !lrcon RE "refpass" "^map q2dm[1-8]$"
sv !lrcon SW "mapper" "map "
sv !lrcon EX "watcher" "status"
sv !listlrcons
sv !lrcondel 4
sv !reloadlrconfile
```

#### `resetrcon`
Restore the real rcon password immediately, instead of waiting for
`lrcon_timeout`.
```
sv !resetrcon
```

### Voting

#### `votecmd` / `listvotes` / `votedel` / `reloadvotefile`
Allow a command to be voted on, list the allowed commands, remove one by
number, or reload `q2a_vote.cfg`.
`sv !votecmd EX|SW|RE "<command>"`
```
sv !votecmd RE "^map q2dm[1-8]$"
sv !votecmd SW "fraglimit "
sv !listvotes
sv !votedel 5
sv !reloadvotefile
```

### Entities

#### `spawncmd` / `listspawns` / `spawndel` / `reloadspawnfile`
Stop an entity class from spawning (from the next map on), list the rules,
delete one by number, or reload `q2a_spawn.cfg`. Changes made here last until
the spawn file is next loaded.
`sv !spawncmd EX|SW|RE "<classname>"`
```
sv !spawncmd EX "weapon_bfg"
sv !spawncmd SW "item_health"
sv !listspawns
sv !spawndel 4
sv !reloadspawnfile
```

### Signals

#### `signal_weight`
Show or change how much a signal adds to a player's score.
`sv !signal_weight <signal> [weight]`
```
sv !signal_weight snap-fire
sv !signal_weight snap-fire 25
```

`signals` and `signaladd` are under [Players](#players).

### Logging

#### `logfile`
View, change or delete log file definitions (numbers 1–32). The optional `mod`
keyword places the file in the mod directory.
`sv !logfile view [n]` · `sv !logfile edit <n> [mod] <filename>` · `sv !logfile del <n>`
```
sv !logfile view
sv !logfile edit 2 mod "chat%p.log"
sv !logfile del 3
```

#### `logevent`
View or change which log files an event type goes to, and its format.
`sv !logevent view <type>` · `sv !logevent edit <type> [log yes|no] [logfiles <n>[+<n>...]] [format "<format>"]`
```
sv !logevent view chat
sv !logevent edit chatban log yes logfiles 2 format "#T #n #m"
sv !logevent edit chatban log no
```

#### `clearlogfile`
Empty a log file.
`sv !clearlogfile <n>`
```
sv !clearlogfile 3
```

#### `displaylogfile`
Print a whole log file to the console, one line per server frame. On a large
log this takes a long time and can't be stopped.
`sv !displaylogfile <n>`
```
sv !displaylogfile 1
```

#### `flush_logs` (console only)
Write any buffered log data to disk.
```
sv !flush_logs
```

### Server

#### `lock`
Show the lock state, or lock and unlock the server to new players.
`sv !lock [yes|no]`
```
sv !lock yes
```

#### `cvarset`
Set a server cvar. A value of `none` clears it.
`sv !cvarset <cvar> <value>`
```
sv !cvarset hostname "My Server"
sv !cvarset motd_extra none
```

#### `cloud` (console only)
Show the Cloud Admin connection's status, or connect, disconnect or
reconnect.
`sv !cloud status | connect | disconnect | reconnect`
```
sv !cloud status
sv !cloud reconnect
```

#### `reloadloginfile`
Reload `q2a_login.cfg` and `q2a_bypass.cfg`.
```
sv !reloadloginfile
```

#### `reloadwhoisfile`
Reload the whois database from `whois.dat`.
```
sv !reloadwhoisfile
```

#### `reloadhashlist`
Download the remote anti-cheat cvar, hash and token lists again.
```
sv !reloadhashlist
```

#### `reloadanticheatlist` / `reloadexceptionlist`
Reload the anti-cheat exception list. The two names are aliases.
```
sv !reloadanticheatlist
```

#### `version`
Show q2admin's version.
```
sv !version
```

#### Showing or changing options
Every option in the [options table](#options) (except those marked cfg only)
can be typed as a command to show it, or with a value to change it:
```
sv !vpn_enable
sv !vpn_enable no
sv !signal_score_threshold 80
```

### Player commands

Any player can use these. Some need the feature to be enabled first.

| Command | Needs | Description |
| --- | --- | --- |
| `vote <command>` | `vote_enable` | Propose a vote on an allowed command. The command name is set by `clientvotecommand`. Example: `vote map q2dm1` |
| `vote yes` / `vote no` | `vote_enable` | Answer the vote in progress. |
| `say_person <player> <msg>` | `say_person_enable` | Send a private message, for example `say_person claire hi`. |
| `say_group <players> <msg>` | `say_group_enable` | Send a message to several players, for example `say_group CL 1 + 2 regroup`. |
| `say !p <player> <msg>` / `say !g <players> <msg>` | `extendedsay_enable` | The same, using normal `say`. |
| `lrcon <password> <command>` | rules in `q2a_lrcon.cfg` | Run an allowed server command, for example `lrcon refpass map q2dm2`. |
| `whois <player>` | `whois_active` | Show a player's known aliases. |
| `timer_start <n> <seconds> "<command>"` | `timers_active` | Run a command of your own after a delay, for example `timer_start 1 60 "say quad soon\n"`. |
| `timer_stop <n>` | `timers_active` | Cancel a timer. |
| `showfps` | — | Toggle the on-screen frame rate display. |
| `motd` | `setmotd` | Show the message of the day again. |
| `!version` | — | Show the q2admin version. |
| `!setadmin <password>` | `adminpassword` | Log in as an admin with the shared password. |
| `!admin <name> <password>` | `q2a_login.cfg` | Log in to an admin account. |
| `!bypass <name> <password>` | `q2a_bypass.cfg` | Log in to a bypass account. |
| `!teleport <server>` / `!invite <player>` | Cloud Admin | Move between servers, or invite a player from another server. The command names are configurable. |

Players logged in to an admin account also get the level-based commands
listed under [Admin access](#admin-access): `!boot`, `!dumpmsec`,
`!changemap`, `!dumpuser`, `!dumpuser_any`, `!auth`, `!gfx`, `!dostuff` and
`!writewhois`.


## Compiling

Building needs `make`, a C compiler and git, because the version number is
taken from the git history. Build settings can be changed on the command line
(`make SHARED_DEPS=1`) or saved in a file named `.config` next to the
Makefile.

| Setting | Effect |
| --- | --- |
| `CPU` | Target architecture, used for the output name, the build directory and to pick the bundled libraries. Detected with `uname -m` by default. |
| `SHARED_DEPS=1` | Link against the system's shared libraries instead of the bundled ones (see [Dependencies](#dependencies)). |
| `CONFIG_WINDOWS=1` | Cross-compile a 32-bit Windows DLL with MinGW. |
| `CONFIG_MACOS=1` | Add the macOS system frameworks needed at link time. |
| `CC`, `CFLAGS`, `LDFLAGS`, `LIBS` | Standard toolchain overrides. |
| `BUILDDIR` | Where build output goes. Defaults to `build-<cpu>`. |
| `V=1` | Show the full compiler commands. |

The source code is in [`src/`](src/). Everything the build produces (object
files, dependency files and the finished library) goes in a separate
directory for each architecture: `build-<cpu>`, for example `build-x86_64`
on 64-bit Linux or `build-x86` for the Windows DLL. Builds for different
architectures don't mix, and the source tree stays clean.

Useful targets:

- `make` builds the library.
- `make strip` strips debug symbols from it.
- `make clean` deletes the build directory for the current architecture.
- `make genkeys` builds `build-<cpu>/genkeys`, a small tool that generates
  Cloud Admin keys.

The library is named `game<cpu>-q2admin-r<revision>~<commit>.<ext>`, for
example `build-x86_64/gamex86_64-q2admin-r1211~0633d58.so`. Rename it, or set
`gamelibrary`, when you install it.

### Linux

```sh
git clone <repository-url> q2admin
cd q2admin
make
make strip      # optional, makes the file smaller
```

On `x86_64` and `arm64`, every library is bundled, so only a compiler is
needed. The `i386` and `arm` bundles don't include SQLite. On those
architectures, either add `deps/<cpu>/sqlite/` yourself or build with
`SHARED_DEPS=1`.

For a 32-bit build on a 64-bit host, install your distribution's 32-bit
(multilib) toolchain and put this in `.config`:

```make
CPU = i386
CFLAGS = -m32
LDFLAGS = -shared -m32
```

Put compiler flags in `.config` rather than passing `CFLAGS=...` on the
`make` command line. Command-line values replace the flags the Makefile adds
itself, instead of adding to them.

### Windows

Windows builds are cross-compiled with MinGW-w64, either from Linux (for
example the `mingw-w64` package on Debian and Ubuntu) or from MSYS2 on
Windows. The output is a 32-bit `gamex86.dll`, which matches 32-bit Windows
Quake 2 servers. The libraries it needs are bundled in `deps/win32`.

```sh
make CONFIG_WINDOWS=1
```

or put `CONFIG_WINDOWS=1` in `.config` and run `make`. The
[`.config-win32mingw`](.config-win32mingw) file is an example of a fuller
Windows `.config` that links against your own MinGW libraries instead of the
bundled ones.

### macOS

The repository doesn't include libraries built for macOS, so install them
with [Homebrew](https://brew.sh) and build against them:

```sh
brew install curl openssl@3 zlib sqlite
```

Then create a `.config` like this one, adjusting the paths if
`brew --prefix` isn't `/opt/homebrew` on your machine:

```make
CONFIG_MACOS = 1
SHARED_DEPS = 1
CFLAGS = -I/opt/homebrew/include -I/opt/homebrew/opt/openssl@3/include
LDFLAGS = -shared -L/opt/homebrew/lib -L/opt/homebrew/opt/openssl@3/lib
```

and run `make`. The output still ends in `.so`. Rename it to whatever your
Quake 2 server expects, or add `EXT = dylib` to `.config`.

### Example configs

The [`runtime-config/`](runtime-config/) folder holds a commented example of
every config file q2admin reads (`q2admin.cfg`, `q2a_ban.cfg`, `q2a_log.cfg`
and the others). Copy them into your mod directory as a starting point; every
option is explained in the files themselves and in the
[Configuration reference](#configuration-reference).


## Dependencies

| Library | Used for |
| --- | --- |
| **libcurl** | HTTP(S) downloads: VPN lookups (vpnapi.io and IPLogs), remote ban lists, `INCLUDE:` URLs and the anti-cheat hash lists. |
| **OpenSSL** (libssl, libcrypto) | HTTPS for libcurl, plus RSA authentication and AES encryption for the Cloud Admin connection. |
| **zlib** | Compression support needed by libcurl and OpenSSL. |
| **SQLite** | Reserved for local IP-context (VPN/ASN) lookups. It's linked in, but nothing in the code calls it yet. |
| **pthreads** | Background DNS lookups for Cloud Admin, so a slow lookup never stalls the server. |
| **libdl** | Loading the real mod library at runtime (Linux and macOS). |

**All dependencies are pre-built and included in the repository** under
[`deps/`](deps/), one folder per architecture (`x86_64`, `i386`, `arm64`,
`arm`, `win32`). Each folder's `versions` file lists the exact library
versions. The `i386` and `arm` folders don't include SQLite yet. By default they're **statically linked** into the q2admin library,
so the server you install it on doesn't need any of them installed. It works
on minimal containers and older distributions alike.

The trade-off is size: a default 64-bit Linux build is about 8 MB (about
6.5 MB after `make strip`), mostly OpenSSL.

To link against the shared libraries installed on the build machine instead,
set `SHARED_DEPS=1`:

```sh
make SHARED_DEPS=1
```

or add `SHARED_DEPS=1` to `.config`. This makes the library much smaller:
about 1.3 MB, or about 470 KB stripped. The build machine then needs the
development packages (on Debian and Ubuntu: `libcurl4-openssl-dev`,
`libssl-dev`, `zlib1g-dev` and `libsqlite3-dev`), and every server you deploy
to needs the matching runtime libraries installed. This is worth doing if you
run many servers on one machine and want them to share one copy of the
libraries in memory.
