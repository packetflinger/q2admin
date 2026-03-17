# Q2Admin

Q2Admin is a management and security addon for Quake 2 servers. It acts as a proxy game module; the Quake 2 server loads q2admin as its game module and q2admin will open the actual game module and proxy between the two.


## Features

* Detect and handle cheaters (old ratbots and proxies)
* IPv6 Support
* Chat and name filtering (prevent certain words from being used)
* Force or prohibit specific skins
* Force client-side variables (cvars) to ranges or exact values (`cl_maxfps`, `rate`, `timescale`, etc)
* Useful commands commands added (`say_person`, `play_team`, `stuff`)
* Remote management via webapp/SSH *still under development*
  * View status and logs of all your Q2 servers (chats, frags, connections)
  * Issue commands to groups of servers
  * Delegate control of servers to other admin users
* Very flexible logging (up to 32 different log files)
* Custom banning (IP, chat, nick, client version, VPN, ASN)
* Built-in command disabling
* Flood control
* Limited RCON support
* Map Entity disabling by class (`weapon_bfg`, `ammo_slugs`, `item_quad`, `trigger_hurt`, etc...)
* Map entity swapping 
* Custom voting
* VPN detection and banning


## Compiling for Linux

Simply run `make` in the root folder 


## Compiling for Windows using MinGW

Edit `.config-win32mingw` file to suit your environment and rename it to `.config`. Then run `make` to build your DLL.


## Configuration

Copy all all the .cfg files from the `runtime-config` directory to your mod directory. The main config file is `q2admin.cfg`, edit this file to suit your needs. You should specify the real game library in the main config file using the `gamelibrary` option. You can also the old `gamex86_64.real.so` or `gamex86.real.dll` method or use the `+set gamelib <libraryname>` command-line argument when starting your Quake 2 server.

 
## Dependencies

1. `libcurl`
2. `libssl`
3. `libcrypto`
4. `zlib`
5. `libpthread`

All dependencies are included and statically linked. The resulting binary is about 8MB, most of which is related to libcrypto. This is unfortunately required for the Cloud Admin connection (authentication and transit encryption).  

## Settings

### Server CVARs
CVAR | Purpose | Default
--- | --- | ----
`gamelib` | Specify the real game library for q2admin to load |
`q2aconfig` | Specify the main config filename | q2admin.cfg
`configfile_ban` | Config file for bans | q2a_ban.cfg
`configfile_bypass` | Config file for bypass access | q2a_bypass.cfg
`configfile_cloud` | Cloud admin config | q2a_cloud.cfg
`configfile_cvar` | CVAR banning config | q2a_cvar.cfg
`configfile_disable` | Command disabling config | q2a_disable.cfg
`configfile_flood` | Flood config | q2a_flood.cfg
`configfile_login` | Admin definitions | q2a_login.cfg
`configfile_log` | Logging setup | q2a_log.cfg
`configfile_rcon` | LRCON setup | q2a_lrcon.cfg
`configfile_spawn` | Entity disabling config | q2a_spawn.cfg
`configfile_vote` | Voting setup | q2a_vote.cfg


