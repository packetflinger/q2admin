q2admin
==========

Q2Admin module used on the Packetflinger.com Q2 servers. Originally forked from version 1.17.51

I'm in the process of removing the old terrible regular expression matching and moving to wildcard matching.
It can now communicate with a central remote q2admin server to manage all your Quake II servers from a 
single web interface.

Compiling for Linux
-------------------
Simply run `make` in the root folder 


Compiling for Windows using MinGW
---------------------
Edit `.config-win32mingw` file to suit your environment and rename it to `.config`. Then run `make` to build your DLL.


Dependencies
------------
1. `libcurl`
2. `glib2.0`
