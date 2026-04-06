# Q2Admin

Q2Admin is a management and security addon for Quake 2 servers. It acts as a proxy game module; the Quake 2 server loads q2admin as its game library and q2admin will open the actual game library and proxy between the two. This allows q2admin to inspect and alter much of the data between client and server on the fly. 


## Features

* Detect and handle cheaters (old ratbots and proxies)
  * Aim bots (ratbot, zbot)
  * Proxies
* IPv6 Support (requires v6-aware server AND mod)
* Chat filtering
* Name filtering
* Skin filtering
* Client-side variables management
  * Disabling entirely
  * Forcing an exact value
  * Clamping to range
* Useful commands added
  * For management (ex: `stuff`, etc)
  * For player experience (ex: `say_person`)
* Out-of-band management via webapp/SSH *still under development*
  * View status and logs of all your Q2 servers
  * Issue commands to groups of servers
  * Delegate control of servers to other admin users
  * Centralized logging
  * Centralized banning
  * Custom maplists
* Very flexible logging
  *  Supports up to 32 different log files
  *  23 different log contexts
  *  Per-log customizable message formatting
* Custom banning
  * By IP address (including IPv6)
  * By VPN status
  * By ASN (entire ISPs)
  * By client version
  * Inclusion/exclusion
  * Password protect any attribute
    * password needed for certain names
    * password needed to connect from certain IP
    * password needed to use certain client software
* Command disabling
* Flood control
  * Chat spam
  * Name-change spam
  * Skin-change spam
  * Custom command spam
* Limited RCON support
  * Password only allows specific commands
  * Least-privilege 
* Entity disabling by class (`weapon_bfg`, `item_quad`, `trigger_hurt`, etc...)
* Map entity swapping
  * Ex: change powershields to powerscreens
* Custom voting
  * Allow voting for any command with args
  * Adds voting to unsupported mods (Rocket Arena 2)
