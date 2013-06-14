/*
Copyright (C) 2008 MDVz0r

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
// zb_hashl.c
//
// copyright 2008 MDVz0r
//

#include "g_local.h"
#include "fopen.h"

qboolean ReadRemoteHashListFile(char *bfname, char *blname)
{
    URL_FILE *handle;
    FILE *outf;

    sprintf(buffer, "%s/%s", moddir, blname);

    // copy from url line by line with fgets //
    outf=fopen(buffer,"w");
    if(!outf)
    {
	gi.dprintf ("Error opening local hash list file.\n");
	return FALSE;
    }

    handle = url_fopen(bfname, "r");
    if(!handle)
    {
	gi.dprintf ("Error opening remote hash list file.\n");
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
void getR1chHashList(char *hashname)
{

    char cfgHashList_enabled[100];
    q2a_strcpy(cfgHashList_enabled, q2adminhashlist_enable->string);

        if ( cfgHashList_enabled[0] == '1')
        {
	    qboolean ret;
            char cfgHashRemoteList[100];

            if(!q2adminhashlist_dir || isBlank(q2adminhashlist_dir->string))
                    {
                        q2a_strcat(q2a_strcat(q2a_strcpy(cfgHashRemoteList, HASHLISTREMOTEDIR), "/"), hashname);
                    }
            else
                    {
                        q2a_strcat(q2a_strcat(q2a_strcpy(cfgHashRemoteList, q2adminhashlist_dir->string), "/"), hashname);
                    }
            ret = ReadRemoteHashListFile(cfgHashRemoteList, hashname);

            if(!ret)
                {
                        gi.dprintf ("WARNING: " HASHLISTREMOTEDIR " could not be found\n");
                        logEvent(LT_INTERNALWARN, 0, NULL, HASHLISTREMOTEDIR " could not be found", IW_BANSETUPLOAD, 0.0);
                }
        }
}

void loadhashlist(void)
{
    char cfgHashList_enabled[100];
    q2a_strcpy(cfgHashList_enabled, q2adminhashlist_enable->string);
        if ( cfgHashList_enabled[0] == '1')
	{
	    // flush cache, which really sux
            q2a_strcpy(buffer, "fsflushcache\n");
	    gi.AddCommandString(buffer);
	    getR1chHashList("anticheat-cvars.txt");
	    getR1chHashList("anticheat-hashes.txt");
	    getR1chHashList("anticheat-tokens.txt");
    	    // load hash list (is there a command to do this? else load on restart
            q2a_strcpy(buffer, "svacupdate\n");
	    gi.AddCommandString(buffer);
	}
}

void reloadhashlistRun(int startarg, edict_t *ent, int client)
{
    loadhashlist();
    gi.cprintf (ent, PRINT_HIGH, "Remote hashlist loaded.\n");
}
