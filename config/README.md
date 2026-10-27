# Example config files

These are example q2admin config files. Each one is commented to explain
every setting it holds and the format of its lines. They aren't used by the
build. q2admin reads files like these at runtime, from your Quake 2 server's
directories.

## Using them

1. Copy the `.cfg` files you need into your mod directory, next to the
   q2admin library (for example `opentdm/`).
2. Edit them to suit your server. Start with `q2admin.cfg`.
3. Start or restart the server.

You don't need every file. If a file is missing, q2admin uses its built-in
defaults for that feature.

Each file is read from the Quake 2 root directory first and then from the mod
directory, and later values override earlier ones. So you can keep shared
settings in the root and per-mod settings beside the mod.

## The files

| File | Purpose | Cvar to use a different name |
| --- | --- | --- |
| `q2admin.cfg` | Main settings: every option in the [options table](../README.md#options). | `q2aconfig` |
| `q2a_ban.cfg` | Ban and chat-ban rules, plus `INCLUDE:` of local files or URLs. | `q2a_banfile` |
| `q2a_cvar.cfg` | Client variables to enforce (constant values or ranges). | `q2a_cvarfile` |
| `q2a_disable.cfg` | Mod commands players aren't allowed to use. | `q2a_disablefile` |
| `q2a_flood.cfg` | Extra commands that count toward flood protection. | `q2a_floodfile` |
| `q2a_lrcon.cfg` | Limited-rcon passwords and the commands each one may run. | `q2a_rconfile` |
| `q2a_vote.cfg` | Commands players are allowed to call votes on. | `q2a_votefile` |
| `q2a_spawn.cfg` | Map entities that should never spawn. | `q2a_spawnfile` |
| `q2a_log.cfg` | Log file definitions and which events go to which file. | `q2a_logfile` |
| `q2a_login.cfg` | In-game admin accounts: `name password level` per line. | `q2a_loginfile` |
| `q2a_bypass.cfg` | Anti-cheat bypass accounts: `name password level` per line. | `q2a_bypassfile` |
| `q2a_cloud.cfg` | Cloud Admin connection settings (still under development). | `q2a_cloudfile` |

The cvars are set on the server's command line, for example
`+set q2a_banfile q2a_ban_ctf.cfg`. That lets several servers share one mod
directory and still have their own files.

## Format

Lines starting with `;` are comments. Settings in `q2admin.cfg` are written
as `option "value"`, one per line. The other files each have their own line
format, described in the comments at the top of the file. Unknown options in
`q2admin.cfg` are silently ignored, so check the spelling if a setting
doesn't seem to take effect.

## example.cfg

`make example-cfg` generates `example.cfg` here. It lists the options you're
most likely to change, each with its default value. It's built from the
option table in the source, so it's always current.

## More detail

The main [README](../README.md) covers each feature and option in full. See
[Configuration basics](../README.md#configuration-basics) and the
[Configuration reference](../README.md#configuration-reference).