### Config Options
Option | Type | Default | What it does
--- | --- | --- | ---
`adminpassword` | string | "" | A password to use to auth as an admin
`banonconnect` | bool | yes | Banning happens in `ClientConnect` instead of `ClientBegin`
`chatbanning_enable` | bool | yes | Filter chat messages based on config
`chatfloodprotectmsg` | string | "" | Msg to send when flooding is triggered
`checkclientipaddress` | bool | yes | Do various checks based on a player's IP
`checkvar_poll_time` | number |  | How many seconds between CVAR checks?
`checkvarcmds_enable` | bool | yes | Enable CVAR checking
`clientremindtimeout` | number | ?? | ??
`clientsidetimeout` | number | ?? | ??
`clientvotecommand` | string | "qvote" | The command players will use to cast and propose votes
`clientvotetimeout` | number | ?? | How long is a vote proposal valid?
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
`client_map_cfg` | number | 6 | Bitmask controlling what config files get automatically stuffed to clients <br><br>set map_name [map] = 1<br>exec [mapname].cfg = 2 <br> exec all.cfg = 4
`client_msg` | string | ?? | ??
`cl_anglespeedkey_display` | bool | yes | Broadcast to all players when someone is messing with their cl_anglespeedkey cvar
`cl_anglespeedkey_enable` | bool | no | Enable cl_anglespeedkey monitoring
`cl_anglespeedkey_kick` | bool | no | Kick players caught manipulating this cvar
`cl_anglespeedkey_kickmsg` | string | ?? | The message to send the offending player when they're kicked
`cl_pitchspeed_display` | bool | yes | Broadcast to all players when someone is messing with their cl_pitchspeed cvar
`cl_pitchspeed_enable` | bool | no | Enable cl_pitchspeed monitoring
`cl_pitchspeed_kick` | bool | no | Kick players caught manipulating this cvar
`cl_pitchspeed_kickmsg` | string | ?? | The message to send the offending player when they're kicked
`consolechat_disable` | bool | no | Disable sending player chat to the server console
`consolelog_enable` | bool | no | Enable logging some events to the server console
`consolelog_pattern` | string | "[q2a] %s\n" | The printf-like format for events logged to server console
`customclientcmd` | string | "" | Command to stuff to players ?????
`customclientcmdconnect` | string | "" | Command to stuff to players when they connect
`customservercmd` | string | "" | Command to run on the server ?????
`customservercmdconnect` | string | "" | Command to run on the server when a player connects
`defaultbanmsg` | string | "" | Message players will see by default when they are not allowed to connect
`defaultchatbanmsg` | string | "" | Message players will see by default when they say something that isn't allowed
`defaultreconnectmessage` | string | "" | Message players will see when they are forced to reconnect on connect
`disablecmds_enable` | bool | no | Enable checking for disabled commands 
`disconnectuser` | bool | yes | Kick a player if it's discovered they're cheating
`disconnectuserimpulse` | bool | no | Kick a player if they use certain impulses
`displayimpulses` | bool | no | Broadcast the fact that a player just used a certain impulse
`displaynamechange` | bool | yes | Broadcast when someone changes their name
`dopversion` | bool | ?? | Do a p_verion probe for proxies when a player connects
`do_franck_check` | bool | yes | Check for franck when a player connects
`do_vid_restart` | bool | no | Force client to do a `vid_restart` command when they connect. This can unload some wallhacks
`enforce_deadlines` | bool | yes | When asking the player's client for certain information, set a reasonable deadline for a response and kick the player if no response is provided (indicates a modified client)
`entity_classname_offset` | number | ??? | What byte offset can the `classname` property be found in the `edict_s` struct? Since most of the edict_s struct is opaque and filled in by the game library, it's not possible for q2admin to know where that value is located. This value can be different for each game mod. Using an incorrect value here can lead to crashes, especially if you're doing entity substitution/blocking.<br><br>Common values:<br>baseq2 = xxxxx<br>opentdm = xxxxxx
`extendedsay_enable` | bool | no | ?????
`filternonprintabletext` | bool | no | Strip console characters (ascii values in range 128-256) from chat messages
`fpsfloodexempt` | bool | no | ?????
`framesperprocess` | number | 0 | ?????
`gamemaptomap` | bool | no | Convert any usage of `gamemap` command to `map`. This will cause all new maps to reload the game library and use significant resources. *Don't use this*. Use `sv_recycle` as part of q2pro/r1q2.
`gamelibrary` | string | "" | Specifies the real mod library to use. This can be in various ways which have different priorites.<br><br>Setting game via CVAR when server is run will override all, then this option in the config, then using filenames (*gamex86_64.real.so*)
`gl_driver_check` | number | 0 | ???
`gl_drive_max_changes` | number | 3 | Number of times the GL drive can be changed before assuming shenanigans. To catch people toggling a wallhack while playing
`hackuserdisplay` | string | ??? | The message to broadcast when a cheating player is discovered
`http_cacert_path` | string | "/etc/ssl/certs" | Where do we find the system's certificate authority public keys? Only used if `http_veryifyssl` is enabled for ensuring the https server is who they say they are
`http_debug` | bool | no | Show extra debug info related to CURL usage in the server console
`http_enable` | bool | yes | Enable libcurl for downloading stuff via http(s)<br><br>Required for<br>- Remote ban files<br>- VPN detection<br>- Loading remote anticheat configs<br>- ASN banning
`http_verifyssl` | bool | yes | When downloading a file via https, verify the TLS certificate is signed, valid, trusted and the common name on the cert matches the domain in the URL. Q2admin doesn't download and execute arbitrary files from the internet, so certificate issues are fairly low risk. Try disabling this if you're having trouble fetching https files (especially with https redirects)
`impulsestokickon` | string | ??? | A list of impulse values that will earn a player a kick. These values are what zbot's use for their onscreen menu
