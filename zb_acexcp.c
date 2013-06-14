/*
Copyright (C) 2007-2008 MDVz0r

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

//
// q2admin
//
// zb_acexcp.c
//
// copyright 2007-2008 MDVz0r
//

#include "g_local.h"
#include "fopen.h"

qboolean ReadRemoteAnticheatExceptionFile(char *bfname)
{
    URL_FILE *handle;
    FILE *outf;

    sprintf(buffer, "%s/%s", moddir, ANTICHEATEXCEPTIONLOCALFILE);

    // copy from url line by line with fgets //
    outf=fopen(buffer,"w");
    if(!outf)
    {
	gi.dprintf ("Error opening local anticheat exception file.\n");
	return FALSE;
    }

    handle = url_fopen(bfname, "r");
    if(!handle)
    {
	gi.dprintf ("Error opening remote anticheat exception file.\n");
	fclose(outf);
	return FALSE;
    }

    while(!url_feof(handle))
    {
        if(!url_fgets(buffer,sizeof(buffer),handle)) {
		// if it did timeout we are not trying again forever... - hifi
		gi.dprintf("Timeout while waiting for reply.\n");
		url_fclose(handle);
		fclose(outf);
		return FALSE;
	}
        fwrite(buffer,1,strlen(buffer),outf);
    }

    url_fclose(handle);
    fclose(outf);
    return TRUE;
}

// download up to date anticheat config file including all execptions for r1ch
// ugly code.. ;x
void getR1chExceptionList(void)
{
    char cfgAnticheatList_enabled[100];
    q2a_strcpy(cfgAnticheatList_enabled, q2adminanticheat_enable->string);

        if ( cfgAnticheatList_enabled[0] == '1')
        {
	    qboolean ret;
            char cfgAnticheatRemoteList[100];

            if(!q2adminanticheat_file || isBlank(q2adminanticheat_file->string))
                    {
                        q2a_strcpy(cfgAnticheatRemoteList, ANTICHEATEXCEPTIONREMOTEFILE);
                    }
            else
                    {
                        q2a_strcpy(cfgAnticheatRemoteList, q2adminanticheat_file->string);
                    }

            ret = ReadRemoteAnticheatExceptionFile(cfgAnticheatRemoteList);

            if(!ret)
                {
                        gi.dprintf ("WARNING: " ANTICHEATEXCEPTIONREMOTEFILE " could not be found\n");
                        logEvent(LT_INTERNALWARN, 0, NULL, ANTICHEATEXCEPTIONREMOTEFILE " could not be found", IW_BANSETUPLOAD, 0.0);
                }
        }
}

void loadexceptionlist(void)
{
    char cfgAnticheatList_enabled[100];
    q2a_strcpy(cfgAnticheatList_enabled, q2adminanticheat_enable->string);
        if ( cfgAnticheatList_enabled[0] == '1')
	{
	    // flush cache, which really sux
            q2a_strcpy(buffer, "fsflushcache\n");
	    gi.AddCommandString(buffer);
	    getR1chExceptionList();
    	    // execute exception list even if the download was not succeeded, since there is probably an old version available.
            q2a_strcpy(buffer, "exec " ANTICHEATEXCEPTIONLOCALFILE "\n");
	    gi.AddCommandString(buffer);
	}
}

void reloadexceptionlistRun(int startarg, edict_t *ent, int client)
{
    loadexceptionlist();
    gi.cprintf (ent, PRINT_HIGH, "Exceptionlist loaded.\n");
}