* VPN detection
* Very flexible string handling
  * Exact matches (x == y)
  * Starting with... (x at the beginning of y
  * Like... (x contained in y)
  * Globbing (`q2dm*`)
  * Full regular expression support (`^q2dm[1-8]$`)


## Compiling for Linux

Simply run `make` in the root folder 


## Compiling for Windows using MinGW

Edit `.config-win32mingw` file to suit your environment and rename it to `.config`. Then run `make` to build your DLL.


## Configuration

* Copy all the .cfg files from the `runtime-config` directory to your mod directory.
* Edit `q2admin.cfg` to suit your needs.
* Inform q2admin what the actual game library is.
  * Either set the `gamelibrary` option in `q2admin.cfg` (**recommended**)
  * Or specifically name the real game library: `game{arch}.real.{ext}`
    * Linux libraries have a `.so` extension while Windows uses `.dll`
    * For 32-bit systems the architechture should be `x86`
      * Older R1Q2 and 3.2[01] servers will probably look for `i386` for arch
    * For 64-bit systems, the arch will be `x86_64`
    * Examples:
      * 64-bit Linux:   `gamex86_64.real.so`
      * 32-bit Windows: `gamex86.real.dll`
      * Ancient Linux:  `gamei386.real.so`
    * You cannot mix architectures. The Quake 2 server, q2admin, and the mod all need to either by compiled for 32 or 64 bit. Unfortunately many mods are only available as compiled binaries, the source code is not available or long lost. So depending on what mod you want to run, that will almost certainly dictate what architecture you'll need to run. 
  * Or use `+set gamelib <libraryname>` starting your Quake 2 server.
* Edit each of the `q2a_*.cfg` files to setup the features you need. 

 
## Dependencies

1. `libcurl`
2. `libssl`
3. `libcrypto`
4. `zlib`
5. `libpthread`

All dependencies are included and statically linked. The resulting binary is about 8MB, most of which is related to libcrypto. This is unfortunately required for the Cloud Admin connection (authentication and transit encryption).  


## Settings

### Server CVARs

Set in your server config or at the command line using `+set {cvar} {value}`

CVAR | Purpose | Default
--- | --- | ----
`gamelib` | Specify the real game library for q2admin to load | ""
`q2aconfig` | Specify the main config filename | "q2admin.cfg"
`configfile_ban` | Config file for bans | "q2a_ban.cfg"
`configfile_bypass` | Config file for bypass access | "q2a_bypass.cfg"
`configfile_cloud` | Cloud admin config | "q2a_cloud.cfg"
`configfile_cvar` | CVAR banning config | "q2a_cvar.cfg"
`configfile_disable` | Command disabling config | "q2a_disable.cfg"
`configfile_flood` | Flood config | "q2a_flood.cfg"
`configfile_login` | Admin definitions | "q2a_login.cfg"
`configfile_log` | Logging setup | "q2a_log.cfg"
`configfile_rcon` | LRCON setup | "q2a_lrcon.cfg"
`configfile_spawn` | Entity disabling config | "q2a_spawn.cfg"
`configfile_vote` | Voting setup | "q2a_vote.cfg"


### Config Options
Set these options in the main q2admin config files
Option | Type | Default | What it does
--- | --- | --- | ---
`adminpassword` | string | "" | A password to use to auth as an admin
`banonconnect` | bool | yes | Banning happens in `ClientConnect` instead of `ClientBegin`
`chatbanning_enable` | bool | yes | Filter chat messages based on config
`chatfloodprotectmsg` | string | "" | Msg to send when flooding is triggered
`checkclientipaddress` | bool | yes | Do various checks based on a player's IP
`checkvar_poll_time` | number | 60 | How many seconds between CVAR checks?
`checkvarcmds_enable` | bool | yes | Enable CVAR checking
`clientremindtimeout` | number | 10 | Remind clients of an active vote every this many seconds
`clientsidetimeout` | number | 30 | Delay in seconds for ratbot and proxy detection. Clamped to a minimum of 5 seconds
`clientvotecommand` | string | "qvote" | The command players will use to cast and propose votes
`clientvotetimeout` | number | 60 | The number of seconds a vote proposal valid for
`client_map_cfg` | number | 6 | Bitmask controlling what config files get automatically stuffed to clients <br><br>set map_name [map] = 1<br>exec [mapname].cfg = 2 <br> exec all.cfg = 4
`cloud_flags` | number | 4095 | Bitmask for what cloud admin features are enabled. Add them up:<br>1 = frag accounting<br>2 = log chat<br>4 = support teleporting <br>8 = support inviting <br>16 = enable finding players <br>32 = enable whois <br>1024 = show debug info 
`cloud_cmd_teleport` | string | "!teleport" | Command to teleport between servers
`cloud_cmd_invite` | string | "!invite" | Command to invite players from other servers
`cloud_cmd_seen` | string | "!seen" | Command to find when a player was last seen
`cloud_cmd_whois` | string | "!whois" | Command to lookup a player's other aliases
`cloud_dns` | string | "64" | Preference order for IPs looked up from DNS, IPv6 vs IPv4
`cloud_enabled` | bool | no | Whether Q2 server will attempt to connect to a Cloud Admin server
`cloud_address` | string | "[::1]" | The IP address or DNS name of the Cloud Admin server
`cloud_port` | number | 9988 | TCP port for the Cloud Admin server
`cloud_encryption` | bool | yes | Encrypt the traffic between the Cloud Admin server and the q2 server
`cloud_privatekey` | string | "private.pem" | The q2 server's private key
`cloud_publickey` | string | "public.pem" | The q2 server's public key, this get shared with the Cloud Admin server
`cloud_serverkey` | string | "server.pem" | The Cloud Admin server's public key
`cloud_uuid` | string | "" | A unique identifier shared between the q2 server and the Cloud Admin server
`cl_anglespeedkey_display` | bool | yes | Broadcast to all players when someone is messing with their cl_anglespeedkey cvar
`cl_anglespeedkey_enable` | bool | no | Enable cl_anglespeedkey monitoring
`cl_anglespeedkey_kick` | bool | no | Kick players caught manipulating this cvar
`cl_anglespeedkey_kickmsg` | string | "cl_anglespeedkey changes not allowed ont his server" | The message to send the offending player when they're kicked
`cl_pitchspeed_display` | bool | yes | Broadcast to all players when someone is messing with their cl_pitchspeed cvar
`cl_pitchspeed_enable` | bool | no | Enable cl_pitchspeed monitoring
`cl_pitchspeed_kick` | bool | no | Kick players caught manipulating this cvar
`cl_pitchspeed_kickmsg` | string | "cl_pitchspeed changes not allowed on this server" | The message to send the offending player when they're kicked
`consolechat_disable` | bool | no | Disable sending player chat to the server console
`consolelog_enable` | bool | no | Enable logging some events to the server console
`consolelog_pattern` | string | "[q2a] %s\n" | The printf-like format for events logged to server console
`customclientcmd` | string | "" | Command to stuff to players when they're detected as a cheat prior to disconnecting them
`customclientcmdconnect` | string | "" | Command to stuff to players when they connect
`customservercmd` | string | "" | Command to run on the server when a client is detected as a cheat. The `%c` placeholder is substituted with the offending client's ID number.
`customservercmdconnect` | string | "" | Command to run on the server when a player connects
`defaultbanmsg` | string | "" | Message players will see by default when they are not allowed to connect
`defaultchatbanmsg` | string | "" | Message players will see by default when they say something that isn't allowed
`defaultreconnectmessage` | string | "" | Message players will see when they are forced to reconnect on connect
`disablecmds_enable` | bool | no | Enable checking for disabled commands 
`disconnectuser` | bool | yes | Kick a player if it's discovered they're cheating
`disconnectuserimpulse` | bool | no | Kick a player if they use certain impulses
`displayimpulses` | bool | no | Broadcast the fact that a player just used a certain impulse
`displaynamechange` | bool | yes | Broadcast when someone changes their name
`dopversion` | bool | yes | Do a p_verion probe for proxies when a player connects
`do_franck_check` | bool | yes | Check for franck when a player connects
`do_vid_restart` | bool | no | Force client to do a `vid_restart` command when they connect. This can unload some wallhacks
`enforce_deadlines` | bool | yes | When asking the player's client for certain information, set a reasonable deadline for a response and kick the player if no response is provided (indicates a modified client)
`entity_classname_offset` | number | 280 | What byte offset can the `classname` property be found in the `edict_s` struct? Since most of the edict_s struct is opaque and filled in by the game library, it's not possible for q2admin to know where that value is located. This value can be different for each game mod. Using an incorrect value here can lead to crashes, especially if you're doing entity substitution/blocking.<br><br>Common values:<br>baseq2 = 280<br>opentdm = xxxxxx
`extendedsay_enable` | bool | no | Add to regular `say` command for targeting specific players or groups. Syntax:<br>`say !p PLAYERSPEC msg`<br>`say !g PLAYERSPECMULTI msg`
`filternonprintabletext` | bool | no | Strip console characters (ascii values in range 128-256) from chat messages
`fpsfloodexempt` | bool | no | Consider excessive `cl_maxfps` changes as flooding. This is common in the Jump mod
`framesperprocess` | number | 0 | The number of server frames to process for every q2admin frame. 0 = process every frame. It's currently unclear how this affects things like msec checking, better to leave this value at 0.
`gamemaptomap` | bool | no | Convert any usage of `gamemap` command to `map`. This will cause all new maps to reload the game library and use significant resources. *Don't use this*. Use `sv_recycle` as part of q2pro/r1q2.
`gamelibrary` | string | "" | Specifies the real mod library to use. This can be in various ways which have different priorites.<br><br>Setting game via CVAR when server is run will override all, then this option in the config, then using filenames (*gamex86_64.real.so*)
`gl_driver_check` | number | 0 | Checks for GL driver changes and disconnects player if they're over `gl_driver_max_changes`
`gl_driver_max_changes` | number | 3 | Number of times the GL drive can be changed before assuming shenanigans. To catch people toggling a wallhack while playing
`hackuserdisplay` | string | "%s is using a modified client" | The message to broadcast when a cheating player is discovered
`http_cacert_path` | string | "/etc/ssl/certs" | Where do we find the system's certificate authority public keys? Only used if `http_veryifyssl` is enabled for ensuring the https server is who they say they are
`http_debug` | bool | no | Show extra debug info related to CURL usage in the server console
`http_enable` | bool | yes | Enable libcurl for downloading stuff via http(s)<br><br>Required for<br>- Remote ban files<br>- VPN detection<br>- Loading remote anticheat configs<br>- ASN banning
`http_verifyssl` | bool | yes | When downloading a file via https, verify the TLS certificate is signed, valid, trusted and the common name on the cert matches the domain in the URL. Q2admin doesn't download and execute arbitrary files from the internet, so certificate issues are fairly low risk. Try disabling this if you're having trouble fetching https files (especially with https redirects)
`impulsestokickon` | string | "" | A comma-separated list of impulse values that will earn a player a kick. These values are what zbot's use for their onscreen menu. Default is empty, but a good value for this is "`169, 170, 171, 172, 173, 174, 175`"
`inverted_command[1-4]` | string | "" | Private commands stuffed to a player as part of the standard proxy check.
`ip_limit` | number | 0 | The number of players allowed from the same IP address. Exceeding this limit kicks the player.<br><br>0 = unlimited/no filtering
`ip_limit_vpn` | number | 0 | The number of players allowed from the same VPN provider. The ASN number of the provider is used here, so players can be on discontiguous netblocks and still be kicked.<br><br>0 = unlimited/no filtering
`ipbanning_enable` | bool | yes | Enable the functionality of banning players based on their IP address.
`kickonnamechange` | bool | no | If a player successfully joins with a password-protected name, kick them if they change names after connecting.
`lock` | bool | no | lockdown mode, prevent anyone from joining. This is presumablity to allow for some kind of maintenance without player interference.
`lockoutmsg` | string | "This server is currently locked." | The message to display to clients attempting to connect while the server is locked.
`lrcon_timeout` | number | 2 | The seconds from now that an lrcon random password/request is valid for.
`mapcfgexec` | bool | no | On new map (or connect), stuff these commands to player:<br><br>- `exec mapcfg/{oldmapname}-end.cfg`<br>- `exec mapcfg/{newmapname}-pre.cfg`
`maxclientsperframe` | number | 100 | The number of players q2admin can deal with per server frame. This assumes default of 10HZ, *don't touch this unless you know what you're doing*
`maxfps` | number | 0 | The maximum value for `cl_maxfps` cvar allowed on this server. It should be evenly divisible by 1000, 125 is a good value. I've seen 150 used on map `q2duel5` for making some otherwise impossible jumps. Value of 0 means no max.
`maximpulses` | number | 1 | Max impulses before taking action
`maxmsglevel` | number | 3 | Max value allowed for the `msg` userinfo variable.
`maxrate` | number | 0 | Max value allowed for the `rate` userinfo variable. Value of `0` means no limit.
`max_pmod_noreply` | number | 2 | The number of seconds before taking action for unanswered pmod request
`minfps` | number | 0 | The minimum value allowed for the `cl_maxfps` cvar
`msec_action` | number | 2 | What action should be taken if there are msec violations?<br><br>0 = legacy behavior, kick if over limit<br>1 = do nothing<br>2 = announce and kick the player
`msec_max_allowed` | number | 5600 | The maximum cumulatvie msec consumption for `msec_timespan` amount of time. This value should be `(1000 * msec_timespan) * 1.12` -ish. The 1.12 multipier allows for some slop. This is a similar calculation to how q2pro allocates msec to clients but on a shorter timespan.
`msec_max_violations` | number | 2 | How many violations allowed before taking action.
`msec_min_required` | number | 0 | This is the minimum msec consumption required for the `msec_timespan` amount of time. If you really want to get strict, set this to `(1000 * msec_timespan) * 0.88` -ish. Modified clients can underflow their msec consumption in order to bank it and use it later in the same msec_timespan for a speed boost.<br><br>Be aware packet loss can affect this causing a player to be in violation if their move packet is never received, so they could be kicked simply for having poor network performance. Use at your own risk.
`msec_timespan` | number | 5 | Span of seconds to evaluate msec consumption. The smaller the value the more unforgiving to things like packet loss and network jitter. The larger the span the more accurate reading you get, but that delays the ability to take action for that amount of time.
`namechangefloodprotectmsg` | string | ??? | The message the player will see when they change their name too many times
`nickbanning_enable` | bool | yes | Enable the ability to ban based on player name
`numofdisplays` | number | 4 | How many times to in a row to broadcast that a player was caught cheating
`printmessageonplaycmds` | bool | yes | Display when a player uses one of the `play_*` commands.<br><br>This command is useless for the actual `play` command because that command is kept in the client and not sent to the game library anymore with modern clients. Ancient 3.20 clients still send it though.
`private_command[1-4]` | string | "" | Private commands stuff to a player as part of the standard proxy check
`private_command_kick` | bool | no | If responses to the private commands are not received, kick the player.
`proxy_bwproxy` | number | 1 | Check for bwproxies
`proxy_nitro2` | number | 1 | Check for nitro2 proxies
`q2adminrunmode` | number | 100 | 100 = fully operational, 0 = passthru - *don't touch this value*
`q2a_command_check` | bool | false | ????
`randomwaitreporttime` | number | 55 | ????
`rcon_random_password` | bool | yes | Change the actual rcon password to random string and back for lrcon usage
`rcon_insecure` | bool | yes | If no, lrcon commands are executed directly on the server rather than let the client execute it, this means the output is never seen by the client.
`reconnect_address` | string | "" | The IP/hostname to force the client to reconnect to.
`reconnect_checklevel` | number | 0 | How specific to do userinfo checking for reconnecting player. Positive integer means check the keys, 0 means just compare the whole userinfo string (different ordering will cause non-match)
`reconnect_time` | number | 60 | The max time to wait for a reconnect
`say_group_enable` | bool | no | Allow players to send chats to a specific group of other players
`say_person_enable` | bool | yes | Allow players to send chats to another specific player, not everyone
`serverinfoenable` | bool | yes | Add q2admin version to the server info string so it will appear in server browsers
`serverip` | string | "" | Used in proxy checking
`setmotd` | string | "" | The filename containing message-of-the-day data to display when players connect
`skinchangefloodprotectmsg` | string | "" | Message to display to players who change their skin too many times
`skincrashmsg` | string | "" | Message to display to players who attempt to use a special skin designed to crash the server
`soloadlazy` | bool | no | Linux only. Sometimes the forward mod doesn't load properly and crashes. Try setting this to fix this behavior.
`spawnentities_enable` | bool | no | Enable entity-related stuff like swapping or disabling
`spawnentities_internal_enable` | bool | no | Whether q2admin will run it's own `SpawnEntities` function before forwarding to the real game library.
`speedbot_check_type` | number | 3 | Whether or not to display when a speedbot is detected
`swap_attack_use` | bool | no | If attack input is detected, change it to use. Vice-versa
`timers_active` | bool | no | Whether q2admin timer support is enabled
`timers_max_seconds` | number | 180 | The largest timer value allowed
`timers_min_seconds` | number | 10 | The smallest timer value allowed
`timescaledetect` | bool | yes | Monitor players' timescale variable. This is a cheat-locked cvar that would allow players to move faster
`timescaleuserdisplay` | string | ??? | Message to send player if their timescale is greater than 1.0
`userinfochange_count` | number | 40 | Players changing userinfo more than this will result in being kicked
`userinfochange_time` | number | 60 | The timespan for flooding the server with userinfo changes
`versionbanning_enable` | bool | yes | Enable the ability to ban players based on the client version they're using
`voteclientmaxvotes` | number | 0 | Only allow clients to propose this many votes. 0 = unlimited
`voteclientmaxvotetimeout` | number | 0 | ????
`votecountnovotes` | bool | 1 | ???
`voteminclients` | number | 0 | The minimum number of players required to propose a vote. 0 = any
`votepasspercent` | number | 50 | This percent of players need to vote yes for it to pass
`vote_enable` | bool | no | Enable the voting system
`vpn_api_key` | string | "" | The API key for the VPN lookup service
`vpn_enable` | bool | no | Enable VPN checking (requires `http_enable`)
`vpn_kick` | bool | yes | Kick all players connecting from a VPN
`whois_active` | number | 0 | Whether to enable whois tracking
`zbc_enable` | bool | yes | Check clients for aim-assist (includes zbots and ratbots)
`zbc_jittermax` | number | 4 | ???
`zbc_jittermove` | number | 500 | The max jump in view angles between client frames before considering aim is assisted
`zbc_jittertime` | number | 10 | ????
`zbotdetect` | bool | yes | Detect zbots
`zbotdetectactivetimeout` | number | -0 | ????
`zbotuserdisplay` | string | ???? | The message sent to users when they're detected as a zbot


## Commands

### `ban`

Add an entry to the banlist
```
sv !BAN [+/-(-)] [ALL/[NAME [LIKE/RE] name/%%p x/BLANK/ALL(ALL)] [IP VPN/ipv4addr/ipv6addr/%%p x][/yyy(32|128)]] [ASN as###] [VERSION [LIKE/RE] xxx] [PASSWORD xxx] [MAX 0-xxx(0)] [FLOOD xxx(num) xxx(sec) xxx(silence] [MSG xxx] [TIME 1-xxx(mins)] [SAVE [MOD]] [NOCHECK]

Examples:
// don't allow players with names containing nametoban, show them the message "not allowed"
sv !ban - NAME LIKE "nametoban" MSG "not allowed"

// don't allow any player using q2pro r1908
sv !ban - VERSION RE "^q2pro.*r1908.+"

// allow players to use the name claire only if they have the right password.
// passwords are supplied as "pw" in the userinfo
sv !ban + NAME "claire" PASSWORD "superpassword"

// don't allow a specific IPv4 address
sv !ban - IP 192.0.2.3/32

// don't allow players named claire from a specific IPv6 address
sv !ban - NAME RE "^claire$" IP 2001:db8::face:b00b/128 MSG "go away"

// don't allow any VPN address
sv !ban - IP VPN MSG "vpns are not allowed"

// don't allow anyone from Charter Communications [as7843]
sv !ban - ASN as7843 MSG "Charter customers not allowed"  
```

### `chatban`

Add an entry to the chatban list
```
sv !CHATBAN [LIKE/RE(LIKE)] xxx [MSG xxx] [SAVE [MOD]]

Examples:
// don't allow players to say "fart", use the default msg
sv !chatban LIKE "fart" 

// don't allow players to say "fart" and the friend "f4rt"
sv !chatban RE ".*f[a4]rt.*" MSG "potty talk not allowed"
```

### `chatfloodprotect`

Set the params for what constitutes a chat flood:
1. The number of chat messages to trigger
2. The number of seconds those messags are seen in
3. The number of seconds that player will be silenced
```
// sending 10 chats in 5 seconds will result in a 30 second mute
sv !chatfloodprotect 10 5 30
```

### `checkvarcmd`

Manually add an entry to the checkvar list. 

```
Syntax:
sv !checkvarcmd [CT/RG] "variable" ["value" | "lower" "upper"]
```
CT = constant value \
RG = range
```
// force cl_maxpackets to be between 30-100
sv !checkvarcmd RG cl_maxpackets 30 100

// force gl_shadows to 1
sv !checkvarcmd CT gl_shadows 1
```

### `checkvardel`

Delete a checkvar entry from the list. The only arg is the index number of the entry to remove.
```
sv !checkvardel 2
```

### `clearlogfile`

Delete all entries in a particular log file. The only arg is the number of the log file.
```
sv !clearlogfile 3
```

### `clientchatfloodprotect`

Set custom chat flood protection for a specific player
```
sv !clientchatfloodprotect [CL # | name] [num] [secs] [silence]
```
Specify the client either by number using `CL` or directly by name. `Num` is the number of chat messages seen within `secs` seconds to trigger the rule resulting in this player being muted for `silence` seconds.
```
// Silence player #3 for 1 minute if they say 20 chats in 7 seconds
sv !clientchatfloodprotection CL 3 20 7 60

// Silence claire for 30 seconds if they say 10 chats in 5 seconds
sv !clientchatfloodprotection claire 10 5 30
```

### `cloud`

Enable/Disable or check the status of the Cloud Admin server connection
```
// enable connecting to cloud admin server
sv !cloud enable

// disable connecting to CA
sv !cloud disable

// show the connection status
sv !cloud status
```

### `cvarset`

Set (or unset) a CVAR to a specific value on the server
```
// set the variable "foo" to value "bar"
sv !cvarset "foo" "bar"

// unset the variable "foo"
sv !cvarset "foo" none
```

### `delban`

Delete a ban entry from the list. The only arg is the number of the ban entry to remove. Related commands: `ban`, `listbans`
```
// remove ban #4
sv !delban 4
```

### `delchatban`

Delete a chatban entry from the list. The only arg is the number of the ban entry to remove. Related commands: `chatban`, `listchatbans`
```
// remove ban #4
sv !delchatban 4
```

### `disablecmd`

Add an entry to the list of disabled commands. Related commands: `disabledel`
```
sv !disablecmd [SW/EX/RE] "command"
```
SW = starts with \
EX = exact \
RE = regular expression
```
// disable the play command
sv !disablecmd EX "play"

// disable say_team, say_person, say_group
sv !disablecmd SW "say_"
```

### `disabledel`

Delete a disabled command from the list. Takes the entry number as an arg. Related commands: `disablecmd`
```
// remove entry #5
sv !disabledel 5
```

### `displaylogfile`

Spit out an entire log file to the server console. In order to not DOS the server, it spits out a single line per server frame. Quake2 server logs can grow to absurd sizes, displaying entire log files is really dumb, don't use this command! Once the process is started, there doesn't seem to be a way to stop it.

```
// display the whole file for log #12 one line at a time
sv !displaylogfile 12
```

### `floodcmd`

Add an entry to the floodcmd list. The floodcmd entry is for applying normal flood rules to any arbitrary command. Only the first part of the command is matched, not any arguments. Do not use this for any command that outputs chat messages (`say`, `say_team`, `say_person`, etc).
```
sv !floodcmd [SW/EX/RE] "command"
```
SW = starts with \
EX = exact \
RE = regular expression
```
// add floodcmd for the play command
sv !floodcmd EX "play"

// any command starting with "play_"
sv !floodcmd SW "play_"
```

### `flooddel`

Remove a floodcmd entry from the list. The only argument is the number of the entry.
```
// remove the #2 floodcmd
sv !flooddel 2
```

### `freeze`

Lock a specific player in place. This works by overriding the player's msec value to 0 in their `usercmd_t` messages.
```
// lock player 3 in place
sv !freeze CL 3

// lock player named claire in place
sv !freeze claire
```

### `flush_logs`

Flush the log buffers to disk if contain any pending data.
```
// write all buffered log data to the files
sv !flush_logs
```

### `ip`

Display a player's IP address, either by name or player ID.
```
// show player 0's IP
sv !ip CL 0

// show claire's IP
sv !ip claire
```

### `kick`

Remove one or more players from the server. Multiple players supported by ID only.
```
// disconnect player 4
sv !kick CL 4

// disconnect claire
sv !kick claire

// disconnect players 3, 5, and 7
sv !kick CL 3 + 5 + 7
```

### `listbans`

Show all ban entires currently loaded.
```
sv !listbans
```

### `listchatbans`

Show all the chatban entires currently loaded.
```
sv !listchatbans
```

### `listcheckvar`

Show all the checkvar entries currently loaded.
```
sv !listcheckvar
```

### `listdisable`

Show all the disable entries currently loaded.
```
sv !listdisable
```

### `listfloods`

Show all the flood entries currently loaded.
```
sv !listfloods
```

### `listlrcons`

Show all the lrcon entries currently loaded.
```
sv !listlrcons
```

### `listspawns`

Show all custom spawn entries currently loaded.
```
sv !listspawns
```

### `listvotes`

Show all vote commands currently loaded.
```
sv !listvotes
```

### `logevent`

Manually show/edit logs definitions for a specific logtype
```
sv !logevent [view {logtype} / edit {logtype} [log {yes/no}] [logfiles [logfile1[+logfile2[...]]]] [format "format"]
```
Logtypes:
- *ADMINLOG*
- *BAN*
- *CHAT*
- *CHATBAN*
- *CLIENTBEGIN*
- *CLIENTCMDS*
- *CLIENTCONNECT*
- *CLIENTDISCONNECT*
- *CLIENTKICK*
- *CLIENTLRCON*
- *CLIENTUSERINFO*
- *DISABLECMD*
- *ENTITYCREATE*
- *ENTITYDELETE*
- *IMPULSES*
- *INTERNALWARN*
- *INVALIDIP*
- *NAMECHANGE*
- *PERFORMANCEMONITOR*
- *PRIVATELOG*
- *SERVEREND*
- *SERVERINIT*
- *SERVERSTART*
- *SKINCHANGE*
- *ZBOT*
- *ZBOTIMPULSES*
```
// View log setup for the "chat" logtype
sv !logevent view chat

// setup a new log for chatban logtype
sv !logevent edit chatban log yes logfiles "chatbans.log" format "%s"

// disable and existing log definition for chatbans
sv !logevent edit chatban log no
```

### `logfile`

View/edit all log file definitions. Unclear how this is different from `logevent` command.
```
sv !logfile [view [#]] / [edit # [mod] [filename]] / [del #]

// list all logfiles
sv !logfile view

// change the filename for logfile 2 and keep it in the mod directory
sv !logfile edit 2 yes newlogname.log

// delete the log definition for log 3
sv !logfile del 3
```

### `lrcon`

Add limited-rcon entries in the current list. An lrcon entry is a least-privilege implemention of rcon passwords that are only allowed to run very specific commands. You're able to create up to 1024 of them. You can use these add moderators to watch over your server who can only do things like change maps or mute/kick players. You can also simply revoke this access and never have to reveal/change your actual rcon password. This functionality has since been integrated into q2pro and r1q2 (I think), so mileage may vary.

The entire command (including args) is evaluated

Related commands: `listlrcon`, `lrcondel`
```
sv !lrcon [SW/EX/RE] "password" "command"
```
SW = starts with \
EX = exact \
RE = regular expression
```
// Allow changing map to only q2dm1 through q2dm8 (as a regular expression) with password "dingleberry"
sv !lrcon RE "dingleberry" "gamemap q2dm[1-8]"

// allow the map to be changed to anything (note the space)
sv !lrcon SW "dingleberry" "gamemap "

// allow user to see the status output (to see ips)
sv !lrcon EX "1ngr0wn" "status"
```

### `lrcondel`

Delete an lrcon entry from the list. To view the current list use the `listlrcon` command.
```
// delete lrcon entry #4
sv !lrcondel 4
```

### `mute`

Toggle a player's ability to speak for a specific timespan or for the duration of their playing session. If the player is already muted, they will be unmuted. The time arg is not needed to unmute.

Related commands: `stifle`
```
sv !mute [CL # | [LIKE] name] [[time in secs]|PERM] 
```
For a permanent mute, use "PERM" as the duration, otherwise it's a length of seconds. Permanent is relative here, if the player quits and rejoins, the mute will be gone.
```
// mute player 3 for 30 seconds
sv !mute CL 3 30

// mute claire for this whole playing session
sv !mute claire PERM

// unmute claire if they're already muted
sv !mute claire
```

### `namechangefloodprotect`

Set the global parameters for name-change flooding. These apply to all players but are tracked individually, so one player triggering this rule will not affect other player's ability to change names.
```
sv !namechangefloodprotect num_changes time_span prevention_time

// Set limits to 5 name changes in 30 seconds, prevent change again for 5 minutes
sv !namechangefloodprotect 5 30 300 
```

### `readloadbanfile`

Reread the banfile from disk. Useful to rehash the bans without needing to restart the server. This will reload ALL banlist files q2admin knows about, including:
1. any `q2a_ban.cfg` file in the main q2 directory
2. any `q2a_ban.cfg` file in the mod directory
3. any remote (http) ban files included in 1 & 2.

Ban file processing is recursive, so creating an include-loop will result in a very bad day. 
```
sv !reloadbanfile
```

### `reloadanticheatlist`

Reread the anticheat exception list. This will first take any anticheat exceptions from a remote exception file (if one is defined), and then the the local config file.
```
sv !reloadanticheatlist
```

### `reloadexceptionlist`

This is an alias for `reloadanticheatlist`
```
sv !reloadexceptionlist
```

### `reloadhashlist`

Reread the anticheat hash list files:
1. `anticheat-cvars.txt`
2. `anticheat-hashes.txt`
3. `anticheat-tokens.txt`
```
sv !reloadhashlist
```

### `reloadcheckvarfile`

Reread the checkvar config
```
sv !reloadcheckvarfile
```

### `reloaddisablefile`

Reread the disabled commands config
```
sv !reloaddisablefile
```

### `reloadfloodfile`

Reread the command flood config
```
sv !reloadfloodfile
```

### `reloadloginfile`

Reread the admin password/level config file
```
sv !reloadloginfile
```

### `reloadlrconfile`

Reread the lrcon config from disk
```
sv !reloadlrconfile
```

### `reloadspawnfile`

Reread the entity spawn config from disk
```
sv !reloadspawnfile
```

### `reloadvotefile`

Reread the voting config from disk
```
sv !reloadvotefile
```

### `reloadwhoisfile`
Reread the whois config from disk
```
sv !reloadwhoisfile
```

### `resetrcon`

Reset the server's rcon password to it's original value. Every time an lrcon password is used, the real rcon password is changed to something random to prevent sniffing traffic to see the real rcon password. It's set back automatically after a timeout, but this command forces that reset.
```
sv !resetrcon
```

### `say_group`

Send a private chat msg to a select group of players instead of to everyone.

Related commands: `say_person`, `say_person_low`
```
// say hi to players 0, 4, and 5
sv !say_group CL 0 + 4 + 5 "hey there"
```

### `say_person`

Send a private message to a particular player instead of to everyone.

Related commands: `say_person_low`, `say_group`
```
// say hi to player 2
sv !say_person CL 2 "yooooo!"

// say hi claire
sv !say_person claire "hey bruh"
```

### `say_person_low`

Send a message to a particular player as a PRINT_LOW message. This will not highlight the message as chat and will not trigger the chat sound in-game.

Related commands: `say_person`, `say_group`
```
sv !say_person_low CL 3 "The white zone is for loading and unloading only"
```

### `setadmin`

Seemingly unimplemented...

### `skinchangefloodprotect`

Set parameters around skin changes
```
sv !skinchangefloodprotect num_changes in_secs relax_secs

// if someone changes skins 3 times in 10 seconds, prevent changes for 5 minutes
sv !skinchangefloodprotect 3 10 300
```

### `spawncmd`

Add an entry to block the spawning of a particular entity. This won't despawn entities that already linked, but can prevent them from respawning.

Related commands: `listspawn`, `spawndel`
```
sv !spawncmd [SW/EX/RE] "classname"
```
SW = starts with \
EX = exact \
RE = regular expression
```
// prevent health from spawning
sv !spawncmd SW "health_"

// make it dark (untested probably won't work)
sv !spawncmd RE ".*light.*"
```

### `spawndel`

Remove a specific disabled spawn entry from the list. This isn't persistent, restarting the server will reload spawncmds from disk and restore the entry.
```
// remove spawn entry #4
sv !spawndel 4
```

### `stifle`

Half-mute for players. This allows the player to say one thing every minute.
```
sv !stifle [CL # | name] length_in_secs
```
The `length_in_secs` arg is how long the half-mute applies to the player, not the length of time between allowing the single chat message.
```
// half-mute for player named 888 for the next 10 minutes
sv !stifle 888 600

// half-mute player 0 for the next 30 minutes
sv !stifle CL 0 1800
```

### `stuff`

Force a player to execute a specific command (or set of commands) as if they typed it into their own console. You can specify the command directly or supply a file containing a list of commands.
```
sv !stuff [CL #] | name ["command" | FILE "filename"]
```
If you're supplying a file, it needs to be in the mod folder
```
// force claire to change name
sv !stuff claire "name bunghole_breeze"

// force player 2 to quit
sv !stuff CL 2 "quit"

// force player 4 to do all the commands in a file
sv !stuff CL 4 FILE "stuffcmds.txt"
```

### `unfreeze`

Release a player from being frozen.

Related commands: `freeze`
```
// unfreeze player 0
sv !unfreeze CL 0

// unfreeze player named claire
sv !unfreeze claire
```

### `unstifle`

Remove a half-mute affecting a player
```
sv !unstifle CL 0

sv !unstifle claire
```

### `version`

Display q2admin's current version string
```
sv !version
```

### `votecmd`

Add a new vote proposal for players to use
```
sv !votecmd [SW/EX/RE] "command"
```
SW = starts with \
EX = exact \
RE = regular expression
```
// allow voting for only the original 8 dm maps
sv !votecmd RE "^gamemap q2dm[1-8]$"

// allow voting to stifle any player
sv !votecmd SW "^sv !stifle "

// allow voting to kick 888 specifically
sv !votecmd EX "sv !kick 888"
```

### `votedel`

Remove a votecmd from the current list by number.

Related commands: `listvote`, `votecmd`
```
// remove vote #5
sv !votedel 5
```

### `vpnusers`

Display all players who are connected via a VPN.
```
sv !vpnusers
```
