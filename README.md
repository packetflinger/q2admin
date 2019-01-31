Q2Admin
==========

Q2Admin is a management and security addon for Quake 2 servers. It acts as a proxy game module; the Quake 2 server loads q2admin as its game module and q2admin will open the actual game module and proxy between the two.


Features
---------
* Detect and handle cheaters (old ratbots and proxies)
* Chat and name filtering (prevent certain words from being used)
* Force or prohibit skins
* Force client-side variables (cvars) to ranges or exact values (`cl_maxfps`, `rate`, `timescale`, etc)
* Useful commands commands added (`say_person`, `play_team`, `stuff`)
* Remote management via webapp *still under development*
  * View status and logs of all your Q2 servers (chats, frags, connections)
  * Issue commands to groups of servers
  * Delegate control of servers to other admin users
* Very flexible logging (up to 32 different log files)
* Custom banning (IP, chat, nick)
* Built-in command disabling
* Flood control
* Limited RCON support
* Map Entity disabling by class (`weapon_bfg`, `ammo_slugs`, `item_quad`, `trigger_hurt`, etc...)
* Custom voting


Compiling for Linux
-------------------
Simply run `make` in the root folder 


Compiling for Windows using MinGW
---------------------
Edit `.config-win32mingw` file to suit your environment and rename it to `.config`. Then run `make` to build your DLL.


Configuration
--------------
Copy all all the .cfg files from the `runtime-config` directory to your mod directory. The main config file is `q2admin.cfg`, edit this file to suit your needs. You should specify the real game library in the main config file using the `gamelibrary` option. You can also the old `gamex86_64.real.so` or `gamex86.real.dll` method or use the `+set gamelib <libraryname>` command-line argument when starting your Quake 2 server.

 
Dependencies
------------
1. `libcurl`
2. `glib2.0`

For Debian run `apt-get install libcurl4-openssl-dev libglib2.0-dev`

For Fedora run `dnf install libcurl-devel glib2-devel`

For Centos run `yum install libcurl-devel glib2-devel`

or...if you're a glutton for punishment, compile them manually on your own.
