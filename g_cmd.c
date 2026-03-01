/*
Copyright (C) 2000 Shane Powell

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

#include "g_local.h"

int lastClientCmd = -1;

block_model block_models[MAX_BLOCK_MODELS] ={
    //projected model wallhack protection list.
    {
        "models/items/adrenal/tris.md2"
    },
    {
        "models/items/armor/body/tris.md2"
    },
    {
        "models/items/armor/combat/tris.md2"
    },
    {
        "models/items/armor/jacket/tris.md2"
    },
    {
        "models/items/armor/shield/tris.md2"
    },
    {
        "models/items/band/tris.md2"
    },
    {
        "models/items/invulner/tris.md2"
    },
    {
        "models/items/mega_h/tris.md2"
    },
    {
        "models/items/quaddama/tris.md2"
    },
    {
        "models/objects/rocket/tris.md2"
    },
    {
        "models/ctf/strength/tris.md2"
    },
    {
        "models/ctf/haste/tris.md2"
    },
    {
        "models/ctf/resistance/tris.md2"
    },
    {
        "models/ctf/regeneration/tris.md2"
    },
    {
        "players/male/flag2.md2"
    },
    {
        "players/male/flag1.md2"
    },
    {
        "models/objects/grenade2/tris.md2"
    },
    {
        "models/weapons/g_machn/tris.md2"
    },
    { 
        "models/weapons/g_rocket/tris.md2"
    },
    { 
        "models/weapons/g_hyperb/tris.md2"
    },
    {
        "models/weapons/g_shotg/tris.md2"
    },
    {
        "models/weapons/g_chain/tris.md2"
    },
    {
        "models/weapons/g_rail/tris.md2"
    },
    {
        "models/weapons/g_shotg2/tris.md2"
    },
    {
        "models/weapons/g_launch/tris.md2"
    },
    {
        "models/items/armor/shard/tris.md2"
    }
};

q2acmd_t q2aCommands[] = {
    {
        "adminpassword",
        CMDWHERE_CFGFILE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        adminpassword
    },
    {
        "ban",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        banRun
    },
    {
        "banonconnect",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &banOnConnect
    },
    {
        "chatban",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        chatbanRun
    },
    {
        "chatbanning_enable",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &ChatBanning_Enable
    },
    {
        "chatfloodprotect",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        chatFloodProtectRun,
        chatFloodProtectInit
    },
    {
        "chatfloodprotectmsg",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        chatFloodProtectMsg
    },
    {
        "checkclientipaddress",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &checkClientIpAddress
    },
    {
        "checkvar_poll_time",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &checkvar_poll_time
    },
    {
        "checkvarcmd",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        checkvarcmdRun
    },
    {
        "checkvarcmds_enable",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &checkvarcmds_enable
    },
    {
        "checkvardel",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        checkvarDelRun
    },
    {
        "clearlogfile",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        clearlogfileRun
    },
    {
        "clientchatfloodprotect",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        clientchatfloodprotectRun
    },
    {
        "clientremindtimeout",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &clientRemindTimeout,
    },
    {
        "clientsidetimeout",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &clientsidetimeout,
        clientsidetimeoutRun,
        clientsidetimeoutInit,
    },
    {
        "clientvotecommand",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        clientVoteCommand
    },
    {
        "clientvotetimeout",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &clientVoteTimeout,
    },
    {
        "cloud",
        CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        cloudRun,
    },
    {
        "cloud_flags",
        CMDWHERE_CFGFILE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &cloud_flags,
    },
    {
        "cloud_cmd_teleport",
        CMDWHERE_CFGFILE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        cloud_cmd_teleport,
    },
    {
        "cloud_cmd_invite",
        CMDWHERE_CFGFILE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        cloud_cmd_invite,
    },
    {
        "cloud_cmd_seen",
        CMDWHERE_CFGFILE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        cloud_cmd_seen,
    },
    {
        "cloud_cmd_whois",
        CMDWHERE_CFGFILE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        cloud_cmd_whois,
    },
    {
        "cloud_dns",
        CMDWHERE_CFGFILE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        cloud_dns,
    },
    {
        "cloud_enabled",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &cloud_enabled,
    },
    {
        "cloud_address",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        &cloud_address,
    },
    {
        "cloud_port",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &cloud_port,
    },
    {
        "cloud_encryption",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &cloud_encryption,
    },
    {
        "cloud_publickey",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        &cloud_publickey,
    },
    {
        "cloud_privatekey",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        &cloud_privatekey,
    },
    {
        "cloud_serverkey",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        &cloud_serverkey,
    },
    {
        "cloud_uuid",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        &cloud_uuid,
    },
    {
        "cl_anglespeedkey_display",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &cl_anglespeedkey_display,
    },
    {
        "cl_anglespeedkey_enable",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &cl_anglespeedkey_enable,
        cl_anglespeedkey_enableRun,
    },
    {
        "cl_anglespeedkey_kick",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &cl_anglespeedkey_kick,
    },
    {
        "cl_anglespeedkey_kickmsg",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        cl_anglespeedkey_kickmsg,
    },
    {
        "cl_pitchspeed_display",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &cl_pitchspeed_display,
    },
    {
        "cl_pitchspeed_enable",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &cl_pitchspeed_enable,
        cl_pitchspeed_enableRun,
    },
    {
        "cl_pitchspeed_kick",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &cl_pitchspeed_kick,
    },
    {
        "cl_pitchspeed_kickmsg",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        cl_pitchspeed_kickmsg,
    },
    {
        "consolechat_disable",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &consolechat_disable
    },
    {
        "consolelog_enable",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &consolelog_enable
    },
    {
        "consolelog_pattern",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        &consolelog_pattern
    },
    {
        "customclientcmd",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        customClientCmd
    },
    {
        "customclientcmdconnect",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        customClientCmdConnect
    },
    {
        "customservercmd",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        customServerCmd
    },
    {
        "customservercmdconnect",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        customServerCmdConnect
    },
    {
        "cvarset",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        cvarsetRun
    },
    {
        "defaultbanmsg",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        defaultBanMsg
    },
    {
        "defaultchatbanmsg",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        defaultChatBanMsg
    },
    {
        "defaultreconnectmessage",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        defaultreconnectmessage
    },
    {
        "delban",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        delbanRun
    },
    {
        "delchatban",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        delchatbanRun
    },
    {
        "developer",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &q2a_developer
    },
    {
        "disablecmd",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        disablecmdRun
    },
    {
        "disablecmds_enable",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &disablecmds_enable
    },
    {
        "disabledel",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        disableDelRun
    },
    {
        "disconnectuser",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &disconnectuser
    },
    {
        "disconnectuserimpulse",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &disconnectuserimpulse
    },
    {
        "displayimpulses",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &displayimpulses
    },
    {
        "displaylogfile",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        displaylogfileRun
    },
    {
        "displaynamechange",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &displaynamechange
    },
    {
        "dopversion",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &dopversion
    },
    {
        "displayzbotuser",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &displayzbotuser
    },
    {
        "enforce_deadlines",
        CMDWHERE_CFGFILE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &enforce_deadlines
    },
    {
        "entity_classname_offset",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &entity_classname_offset,
    },
    {
        "extendedsay_enable",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &extendedsay_enable
    },
    {
        "filternonprintabletext",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &filternonprintabletext
    },
    {
        "floodcmd",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        floodcmdRun
    },
    {
        "flooddel",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        floodDelRun
    },
    {
        "fpsfloodexempt",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &fpsFloodExempt
    },
    {
        "framesperprocess",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &framesperprocess
    },
    {
        "freeze",
        CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        freezeRun
    },
    {
        "gamemaptomap",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &gamemaptomap
    },
	{
        "gamelibrary",
        CMDWHERE_CFGFILE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        gamelibrary
    },
    {
        "hackuserdisplay",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        hackuserdisplay
    },
    {
        "http_cacert_path",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        http_cacert_path
    },
    {
        "http_debug",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &http_debug
    },
    {
        "http_enable",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &http_enable
    },
    {
        "http_verifyssl",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &http_verifyssl
    },
    {
        "impulsestokickon",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &impulsesToKickOn,
        impulsesToKickOnRun,
        impulsesToKickOnInit
    },
    {
        "ip",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        ipRun
    },
    {
        "ip_limit",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &ip_limit
    },
    {
        "ip_limit_vpn",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &ip_limit_vpn
    },
    {
        "ipbanning_enable",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &IPBanning_Enable
    },
    {
        "kick",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        kickRun
    },
    {
        "kickonnamechange",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &kickOnNameChange
    },
    {
        "listbans",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        listbansRun
    },
    {
        "listchatbans",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        listchatbansRun
    },
    {
        "listcheckvar",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        listcheckvarsRun
    },
    {
        "listdisable",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        listdisablesRun
    },
    {
        "listfloods",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        listfloodsRun
    },
    {
        "listlrcons",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        listlrconsRun
    },
    {
        "listspawns",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        listspawnsRun
    },
    {
        "listvotes",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        listvotesRun
    },
    {
        "lock",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &lockDownServer,
        lockDownServerRun,
    },
    {
        "lockoutmsg",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        lockoutmsg,
    },
    {
        "logevent",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        logeventRun
    },
    {
        "logfile",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        logfileRun
    },
    {
        "lrcon_timeout",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &lrcon_timeout
    },
    {
        "lrcon",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        lrconRun
    },
    {
        "lrcondel",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        lrconDelRun
    },
    {
        "mapcfgexec",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &mapcfgexec
    },
    {
        "maxclientsperframe",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &maxclientsperframe
    },
    {
        "maxfps",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &maxfpsallowed,
        maxfpsallowedRun,
        maxfpsallowedInit
    },
    {
        "maximpulses",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &maximpulses
    },
    {
        "maxmsglevel",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &maxMsgLevel
    },
    {
        "maxrate",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &maxrateallowed,
        maxrateallowedRun
    },
    {
        "minfps",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &minfpsallowed,
        minfpsallowedRun,
        minfpsallowedInit
    },
    {
        "minrate",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &minrateallowed,
        minrateallowedRun
    },
    {
        "mute",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        muteRun
    },
    {
        "namechangefloodprotect",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        NULL,
        nameChangeFloodProtectRun,
        nameChangeFloodProtectInit
    },
    {
        "namechangefloodprotectmsg",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        nameChangeFloodProtectMsg
    },
    {
        "nickbanning_enable",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &NickBanning_Enable
    },
    {
        "numofdisplays",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &numofdisplays
    },
    {
        "printmessageonplaycmds",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &printmessageonplaycmds
    },
    {
        "proxy_bwproxy",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &proxy_bwproxy
    },
    {
        "proxy_nitro2",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &proxy_nitro2
    },
    {
        "quake2dirsupport",
        CMDWHERE_CFGFILE,
        CMDTYPE_LOGICAL,
        &quake2dirsupport
    },
    {
        "q2adminrunmode",
        CMDWHERE_CFGFILE,
        CMDTYPE_NUMBER,
        &runmode
    },
    {
        "randomwaitreporttime",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &randomwaitreporttime
    },
    {
        "rcon_random_password",
        CMDWHERE_CFGFILE,
        CMDTYPE_LOGICAL,
        &rcon_random_password
    },
    {
        "rcon_insecure",
        CMDWHERE_CFGFILE,
        CMDTYPE_LOGICAL,
        &rcon_insecure
    },
    {
        "reconnect_address",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        reconnect_address
    },
    {
        "reconnect_checklevel",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &reconnect_checklevel
    },
    {
        "reconnect_time",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &reconnect_time
    },
    {
        "reloadbanfile",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        reloadbanfileRun,
    },
    {
        "reloadexceptionlist",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        AC_ReloadExceptions,
    },
    {
        "reloadanticheatlist",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        AC_ReloadExceptions,
    },
    {
        "reloadhashlist",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        reloadhashlistRun,
    },
    {
        "reloadcheckvarfile",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        reloadCheckVarFileRun,
    },
    {
        "reloaddisablefile",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        reloadDisableFileRun,
    },
    {
        "reloadfloodfile",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        reloadFloodFileRun,
    },
    {
        "reloadlrconfile",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        reloadlrconfileRun,
    },
    {
        "reloadspawnfile",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        reloadSpawnFileRun,
    },
    {
        "reloadvotefile",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        reloadVoteFileRun,
    },
    {
        "resetrcon",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        lrcon_reset_rcon_password,
    },
    {
        "say_group",
        CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        sayGroupRun,
    },
    {
        "say_group_enable",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &say_group_enable
    },
    {
        "say_person",
        CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        sayPersonRun,
    },
    {
        "say_person_low",
        CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        sayPersonLowRun,
    },
    {
        "say_person_enable",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &say_person_enable
    },
    {
        "serverinfoenable",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &serverinfoenable
    },
    {
        "setadmin",
        CMDWHERE_CLIENTCONSOLE,
        CMDTYPE_NONE,
        NULL,
        setadminRun
    },
    {
        "setmotd",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        motdFilename,
        motdRun,
    },
    {
        "skinchangefloodprotect",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        NULL,
        skinChangeFloodProtectRun,
        skinChangeFloodProtectInit
    },
    {
        "skinchangefloodprotectmsg",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        skinChangeFloodProtectMsg
    },
    {
        "skincrashmsg",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        skincrashmsg
    },
    {
        "soloadlazy",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &soloadlazy
    },
    {
        "spawncmd",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        spawncmdRun
    },
    {
        "spawndel",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        spawnDelRun
    },
    {
        "spawnentities_enable",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &spawnentities_enable
    },
    {
        "spawnentities_internal_enable",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &spawnentities_internal_enable
    },
    {
        "stifle",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        stifleRun
    },
    {
        "unfreeze",
        CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        unfreezeRun
    },
    {
        "unstifle",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        unstifleRun
    },
    {
        "stuff",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        stuffClientRun,
    },
    {
        "swap_attack_use",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &swap_attack_use
    },
    {
        "timescaledetect",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &timescaledetect
    },
    {
        "timescaleuserdisplay",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        timescaleuserdisplay
    },
    {
        "version",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        versionRun
    },
    {
        "versionbanning_enable",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &VersionBanning_Enable
    },
    {
        "votecmd",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        votecmdRun
    },
    {
        "voteclientmaxvotes",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &clientMaxVotes
    },
    {
        "voteclientmaxvotetimeout",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &clientMaxVoteTimeout
    },
    {
        "votecountnovotes",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &votecountnovotes
    },
    {
        "votedel",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        voteDelRun
    },
    {
        "voteminclients",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &voteminclients
    },
    {
        "votepasspercent",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &votepasspercent
    },
    {
        "vote_enable",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &vote_enable
    },
    {
        "vpn_api_key",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        &vpn_api_key
    },
    {
        "vpnusers",
        CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        vpnUsersRun,
    },
    {
        "vpn_enable",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &vpn_enable
    },
    {
        "vpn_kick",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &vpn_kick
    },
    {
        "zbc_enable",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &zbc_enable
    },
    {
        "zbc_jittermax",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &zbc_jittermax
    },
    {
        "zbc_jittermove",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &zbc_jittermove
    },
    {
        "zbc_jittertime",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &zbc_jittertime
    },
    {
        "zbotdetect",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &zbotdetect
    },
    {
        "zbotdetectactivetimeout",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &zbotdetectactivetimeout
    },
    {
        "zbotuserdisplay",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        zbotuserdisplay
    },
    /*{
        "client_check",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &client_check
    },*/
    {
        "client_map_cfg",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &client_map_cfg
    },
    {
        "client_msg",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        &client_msg
    },
    {
        "do_franck_check",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &do_franck_check
    },
    {
        "do_vid_restart",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &do_vid_restart
    },
    {
        "gl_driver_check",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &gl_driver_check
    },
    {
        "gl_driver_max_changes",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &gl_driver_max_changes
    },
    {
        "inverted_command1",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        &private_commands[4].command
    },
    {
        "inverted_command2",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        &private_commands[5].command
    },
    {
        "inverted_command3",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        &private_commands[6].command
    },
    {
        "inverted_command4",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        &private_commands[7].command
    },
    {
        "lanip",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        lanip
    },
    {
        "max_pmod_noreply",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &max_pmod_noreply
    },
    {
        "msec_timespan",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &(msec.timespan)
    },
    {
        "msec_max_violations",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &(msec.max_violations)
    },
    {
        "msec_min_required",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &(msec.min_required)
    },
    {
        "msec_action",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &(msec.action)
    },
    {
        "msec_max_allowed",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &(msec.max_allowed)
    },
    {
        "private_command1",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        &private_commands[0].command
    },
    {
        "private_command2",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        &private_commands[1].command
    },
    {
        "private_command3",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        &private_commands[2].command
    },
    {
        "private_command4",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        &private_commands[3].command
    },
    {
        "private_command_kick",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &private_command_kick
    },
    {
        "q2a_command_check",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &q2a_command_check
    },
    {
        "reloadloginfile",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        reloadLoginFileRun,
    },
    {
        "reloadwhoisfile",
        CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NONE,
        NULL,
        reloadWhoisFileRun,
    },
    {
        "serverip",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        serverip
    },
    {
        "speedbot_check_type",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &speedbot_check_type
    },
    {
        "timers_active",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_LOGICAL,
        &timers_active
    },
    {
        "timers_max_seconds",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &timers_max_seconds
    },
    {
        "timers_min_seconds",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &timers_min_seconds
    },
    {
        "userinfochange_count",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &USERINFOCHANGE_COUNT
    },
    {
        "userinfochange_time",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_NUMBER,
        &USERINFOCHANGE_TIME
    },
    /*{ 
        "version_check",
        CMDWHERE_CFGFILE | CMDWHERE_CLIENTCONSOLE | CMDWHERE_SERVERCONSOLE,
        CMDTYPE_STRING,
        &version_check
    },*/
    {
        "whois_active",
        CMDWHERE_CFGFILE, //Only allocates memory at InitGame: can only be read from config
        CMDTYPE_NUMBER,
        &whois_active
    },
};

//===================================================================
char mutedText[8192] = "";

/**
 *
 */
void Cmd_Teleport_f(edict_t *ent) {
    if (!(cloud.flags & RFL_TELEPORT)) {
        gi.cprintf(ent, PRINT_HIGH, "Teleport command is currently disabled.\n");
        return;
    }
    uint8_t id = getEntOffset(ent) - 1;
    CA_Teleport(id);
}

/**
 *
 */
void Cmd_Invite_f(edict_t *ent) {
    if (!(cloud.flags & RFL_INVITE)) {
            gi.cprintf(ent, PRINT_HIGH, "Invite command is currently disabled.\n");
            return;
    }
    char *invitetext;
    uint8_t id = getEntOffset(ent) - 1;
    if (gi.argc() > 1) {
            invitetext = gi.args();
    } else {
            invitetext = "";
    }
    CA_Invite(id, invitetext);
}

/**
 * dprintf is for printf-style formatted debug printing to the server console.
 *
 * Called when the real game library calls gi.dprintf(), does some stuff on
 * the input and then sends the result to the real dprintf() from the server.
 *
 * Checks if it's a chat mmessage (has player name prepended) and filters if
 * instructed to, checks for mutes, flood protection and also checks for and
 * filters non-printable characters if instructed to.
 */
void dprintf_internal(char *fmt, ...) {
    char cbuffer[8192];
    va_list arglist;
    int clienti = lastClientCmd;

    // convert to string
    va_start(arglist, fmt);
    Q_vsnprintf(cbuffer, sizeof(cbuffer), fmt, arglist);
    va_end(arglist);

    if (runmode == 0 || !proxyinfo) {
        gi.dprintf("%s", cbuffer);
        return;
    }

    if (clienti == -1) {
        unsigned int i;

        if (maxclients) {
            for (i = 0; i < maxclients->value; i++) {
                if (proxyinfo[i].inuse
                        && startContains(cbuffer, proxyinfo[i].name)) {
                    if (q2a_strstr(cbuffer, proxyinfo[i].lastcmd)) {
                        if (consolechat_disable) {
                            return;
                        }

                        clienti = i;
                        break;
                    }
                }
            }
        }
    } else if (((proxyinfo[clienti].inuse
            && !q2a_strstr(cbuffer, proxyinfo[clienti].name))
            || !q2a_strstr(cbuffer, proxyinfo[clienti].lastcmd))) {
        clienti = -1;
    } else if (consolechat_disable
            && q2a_strstr(cbuffer, proxyinfo[clienti].lastcmd)) {
        return;
    }

    if (clienti != -1) {
        if (checkForMute(clienti, getEnt((clienti + 1)), true)) {
            q2a_strncpy(mutedText, cbuffer, sizeof(mutedText));
            return;
        }

        mutedText[0] = 0;

        logEvent(LT_CHAT, clienti, getEnt((clienti + 1)), cbuffer, 0, 0.0, false);
    }

    if (filternonprintabletext) {
        char *cp = cbuffer;

        while (*cp) {
            if (!isprint(*cp) && *(cp + 1) != 0) {
                *cp = ' ';
            }

            cp++;
        }
    }
    gi.dprintf("%s", cbuffer);
    if (clienti != -1 && (floodinfo.chatFloodProtect || proxyinfo[clienti].floodinfo.chatFloodProtect)) {
        if (checkForFlood(clienti)) {
            return;
        }
    }
}

/**
 * cprintf is for printf-style formatted client printing. Used for printing
 * strings to the player's client (on screen and in the console. Supplying a
 * NULL edict_t arg will send the print to the server console instead of a
 * player.
 *
 * Called when the real game library calls gi.cprintf()
 *
 * ent          = the client entity to send the print to
 * printlevel   = how the client should handle it
 *   PRINT_HIGH for important stuff like chats (plays a sound, emphasized)
 *   PRINT_MED for obituaries
 *   PRINT_LOW for pickups
 * fmt          = the printf style format
 */
void cprintf_internal(edict_t *ent, int printlevel, char *fmt, ...) {
    char cbuffer[8192];
    va_list arglist;
    char *cp;
    int clienti = lastClientCmd;

    va_start(arglist, fmt);
    Q_vsnprintf(cbuffer, sizeof(cbuffer), fmt, arglist);
    va_end(arglist);

    if (runmode == 0) {
        gi.cprintf(ent, printlevel, "%s", cbuffer);
        return;
    }

    if (q2a_strcmp(mutedText, cbuffer) == 0) {
        return;
    }

    if (printlevel == PRINT_CHAT && clienti == -1) {
        unsigned int i;

        for (i = 0; i < maxclients->value; i++) {
            if (proxyinfo[i].inuse && startContains(cbuffer, proxyinfo[i].name)) {
                if (q2a_strstr(cbuffer, proxyinfo[i].lastcmd)) {
                    if (consolechat_disable) {
                        return;
                    }

                    clienti = i;
                    break;
                }
            }
        }
    }

    if (printlevel == PRINT_CHAT && clienti != -1 && consolechat_disable && q2a_strstr(cbuffer, proxyinfo[clienti].lastcmd)) {
        return;
    }

    if (printlevel == PRINT_CHAT && clienti != -1) {
        if (checkForMute(clienti, getEnt((clienti + 1)), (ent == NULL))) {
            return;
        }
    }

    if (printlevel == PRINT_CHAT && filternonprintabletext) {
        cp = cbuffer;

        while (*cp) {
            if (!isprint(*cp) && *(cp + 1) != 0) {
                *cp = ' ';
            }
            cp++;
        }
    }

    if (printlevel == PRINT_CHAT && ent == NULL) {
        if (clienti == -1) {
            logEvent(LT_CHAT, 0, 0, cbuffer, 0, 0.0, false);
        } else {
            logEvent(LT_CHAT, clienti, getEnt((clienti + 1)), cbuffer, 0, 0.0, false);

            chatpest_t *pest = &proxyinfo[clienti].pest;
            pest->printchars += strlen(cbuffer) - 1; // don't count the trailing \n
            pest->chatrate = pest->printchars / (ltime - proxyinfo[clienti].enteredgame);
            q2a_strncpy(pest->last[pest->last_index], cbuffer, MAX_CHAT_CHARS);
            if (pest->last_index == (MSG_SAVE_COUNT - 1)) {
                pest->last_index = -1;
            }
            pest->last_index++;
        }
    }

    // only works if we're a dedicated server
    if (ent == NULL) {
        CA_Print(printlevel, cbuffer);	// send the one for the server console
    }

    gi.cprintf(ent, printlevel, "%s", cbuffer);

    if (printlevel == PRINT_CHAT && clienti != -1 && ent == NULL && (floodinfo.chatFloodProtect || proxyinfo[clienti].floodinfo.chatFloodProtect)) {
        if (checkForFlood(clienti)) {
            return;
        }
    }
}

/**
 * bprintf is for printf-style formatted broadcast printing. It will send the
 * print string to all players connected to the server.
 *
 * Called when real game library calls gi.bprintf()
 *
 * printlevel   = how the client should handle it
 *   PRINT_HIGH for important stuff like chats (plays a sound, emphasized)
 *   PRINT_MED for obituaries
 *   PRINT_LOW for pickups
 * fmt          = the printf style format
 */
void bprintf_internal(int printlevel, char *fmt, ...) {
    char cbuffer[8192];
    va_list arglist;
    int clienti = lastClientCmd;

    // convert to string
    va_start(arglist, fmt);
    Q_vsnprintf(cbuffer, sizeof(cbuffer), fmt, arglist);
    va_end(arglist);
	
    if (runmode == 0) {
        gi.bprintf(printlevel, "%s", cbuffer);
        return;
    }

    // scrap obituaries for frags to send to remote admin server
    if (printlevel == PRINT_MEDIUM ||printlevel == PRINT_HIGH) {
        /**
         * There doesn't seem to be a way of getting the means-of-death
         * for a frag. I've hijacked all player entities' *die() function
         * pointer to capture when a frag happens, but it only give you
         * the victim and attacker edicts. Since edict_s is game-specific,
         * we can't even figure out what gun the attacker is holding at
         * frag-time because we don't know the offset for the weapon gitem_t
         * in the client structure.
         *
         * We can scrape the obituary here, but that requires comparing text,
         * which again is game-specific and could be anything. Not only that,
         * but by doing it here, we can't pinpoint the actual victim and attacker,
         * we have to match by comparing names, and if more than 1 player has
         * the same name (which is allowed) we'll never know who is who.
         *
         * So which is better? Getting a basic idea of means-of-death but
         * possibly not knowing who it applies to, or knowing definitely who
         * fragged who, but not how?
         *
         * -claire (Dec. 22, 2019)
         */
        CA_Print(printlevel, cbuffer);
    }

    if (q2a_strcmp(mutedText, cbuffer) == 0) {
        return;
    }

    if (printlevel == PRINT_CHAT && clienti == -1) {
        unsigned int i;

        for (i = 0; i < maxclients->value; i++) {
            if (proxyinfo[i].inuse && startContains(cbuffer, proxyinfo[i].name)) {
                if (q2a_strstr(cbuffer, proxyinfo[i].lastcmd)) {
                    clienti = i;
                    break;
                }
            }
        }
    }

    if (printlevel == PRINT_CHAT && clienti != -1) {
        if (checkForMute(clienti, getEnt((clienti + 1)), true)) {
            return;
        }
    }

    if (printlevel == PRINT_CHAT) {
        if (filternonprintabletext) {
            char *cp = cbuffer;

            while (*cp) {
                if (!isprint(*cp) && *(cp + 1) != 0) {
                    *cp = ' ';
                }
                cp++;
            }
        }

        if (clienti == -1) {
            logEvent(LT_CHAT, 0, 0, cbuffer, 0, 0.0, false);
        } else {
            logEvent(LT_CHAT, clienti, getEnt((clienti + 1)), cbuffer, 0, 0.0, false);
        }
    }

    gi.bprintf(printlevel, "%s", cbuffer);

    if (printlevel == PRINT_CHAT && clienti != -1 && (floodinfo.chatFloodProtect || proxyinfo[clienti].floodinfo.chatFloodProtect)) {
        if (checkForFlood(clienti)) {
            return;
        }
    }
}

/**
 * Add a command to execute to the server's command buffer.
 *
 * Called when the upstream game library calls gi.addcommandstring()
 *
 * Handles gamemap<->map command auto-conversion
 * Forces server to load map-specific configs before changing map
 *   `mapcfg/{mapname}-end.cfg`
 *   `{mapname}-pre.cfg`
 *
 */
void AddCommandString_internal(char *text) {
    char *str;
    bool mapChangeFound = false;

    if (runmode == 0) {
        gi.AddCommandString(text);
        return;
    }

    if (gamemaptomap) {
        // check for gamemap in string.
        q2a_strncpy(buffer, text, sizeof(buffer));
        q_strupr(buffer);

        str = q2a_strstr(buffer, "GAMEMAP");

        // double check the string is correct
        if (str && (str == buffer || *(str - 1) == ' ') && *(str + 7) == ' ') {
            // change to a map command
            if (str != buffer) {
                q2a_memcpy(buffer, text, str - buffer);
            }
            q2a_memcpy(str, text + ((str + 4) - buffer), q2a_strlen(text) - ((str + 4) - buffer) + 1);
            text = buffer;
        }
    }

    q2a_strncpy(buffer, text, sizeof(buffer));
    q_strupr(buffer);

    str = q2a_strstr(buffer, "GAMEMAP");
    if (str && (str == buffer || *(str - 1) == ' ') && *(str + 7) == ' ') {
        // gamemap found, find map name
        str += 7;
        while (*str == ' ') {
            str++;
        }
        if (*str == '\"') {
            str++;
            str = text + (str - buffer);
            mapChangeFound = true;
        }
    } else {
        str = q2a_strstr(buffer, "MAP");

        if (str && (str == buffer || *(str - 1) == ' ') && *(str + 3) == ' ') {
            // map found, find map name
            str += 3;
            while (*str == ' ') {
                str++;
            }
            if (*str == '\"') {
                str++;
                str = text + (str - buffer);
                mapChangeFound = true;
            }
        }
    }

    if (mapChangeFound) {
        if (mapcfgexec) {
            char *nameBuffer;

            q2a_strncpy(buffer, "exec mapcfg/", sizeof(buffer));
            q2a_strcat(buffer, gmapname);
            q2a_strcat(buffer, "-end.cfg\n");
            gi.AddCommandString(buffer);

            q2a_strncpy(buffer, "exec ", sizeof(buffer));
            nameBuffer = buffer + q2a_strlen(buffer);
            while (*str && *str != '\"') {
                *nameBuffer++ = *str++;
            }
            *nameBuffer = 0;
            q2a_strcat(buffer, "-pre.cfg\n");
            gi.AddCommandString(buffer);
        }

        // force all clients to report if map changes
        uint32_t i;
        for (i=0; i<cloud.maxclients; i++) {
            if (proxyinfo[i].inuse) {
                proxyinfo[i].remote_reported = 0;
            }
        }
    }
    gi.AddCommandString(text);
}


//===================================================================

char argtext[2048];

/**
 * Fetch the entire set of args as a single string, remove beginning and ending
 * quotes.
 *
 * Called by a bunch of funcs in g_cmd.c and a few in g_flood.c
 */
char *getArgs(void) {
    char *p;

    p = gi.args();
    q2a_strncpy(argtext, p, sizeof(argtext));
    p = argtext;
    if (*p == '"') {
        p++;
        p[q2a_strlen(p) - 1] = 0;
    }
    return p;
}

/**
 * Figure out what kind of args (string, numeric, boolean) a command has and
 * store it appropriately.
 *
 * Boolean values can be 1/0, yes/no, y/n (case insensitive)
 */
void processCommand(int cmdidx, int startarg, edict_t *ent) {
    // save value
    if (gi.argc() > startarg) {
        switch (q2aCommands[cmdidx].cmdtype) {
            case CMDTYPE_LOGICAL:
                *((bool *) q2aCommands[cmdidx].datapoint) = getLogicalValue(gi.argv(startarg));
                break;
            case CMDTYPE_NUMBER:
                *((int *) q2aCommands[cmdidx].datapoint) = q2a_atoi(gi.argv(startarg));
                break;
            case CMDTYPE_STRING:
                processstring(q2aCommands[cmdidx].datapoint, gi.argv(startarg), 255, 0);
                break;
        }
    }

	// show value
    switch (q2aCommands[cmdidx].cmdtype) {
        case CMDTYPE_LOGICAL:
            gi.cprintf(ent, PRINT_HIGH, "%s = %s\n", q2aCommands[cmdidx].cmdname, *((bool *) q2aCommands[cmdidx].datapoint) ? "Yes" : "No");
            break;
        case CMDTYPE_NUMBER:
            gi.cprintf(ent, PRINT_HIGH, "%s = %d\n", q2aCommands[cmdidx].cmdname, *((int *) q2aCommands[cmdidx].datapoint));
            break;
        case CMDTYPE_STRING:
            gi.cprintf(ent, PRINT_HIGH, "%s = %s\n", q2aCommands[cmdidx].cmdname, (char *) q2aCommands[cmdidx].datapoint);
            break;
    }
}

/**
 * Open, read and parse a q2admin config file.
 *
 * Only returns false if the file doesn't exist.
 */
bool readCfgFile(char *cfgfilename) {
    FILE *cfgfile;
    char buff1[256];
    char buff2[256];

    cfgfile = fopen(cfgfilename, "rt");
    if (!cfgfile) {
        return false;
    }

    while (fgets(buffer, 256, cfgfile) != NULL) {
        char *cp = buffer;
        SKIPBLANK(cp);
        if (!(cp[0] == ';' || cp[0] == '\n' || isBlank(cp))) {
            if (breakLine(cp, buff1, buff2, sizeof (buff2) - 1)) {
                unsigned int i;

                for (i = 0; i < lengthof(q2aCommands); i++) {
                    if ((q2aCommands[i].cmdwhere & CMDWHERE_CFGFILE) && startContains(q2aCommands[i].cmdname, buff1)) {
                        if (q2aCommands[i].initfunc) {
                            (*q2aCommands[i].initfunc)(buff2);
                        } else {
                            switch (q2aCommands[i].cmdtype) {
                                case CMDTYPE_LOGICAL:
                                    *((bool *) q2aCommands[i].datapoint) = getLogicalValue(buff2);
                                    break;
                                case CMDTYPE_NUMBER:
                                    *((int *) q2aCommands[i].datapoint) = q2a_atoi(buff2);
                                    break;
                                case CMDTYPE_STRING:
                                    q2a_strcpy(q2aCommands[i].datapoint, buff2);
                                    break;
                            }
                        }
                        break;
                    }
                }
            }
        }
    }
    fclose(cfgfile);
    return true;
}

/**
 * Reads the main config file. First from the q2 directory, then from the mod.
 * This way you can have general stuff at the top and mod specific stuff, assuming
 * you're running more than one server.
 *
 * Called in GetGameAPI() when the library is loaded
 *
 */
void readCfgFiles(void) {
    bool ret;

    ret = readCfgFile(q2aconfig->string);
    Q_snprintf(buffer, sizeof(buffer), "%s/%s", moddir, q2aconfig->string);
    if (readCfgFile(buffer)) {
        ret = true;
    }
    if (!ret) {
        gi.dprintf("Q2A: %s could not be found\n", q2aconfig->string);
    }
}

/**
 * Parse players out of command arguments. Can be player index or name,
 * multiple indexes are supported.
 *
 * Returns the number of players selected
 *
 * Actual players that have been selected have the CCMD_SELECTED bit set
 * on their clientcommand.
 *
 * Examples
 *   {some_command} CL {playernum1} [+ playernum2 [+ playernumN]] {other args}
 *   {some_command} {player_name} {other args}
 *   {some_command} "{player_name}" {other args}
 * {player_name} can be partial or wildcard (glob) matched, case insensitive.
 *
 * client   = index of player who issued the command
 * ent      = edict of the player who issued the command
 * cp       = the complete set of args from the command
 * text     = pointer to name parsed if not using CL
 */
int getClientsFromArg(int client, edict_t *ent, char *cp, char **text) {
    int8_t clienti;
    uint8_t like, maxi;
    char strbuffer[sizeof(buffer)];
    char strbuffer2[sizeof(buffer)];

    maxi = 0;

    if (startContains(cp, "CL")) {
        like = 3;

        cp += 2;
        SKIPBLANK(cp);

        if (!isdigit(*cp)) {
            return 0;
        }

        // un-select all players in case they were left from some other cmd
        for (clienti = 0; clienti < maxclients->value; clienti++) {
            proxyinfo[clienti].clientcommand &= ~CCMD_SELECTED;
        }

        if (isdigit(*cp)) {
            while (*cp) {
                clienti = q2a_atoi(cp);

                if (clienti >= 0 && clienti < maxclients->value && proxyinfo[clienti].inuse) {
                    proxyinfo[clienti].clientcommand |= CCMD_SELECTED;
                    maxi++;
                }

                while (isdigit(*cp)) {
                    cp++;
                }

                SKIPBLANK(cp);

                if (*cp && *cp != '+') {
                    break;
                }

                if (*cp == '+') {
                    cp++;
                }

                SKIPBLANK(cp);

                if (*cp && !isdigit(*cp)) {
                    break;
                }
            }
        }

        SKIPBLANK(cp);
    } else {
        like = 0;

        if (*cp == '\"') {
            cp++;
            cp = processstring(strbuffer, cp, sizeof (strbuffer), '\"');
            cp++;
        } else {
            cp = processstring(strbuffer, cp, sizeof (strbuffer), ' ');
        }
        SKIPBLANK(cp);
    }

    if (like < 3) {
        for (clienti = 0; clienti < maxclients->value; clienti++) {
            proxyinfo[clienti].clientcommand &= ~CCMD_SELECTED;

            if (proxyinfo[clienti].inuse) {
                switch (like) {
                    case 0:
                        q2a_strncpy(strbuffer2, strbuffer, sizeof(strbuffer2) - 1);
                        if (wildcard_match(strbuffer2, proxyinfo[clienti].name)) {
                            maxi++;
                            proxyinfo[clienti].clientcommand |= CCMD_SELECTED;
                        } else if (Q_stricmp(proxyinfo[clienti].name, strbuffer) == 0) {
                            maxi++;
                            proxyinfo[clienti].clientcommand |= CCMD_SELECTED;
                        }
                        break;

                    case 1:
                        if (stringContains(proxyinfo[clienti].name, strbuffer)) {
                            maxi++;
                            proxyinfo[clienti].clientcommand |= CCMD_SELECTED;
                        }
                        break;
                }
            }
        }
    }

    if (maxi) {
        *text = cp;
        return maxi;
    } else {
        gi.cprintf(ent, PRINT_HIGH, "no player name matches found.\n");
    }

    return 0;
}

/**
 * Resolve a single player edict from command args.
 *
 * Either "CL {number}" or "name"
 */
edict_t *getClientFromArg(int client, edict_t *ent, int *clientret, char *cp, char **text) {
    int8_t clienti, foundclienti;
    uint8_t like, matchcount;
    char strbuffer[sizeof(buffer)];
    char strbuffer2[sizeof(buffer)];

    foundclienti = -1;
    matchcount = 0;

    if (startContains(cp, "CL ")) {
        like = 3;

        cp += 2;
        SKIPBLANK(cp);

        if (!isdigit(*cp)) {
            return NULL;
        }

        foundclienti = q2a_atoi(cp);

        while (isdigit(*cp)) {
            cp++;
        }

        SKIPBLANK(cp);

        if (foundclienti < 0 || foundclienti > maxclients->value || !proxyinfo[foundclienti].inuse) {
            foundclienti = -1;
        }
    } else {
        like = 0;

        if (*cp == '\"') {
            cp++;
            cp = processstring(strbuffer, cp, sizeof (strbuffer), '\"');
            cp++;
        } else {
            cp = processstring(strbuffer, cp, sizeof (strbuffer), ' ');
        }
        SKIPBLANK(cp);
    }

    if (like < 3) {
        for (clienti = 0; clienti < maxclients->value; clienti++) {
            if (proxyinfo[clienti].inuse) {
                switch (like) {
                    case 0:
                        q2a_strncpy(strbuffer2, strbuffer, sizeof(strbuffer2) - 1);
                        if (wildcard_match(strbuffer2, proxyinfo[clienti].name)) {
                            foundclienti = clienti;
                            matchcount++;
                            if (matchcount > 1) {
                                gi.cprintf(ent, PRINT_HIGH, "2 or more player names matched.\n");
                                return NULL;
                            }
                        } else if (Q_stricmp(proxyinfo[clienti].name, strbuffer) == 0) {
                            if (foundclienti != -1) {
                                gi.cprintf(ent, PRINT_HIGH, "2 or more player name matches.\n");
                                return NULL;
                            }
                            foundclienti = clienti;
                        }
                        break;
                    case 1:
                        if (stringContains(proxyinfo[clienti].name, strbuffer)) {
                            if (foundclienti != -1) {
                                gi.cprintf(ent, PRINT_HIGH, "2 or more player name matches.\n");
                                return NULL;
                            }
                            foundclienti = clienti;
                        }
                        break;
                }
            }
        }
    }
    if (foundclienti != -1) {
        *text = cp;
        *clientret = foundclienti;
        return getEnt((foundclienti + 1));
    } else {
        gi.cprintf(ent, PRINT_HIGH, "no player name matches found.\n");
    }
    return NULL;
}

/**
 * Called when a player issues "say_person" command if extendedsay_enabled is true
 *
 */
bool sayPersonCmd(edict_t *ent, int client, char *args) {
    char *cp = args, *text;
    edict_t *enti;
    int clienti;
    char tmptext[2100];

    SKIPBLANK(cp);
    enti = getClientFromArg(client, ent, &clienti, cp, &text);
    if (enti) {
        // make sure the text doesn't overflow the internal buffer...
        if (q2a_strlen(text) > 2000) {
            text[2000] = 0;
        }

        // check for banned chat words
        if (checkCheckIfChatBanned(text)) {
            gi.cprintf(NULL, PRINT_HIGH, "%s: %s\n", proxyinfo[client].name, currentBanMsg);
            gi.cprintf(ent, PRINT_HIGH, "%s\n", currentBanMsg);
            logEvent(LT_CHATBAN, getEntOffset(ent) - 1, ent, text, 0, 0.0, false);
            return false;
        }

        Q_snprintf(
                tmptext,
                sizeof(tmptext),
                "(%s)(private message to: %s) %s\n",
                proxyinfo[client].name,
                proxyinfo[clienti].name,
                text
        );
        cprintf_internal(NULL, PRINT_CHAT, "%s", tmptext);
        cprintf_internal(ent, PRINT_CHAT, "%s", tmptext);

        Q_snprintf(
                tmptext,
                sizeof(tmptext),
                "(%s)(private message) %s\n",
                proxyinfo[client].name,
                text
        );
        cprintf_internal(enti, PRINT_CHAT, "%s", tmptext);
        return false;
    }
    return true;
}

/**
 * Send a message to a particular player as a PRINT_LOW, like a pickup message.
 * Doesn't highlight or play a sound
 */
void sayPersonLowRun(int startarg, edict_t *ent, int client) {
    char *text;
    edict_t *enti;
    int clienti;
    char tmptext[MAX_STRING_CHARS];

    // skip the first part (!say_xxx)
    text = getArgs();

    if (!ent) {
        while (*text != ' ') {
            text++;
        }
    }
    SKIPBLANK(text);
    enti = getClientFromArg(client, ent, &clienti, text, &text);

    // make sure the text doesn't overflow the internal buffer...
    if (enti) {
        text[MAX_STRING_CHARS - 40] = 0;

        // for server console
        Q_snprintf(tmptext, sizeof(tmptext), "%s-> %s\n", proxyinfo[clienti].name, text);
        cprintf_internal(NULL, PRINT_LOW, "%s", tmptext);

        // for client
        Q_snprintf(tmptext, sizeof(tmptext), "%s\n", text);
        cprintf_internal(enti, PRINT_LOW, "%s", tmptext);
    } else {
        gi.cprintf(ent, PRINT_HIGH, "[sv] !say_person_low [CL <id>]|name message\n");
    }
}

/**
 *
 */
bool sayGroupCmd(edict_t *ent, int client, char *args) {
    char *cp = args, *text;
    edict_t *enti;
    int clienti;
    char tmptext[2100];
    int max;

    SKIPBLANK(cp);
    max = getClientsFromArg(client, ent, cp, &text);
    if (max) {
        // make sure the text doesn't overflow the internal buffer...
        if (q2a_strlen(text) > 2000) {
            text[2000] = 0;
        }

        // check for banned chat words
        if (checkCheckIfChatBanned(text)) {
            // gi.cprintf(NULL, PRINT_HIGH, "%s: %s\n", proxyinfo[client].name, currentBanMsg);
            gi.cprintf(ent, PRINT_HIGH, "%s\n", currentBanMsg);
            logEvent(LT_CHATBAN, getEntOffset(ent) - 1, ent, text, 0, 0.0, true);
            return false;
        }

        for (clienti = 0; clienti < maxclients->value; clienti++) {
            if (proxyinfo[clienti].clientcommand & CCMD_SELECTED) {
                enti = getEnt((clienti + 1));

                Q_snprintf(
                        tmptext,
                        sizeof(tmptext),
                        "(%s)(private message to: %s) %s\n",
                        proxyinfo[client].name,
                        proxyinfo[clienti].name,
                        text
                );
                cprintf_internal(NULL, PRINT_CHAT, "%s", tmptext);
                cprintf_internal(ent, PRINT_CHAT, "%s", tmptext);

                Q_snprintf(
                        tmptext,
                        sizeof(tmptext),
                        "(%s)(private message) %s\n",
                        proxyinfo[client].name,
                        text
                );
                cprintf_internal(enti, PRINT_CHAT, "%s", tmptext);
            }
        }
        return false;
    }
    return true;
}

/**
 *
 */
void proxyDetected(edict_t *ent, int client) {
    proxyinfo[client].charindex = -6;
    removeClientCommand(client, QCMD_TESTRATBOT2);
    removeClientCommand(client, QCMD_ZPROXYCHECK2);
    serverLogZBot(ent, client);

    proxyinfo[client].clientcommand &= ~(CCMD_RATBOTDETECT | CCMD_ZPROXYCHECK2);
    proxyinfo[client].clientcommand |= CCMD_ZBOTDETECTED;

    if (displayzbotuser) {
        unsigned int i;

        q2a_strncpy(buffer, zbotuserdisplay, sizeof(buffer));
        q2a_strcat(buffer, "\n");

        for (i = 0; i < numofdisplays; i++) {
            gi.bprintf(PRINT_HIGH, buffer, proxyinfo[client].name);
        }
    }
    if (customClientCmd[0]) {
        addCmdQueue(client, QCMD_CUSTOM, 0, 0, 0);
    }
    if (disconnectuser) {
        addCmdQueue(client, QCMD_DISCONNECT, 1, 0, zbotuserdisplay);
    }
}

/**
 *
 */
void ratbotDetected(edict_t *ent, int client) {
    proxyinfo[client].charindex = -3;
    removeClientCommand(client, QCMD_TESTRATBOT2);
    removeClientCommand(client, QCMD_ZPROXYCHECK2);
    serverLogZBot(ent, client);

    proxyinfo[client].clientcommand &= ~(CCMD_RATBOTDETECT | CCMD_ZPROXYCHECK2);
    proxyinfo[client].clientcommand |= CCMD_ZBOTDETECTED;

    if (displayzbotuser) {
        unsigned int i;

        q2a_strncpy(buffer, zbotuserdisplay, sizeof(buffer));
        q2a_strcat(buffer, "\n");

        for (i = 0; i < numofdisplays; i++) {
            gi.bprintf(PRINT_HIGH, buffer, proxyinfo[client].name);
        }
    }
    if (customClientCmd[0]) {
        addCmdQueue(client, QCMD_CUSTOM, 0, 0, 0);
    }
    if (disconnectuser) {
        addCmdQueue(client, QCMD_DISCONNECT, 1, 0, zbotuserdisplay);
    }
}

/**
 *
 */
void timescaleDetected(edict_t *ent, int client) {
    proxyinfo[client].charindex = -5;
    removeClientCommand(client, QCMD_TESTRATBOT2);
    removeClientCommand(client, QCMD_ZPROXYCHECK2);
    serverLogZBot(ent, client);

    proxyinfo[client].clientcommand &= ~(CCMD_RATBOTDETECT | CCMD_ZPROXYCHECK2);
    proxyinfo[client].clientcommand |= CCMD_ZBOTDETECTED;

    if (displayzbotuser) {
        unsigned int i;

        q2a_strncpy(buffer, timescaleuserdisplay, sizeof(buffer));
        q2a_strcat(buffer, "\n");

        for (i = 0; i < numofdisplays; i++) {
            gi.bprintf(PRINT_HIGH, buffer, proxyinfo[client].name);
        }
    }

    if (customClientCmd[0]) {
        addCmdQueue(client, QCMD_CUSTOM, 0, 0, 0);
    }

    if (disconnectuser) {
        addCmdQueue(client, QCMD_DISCONNECT, 1, 0, timescaleuserdisplay);
    }
}

/**
 * A client has been determined to be illegitimate.
 */
void hackDetected(edict_t *ent, int client) {
    proxyinfo[client].charindex = -8;
    removeClientCommand(client, QCMD_TESTRATBOT2);
    removeClientCommand(client, QCMD_ZPROXYCHECK2);
    removeClientCommand(client, QCMD_TESTALIASCMD2);
    proxyinfo[client].clientcommand &= ~(CCMD_RATBOTDETECT | CCMD_ZPROXYCHECK2 | CCMD_WAITFORALIASREPLY1 | CCMD_WAITFORALIASREPLY2 | CCMD_WAITFORCONNECTREPLY);
    proxyinfo[client].clientcommand |= CCMD_ZBOTDETECTED;
    q2a_strncpy(buffer, hackuserdisplay, sizeof(buffer));
    q2a_strcat(buffer, "\n");
    gi.bprintf(PRINT_HIGH, buffer, proxyinfo[client].name);
    if (customClientCmd[0]) {
        addCmdQueue(client, QCMD_CUSTOM, 0, 0, 0);
    }
    if (disconnectuser) {
        addCmdQueue(client, QCMD_DISCONNECT, 1, 0, hackuserdisplay);
    }
}

/**
 * doClientCommand is called from ClientComand() for every command from
 * clients. The primary purpose of this func is to catch text from the client
 * and process it as part of the detection and control functionality.
 *
 * Q2admin does much of its work by stuffing commands to clients and then
 * processing the text responses here. If the response is part of the program's
 * logic, it will be handled here and not forwarded along to the real mod's
 * ClientCommand() function. Everything else gets passed along.
 *
 * Returns value:
 *   true  = pass the command along to the forward game library
 *   false = do not pass along
 */
bool doClientCommand(edict_t *ent, int client, bool *checkforfloodafter) {
    unsigned int i, cnt, sameip;
    char abuffer[256];
    char stemp[MAX_STRING_CHARS];
    char response[MAX_STRING_CHARS * 2];
    int alevel, slen;
    int q2a_admin_command = 0;
    char *cmd;
    char text[2048];

    if (client >= maxclients->value) {
        return false;
    }

    cmd = gi.argv(0);

    if (gi.argc() > 1) {
        q2a_strcpy(response, "");
        q2a_strcat(response, gi.args());
    } else {
        q2a_strncpy(response, cmd, sizeof(response));
    }

    if (*(rcon_password->string)) {
        if (q2a_strstr(response, rcon_password->string)) {
            snprintf(abuffer, sizeof (abuffer) - 1, "EXPLOIT - %s", response);
            abuffer[sizeof (abuffer) - 1] = 0;
            logEvent(LT_ADMINLOG, client, ent, abuffer, 0, 0.0, true);
            return false;
        }
    }

    if (Q_stricmp(cmd, zbot_str_q2start) == 0) {
        if (proxyinfo[client].inuse && (proxyinfo[client].clientcommand & CCMD_STARTUPTEST)) {
            proxyinfo[client].clientcommand &= ~CCMD_STARTUPTEST;
            removeClientCommand(client, QCMD_STARTUPTEST);
            proxyinfo[client].retries = 0;
            proxyinfo[client].rbotretries = 0;

            if (zbotdetect) {
                addCmdQueue(client, QCMD_RESTART, 1, 0, 0);
                stuffcmd(ent, "set msg 0 u\n");
                addCmdQueue(client, QCMD_LETRATBOTQUIT, 1, 0, 0);
                if (!(proxyinfo[client].clientcommand & CCMD_RBOTCLEAR)) {
                    addCmdQueue(client, QCMD_TESTRATBOT, 12, 0, 0);
                }
                if (!(proxyinfo[client].clientcommand & CCMD_NITRO2PROXY)) {
                    addCmdQueue(client, QCMD_TESTSTANDARDPROXY, 10, 0, 0);
                }
                if (!(proxyinfo[client].clientcommand & CCMD_ALIASCHECKSTARTED)) {
                    addCmdQueue(client, QCMD_TESTALIASCMD1, 1, 0, 0);
                }
            }
        }
        return false;
    } else if (proxyinfo[client].clientcommand & CCMD_WAITFORVERSION) {
        if (gi.argc() > 1) {
            if (Q_stricmp(cmd, proxyinfo[client].version_test) == 0) {
                proxyinfo[client].clientcommand &= ~CCMD_WAITFORVERSION;
                q2a_strncpy(
                        proxyinfo[client].client_version,
                        gi.args(),
                        sizeof(proxyinfo[client].client_version)
                );
                proxyinfo[client].version_deadline = 0;
                if (checkBanList(ent, client)) {
                    gi.cprintf(ent, PRINT_HIGH, "%s\n", currentBanMsg);
                    addCmdQueue(client, QCMD_DISCONNECT, 1, 0, currentBanMsg);
                }
                CA_PlayerConnect(ent);
                return false;
            }
        }
    } else if (proxyinfo[client].clientcommand & CCMD_ZPROXYCHECK2) { // check for proxy string
        if (!zbotdetect || !proxyinfo[client].inuse) {
            return false;
        }

        if (proxyinfo[client].teststr[0] && Q_stricmp(cmd, proxyinfo[client].teststr) == 0) {
            if (!proxyinfo[client].inuse) {
                return false;
            }

            // we have passed the test!!
            // get next char to test
            proxyinfo[client].charindex++;

            // check if it's a NITRO2 proxy client and skip the test for the '.' or ',' characters.
            while ((proxy_bwproxy == 2 || proxy_nitro2 == 2 || (proxyinfo[client].clientcommand & CCMD_NITRO2PROXY)) &&
                    (testchars[proxyinfo[client].charindex] == '.' || testchars[proxyinfo[client].charindex] == ',')) {
                proxyinfo[client].charindex++;
            }

            proxyinfo[client].clientcommand &= ~CCMD_ZPROXYCHECK2;
            removeClientCommand(client, QCMD_ZPROXYCHECK2);
            return false;
        } else if (Q_stricmp(cmd, zbot_str_please_disconnect) == 0) {
            return false;
        } else if (Q_stricmp(cmd, zbot_teststring_test2) == 0) {
            if (!zbotdetect || !proxyinfo[client].inuse || (proxyinfo[client].clientcommand & CCMD_ZBOTDETECTED)) {
                return false;
            }

            if (proxyinfo[client].retries < MAXDETECTRETRIES) {
                // try and get "unknown command" off the screen as fast as possible
                gi.cprintf(ent, PRINT_HIGH, "\n\n\n\n\n\n");

                proxyinfo[client].clientcommand &= ~CCMD_ZPROXYCHECK2;
                removeClientCommand(client, QCMD_ZPROXYCHECK2);
                addCmdQueue(client, QCMD_CLEAR, 0, 0, 0);
                addCmdQueue(client, QCMD_RESTART, 2 + (3 * random()), 0, 0);
                proxyinfo[client].retries++;
                return false;
            }

            serverLogZBot(ent, client);

            removeClientCommand(client, QCMD_ZPROXYCHECK2);
            addCmdQueue(client, QCMD_CLEAR, 0, 0, 0);
            proxyinfo[client].clientcommand |= CCMD_ZPROXYCHECK2;
            addCmdQueue(client, QCMD_ZPROXYCHECK2, (zbotdetectactivetimeout < 0) ? 5 + (randomwaitreporttime * random()) : zbotdetectactivetimeout, IW_ZBOTDETECT, 0);
            proxyinfo[client].clientcommand |= CCMD_ZBOTDETECTED;

            // try and get "unknown command" off the screen as fast as possible
            gi.cprintf(ent, PRINT_HIGH, "\n\n\n\n\n\n");
            return false;
        }
    } else if (Q_stricmp(cmd, zbot_str_please_disconnect) == 0) {
        return false;
    } else if (Q_stricmp(cmd, zbot_teststring_test2) == 0) { // check for end proxy string
        if (!proxyinfo[client].inuse) {
            return false;
        }

        // do we have more char's to check?
        if (zbotdetect && proxyinfo[client].charindex < testcharslength) {
            addCmdQueue(client, QCMD_ZPROXYCHECK1, 0, 0, 0);
        } else {
            proxyinfo[client].clientcommand |= CCMD_ZBOTCLEAR;
        }
        return false;
    }

    if (q2a_strcmp(cmd, proxyinfo[client].timescale_test_str) == 0) {
        if (!proxyinfo[client].inuse) {
            return false;
        }
        proxyinfo[client].timescale_deadline = 0;
        if (atoi(gi.argv(1)) != 1) {
            timescaleDetected(ent, client);
        } else {
            if (gi.argv(1)[1] == '.') {
                timescaleDetected(ent, client);
            }
        }
        return false;
    }

    if (q2a_strcmp(cmd, proxyinfo[client].hack_checkvar) == 0) {
        if (!proxyinfo[client].inuse) {
            return false;
        }
        int idx = proxyinfo[client].checkvar_idx;
        proxyinfo[client].checkvar_deadline[idx] = 0;
        checkVariableValid(ent, client, gi.argv(1));
        return false;
    }

    if (proxyinfo[client].clientcommand & CCMD_RATBOTDETECT) {
        if (Q_stricmp(cmd, "Please") == 0) {
            char *args = getArgs();

            if (Q_stricmp(args, "help me : What is a Bot ??") == 0) {
                ratbotDetected(ent, client);
                return false;
            }
        } else if (Q_stricmp(cmd, "Yeah") == 0) {
            char *args = getArgs();

            if (Q_stricmp(args, "!!! I am a R A T B O T !!!!! ??") == 0) {
                ratbotDetected(ent, client);
                return false;
            }
        }
    }

    // cmd ~= "q2start[0-9][0-9]" or
    // cmd ~= "q2e[0-9][0]9" or
    // cmd ~= ".FU[0-9][0-9]."
    if ((startContains(cmd, ZBOT_TESTSTRING_TEST1_OLD) && isdigit(cmd[7]) && isdigit(cmd[8]) && cmd[9] == 0) ||
            (startContains(cmd, ZBOT_TESTSTRING_TEST2_OLD) && isdigit(cmd[3]) && isdigit(cmd[4]) && cmd[5] == 0) ||
            (cmd[1] == BOTDETECT_CHAR1 && cmd[2] == BOTDETECT_CHAR2 && isdigit(cmd[3]) && isdigit(cmd[4]) && (cmd[7] == 0 || cmd[8] == 0))) {
        // clear retries just in case...
        proxyinfo[client].retries = 0;
        gi.dprintf("first one!\n");
        return false; //ignore because it's from a older level or something
    } else if (startContains(cmd, ZBOT_TESTSTRING_TEST1_OLD) || startContains(cmd, ZBOT_TESTSTRING_TEST2_OLD) ||
            (cmd[1] == BOTDETECT_CHAR1 && cmd[2] == BOTDETECT_CHAR2 && (cmd[7] == 0 || cmd[8] == 0))) {
        gi.dprintf("second one!\n");
        if (!zbotdetect || !proxyinfo[client].inuse) {
            return false;
        }

        Q_snprintf(
            text,
            sizeof(text),
            "I(%d) Cmd(%s) Exp(%s) (unexcepted cmd)",
            proxyinfo[client].charindex,
            cmd,
            proxyinfo[client].teststr
        );
        logEvent(LT_INTERNALWARN, client, ent, text, IW_UNEXCEPTEDCMD, 0.0, true);

        // clear retries just in case...
        proxyinfo[client].retries = 0;
        return false;
    } else if (cmd[1] == BOTDETECT_CHAR1 && cmd[2] == BOTDETECT_CHAR2) {
        gi.dprintf("third one!\n");
        if (!zbotdetect || !proxyinfo[client].inuse) {
            return false;
        }

        Q_snprintf(
            text,
            sizeof(text),
            "I(%d) Cmd(%s) Exp(%s) (unknown cmd)",
            proxyinfo[client].charindex,
            cmd,
            proxyinfo[client].teststr
        );
        logEvent(LT_INTERNALWARN, client, ent, text, IW_UNKNOWNCMD, 0.0, true);
    }

    if (proxyinfo[client].clientcommand & CCMD_WAITFORALIASREPLY1) {
        if (Q_stricmp(cmd, "alias") == 0) { // client doesn't support "alias" command, it just printed
            proxyinfo[client].clientcommand |= CCMD_ALIASCHECKSTARTED;
            hackDetected(ent, client);
            gi.dprintf("hackDetected() called near CCMD_WAITFORALIASREPLY1\n");
            return false;
        }

        if (proxyinfo[client].hacked_disconnect == 1) {
            sameip = 1;
            gi.dprintf("hacked_disconnect_addr: \"%s\"\n", net_addressToString(&proxyinfo[client].hacked_disconnect_addr, false, false, false));
            gi.dprintf("hacked_disconnect_addr: \"%s\"\n", net_addressToString(&proxyinfo[client].address, false, false, false));
            if (!net_addressesMatch(&proxyinfo[client].hacked_disconnect_addr, &proxyinfo[client].address)) {
                sameip = 0;
            }
            if (sameip == 1) {
                proxyinfo[client].hacked_disconnect = 0;
                hackDetected(ent, client);
                gi.dprintf("hackDetected() called near hacked_disconnect\n");
                return false;
            }
            proxyinfo[client].hacked_disconnect = 0;
        }

        // client doesn't send "rate" with userinfo
        if (proxyinfo[client].checked_hacked_exe == 0) {
            char *ratte = Info_ValueForKey(proxyinfo[client].userinfo.raw, "rate");
            proxyinfo[client].checked_hacked_exe = 1;
            if (*ratte == 0) {
                hackDetected(ent, client);
                gi.dprintf("hackDetected() called near rate check\n");
                return false;
            }
        }

    }

    if (proxyinfo[client].clientcommand & CCMD_WAITFORALIASREPLY2) {
        // alias cmd unsupported, it just printed the alias
        if (Q_stricmp(cmd, proxyinfo[client].alias_test_str1) == 0) {
            hackDetected(ent, client);
            gi.dprintf("hackDetected() called near CCMD_WAITFORALIASREPLY2\n");
            return false;
        }
        // client sent back the value of the alias, normal behavior.
        if (Q_stricmp(cmd, proxyinfo[client].alias_test_str2) == 0) {
            proxyinfo[client].clientcommand &= ~CCMD_WAITFORALIASREPLY2;
            proxyinfo[client].alias_deadline = 0;
            return false;
        }
    }

    if ((proxyinfo[client].clientcommand & CCMD_WAITFORCONNECTREPLY) &&
            Q_stricmp(cmd, proxyinfo[client].connect_test_str) == 0) {
        proxyinfo[client].clientcommand &= ~CCMD_WAITFORCONNECTREPLY;
        proxyinfo[client].hacked_disconnect = 1;
        proxyinfo[client].hacked_disconnect_addr = proxyinfo[client].address;
        return false;
    }

    if (gi.argc() > 1) {
        q2a_strncpy(proxyinfo[client].lastcmd, cmd, sizeof(proxyinfo[client].lastcmd));
        q2a_strcat(proxyinfo[client].lastcmd, " ");
        q2a_strcat(proxyinfo[client].lastcmd, gi.args());
    } else {
        q2a_strncpy(proxyinfo[client].lastcmd, cmd, sizeof(proxyinfo[client].lastcmd));
    }

    if (disablecmds_enable && checkDisabledCommand(proxyinfo[client].lastcmd)) {
        gi.cprintf(NULL, PRINT_HIGH, "%s: Tried to run disabled command: %s\n", proxyinfo[client].name, proxyinfo[client].lastcmd);
        logEvent(LT_DISABLECMD, getEntOffset(ent) - 1, ent, proxyinfo[client].lastcmd, 0, 0.0, true);
        return false;
    }

    if (Q_stricmp(cmd, "admin") == 0 || Q_stricmp(cmd, "referee") == 0 || Q_stricmp(cmd, "ref") == 0 || stringContains(cmd, "r_") == 1) {
        snprintf(abuffer, sizeof (abuffer) - 1, "REFEREE - %s: %s", cmd, gi.argv(1));
        abuffer[sizeof (abuffer) - 1] = 0;
        logEvent(LT_ADMINLOG, client, ent, abuffer, 0, 0.0, true);
    }

    if (proxyinfo[client].private_command > ltime) {
        for (i = 0; i < PRIVATE_COMMANDS; i++) {
            if (private_commands[i].command[0]) {
                if (Q_stricmp(proxyinfo[client].lastcmd, private_commands[i].command) == 0) {
                    proxyinfo[client].private_command_got[i] = true;
                    return false;
                }
            }
        }
    }

    if (Q_stricmp(cmd, "say") == 0 || Q_stricmp(cmd, "say_team") == 0 || Q_stricmp(cmd, "say_world") == 0) {
        q2a_strncpy(stemp, gi.args(), sizeof(stemp));
        slen = strlen(stemp);
        cnt = 0;

        for (i = 0; i < slen; i++) {
            if (stemp[i] == '%') {
                cnt++;
            }
        }

        if (cnt > 5) {
            return false;
        }

        // this check is for non standard p_Ver/p_mod replies. check return
        // string to match q2ace response
        if (proxyinfo[client].cmdlist_timeout > ltime) {
            if (q2a_strstr(gi.args(), "BLOCKED_MODEL")) {
                i = atoi(gi.argv(5));
                if (i != proxyinfo[client].blocklist) {
                    gi.bprintf(PRINT_HIGH, MOD_KICK_MSG, proxyinfo[client].name, i);
                    addCmdQueue(client, QCMD_DISCONNECT, 1, 0, Q2A_MOD_KICK_MSG);
                } else {
                    if (i < 0 || i >= MAX_BLOCK_MODELS) {
                        gi.bprintf(PRINT_HIGH, MOD_KICK_MSG, proxyinfo[client].name, i);
                        addCmdQueue(client, QCMD_DISCONNECT, 1, 0, Q2A_MOD_KICK_MSG);
                    } else {
                        if (q2a_strcmp(block_models[i].model_name, gi.argv(6))) {
                            gi.bprintf(PRINT_HIGH, MOD_KICK_MSG, proxyinfo[client].name, 256);
                            addCmdQueue(client, QCMD_DISCONNECT, 1, 0, Q2A_MOD_KICK_MSG);
                        } else {
                            proxyinfo[client].cmdlist |= 2;
                        }
                    }
                }
                return false;
            } else if (q2a_strstr(gi.args(), "SERVERIP")) {
                if (!*serverip) {
                    proxyinfo[client].cmdlist |= 4;
                    return false;
                }
                if (strcmp(gi.argv(5), proxyinfo[client].serverip) == 0) {
                    if (strcmp(gi.argv(6), serverip) == 0) {
                        proxyinfo[client].cmdlist |= 4;
                    } else {
                        gi.bprintf(PRINT_HIGH, MOD_KICK_MSG, proxyinfo[client].name, 1);
                        addCmdQueue(client, QCMD_DISCONNECT, 1, 0, Q2A_MOD_KICK_MSG);
                    }
                } else {
                    gi.bprintf(PRINT_HIGH, MOD_KICK_MSG, proxyinfo[client].name, proxyinfo[client].pmod);
                    addCmdQueue(client, QCMD_DISCONNECT, 1, 0, Q2A_MOD_KICK_MSG);
                }
                return false;
            }
        }

        if (proxyinfo[client].pmodver > ltime) {
            if (gl_driver_check & 1) {
                if (q2a_strstr(gi.args(), "Q2ADMIN_GL_DRIVER_CHECK")) {
                    if (strlen(proxyinfo[client].gl_driver)) {
                        if (strcmp(proxyinfo[client].gl_driver, gi.args()) == 0) {
                            // they match, ignore
                        } else {
                            q2a_strncpy(proxyinfo[client].gl_driver, gi.args(), sizeof(proxyinfo[client].gl_driver));
                            proxyinfo[client].gl_driver_changes++;
                            gi.cprintf(NULL, PRINT_HIGH, "%s %s\n", proxyinfo[client].name, gi.args());
                        }
                    } else {
                        q2a_strncpy(proxyinfo[client].gl_driver, gi.args(), sizeof(proxyinfo[client].gl_driver));
                        proxyinfo[client].gl_driver_changes++;
                        gi.cprintf(NULL, PRINT_HIGH, "%s %s\n", proxyinfo[client].name, gi.args());
                    }

                    if (gl_driver_max_changes) {
                        if (proxyinfo[client].gl_driver_changes > gl_driver_max_changes) {
                            addCmdQueue(client, QCMD_DISCONNECT, 1, 0, "Too many gl_driver changes.");
                        }
                    }
                    return false;
                }
            }
        }
    }

    if (Q_stricmp(cmd, "say") == 0 || Q_stricmp(cmd, "say_team") == 0) {
        if (strcmp(gi.argv(1), "XANIA") == 0 || strcmp(gi.argv(1), "Nitro2") == 0) {
            if (proxy_nitro2) {
                proxyinfo[client].clientcommand |= CCMD_NITRO2PROXY;
            } else {
                proxyDetected(ent, client);
                return false;
            }
        }

        if (checkForMute(client, ent, true)) {
            return false;
        }

        if (extendedsay_enable) {
            char *args = getArgs();

            if (say_person_enable && startContains(args, "!p")) { // say_person
                if (sayPersonCmd(ent, client, args + 2)) {
                    gi.cprintf(ent, PRINT_HIGH, "say !p [LIKE/CL] name message\n");
                }
                return false;
            } else if (say_group_enable && startContains(args, "!g")) { // say_group
                if (sayGroupCmd(ent, client, args + 2)) {
                    gi.cprintf(ent, PRINT_HIGH, "say !g [LIKE/CL] name message\n");
                }
                return false;
            }
        }
    } else if (checkforfloodcmds(cmd)) {
        if (checkForMute(client, ent, true)) {
            return false;
        }
        *checkforfloodafter = true;
    }

    if (cmd[0] == '!') {
        if (proxyinfo[client].admin_level) {
            q2a_admin_command = doAdminCommand(ent, client);
            if (q2a_admin_command) {
                return false;
            }
        }

        if (Q_stricmp(cmd, "!admin") == 0) {
            if (num_admins) {
                if (gi.argc() != 3) {
                    return false;
                }
                alevel = getAdminLevel(gi.argv(2), gi.argv(1));
                if (alevel) {
                    Q_snprintf(abuffer, sizeof(abuffer), "ADMIN - %s %s %d", gi.argv(2), gi.argv(1), alevel);
                    logEvent(LT_ADMINLOG, client, ent, abuffer, 0, 0.0, true);
                    proxyinfo[client].admin_level = alevel;
                    gi.cprintf(ent, PRINT_HIGH, "\nAdmin mode actived:\n");
                    listAdminCommands(ent, client);
                }
            }
            return false;
        } else if (Q_stricmp(cmd, "!bypass") == 0) {
            if (num_bypasses) {
                if (gi.argc() != 3) {
                    return false;
                }
                alevel = getBypassLevel(gi.argv(2), gi.argv(1));
                if (alevel) {
                    Q_snprintf(abuffer, sizeof(abuffer), "CLIENT BYPASS - %s %s %d", gi.argv(2), gi.argv(1), alevel);
                    logEvent(LT_ADMINLOG, client, ent, abuffer, 0, 0.0, true);
                    proxyinfo[client].bypass_level = alevel;
                }
            }
            return false;
        } else if (!proxyinfo[client].admin) {
            if (Q_stricmp(cmd, "!version") == 0) {
                gi.cprintf(ent, PRINT_HIGH, "Q2Admin Version %s\n", version);
                return false;
            } else if (adminpassword[0] && Q_stricmp(cmd, "!setadmin") == 0) {
                if (gi.argc() != 2) {
                    return false;
                }
                if (q2a_strcmp(gi.argv(1), adminpassword) == 0) {
                    proxyinfo[client].admin = 1;
                    gi.cprintf(ent, PRINT_HIGH, "\nAdmin password set.\n");
                }
                return false;
            }
        } else if (adminpassword[0] && proxyinfo[client].admin) {
            for (i = 0; i < lengthof(q2aCommands); i++) {
                if ((q2aCommands[i].cmdwhere & CMDWHERE_CLIENTCONSOLE) && startContains(q2aCommands[i].cmdname, cmd + 1)) {
                    if (q2aCommands[i].runfunc) {
                        (*q2aCommands[i].runfunc)(1, ent, client);
                    } else {
                        processCommand(i, 1, ent);
                    }
                    return false;
                }
            }

            gi.cprintf(ent, PRINT_HIGH, "Unknown q2admin command!\n");
            return false;
        }
    } else if (say_person_enable && Q_stricmp(cmd, "say_person") == 0) {
        if (checkForMute(client, ent, true)) {
            return false;
        }
        if (sayPersonCmd(ent, client, getArgs())) {
            gi.cprintf(ent, PRINT_HIGH, "say_person [CL <id>]|name message\n");
        }
        return false;
    } else if (say_group_enable && Q_stricmp(cmd, "say_group") == 0) {
        if (checkForMute(client, ent, true)) {
            return false;
        }
        if (sayGroupCmd(ent, client, getArgs())) {
            gi.cprintf(ent, PRINT_HIGH, "say_group [CL <id>]|name message\n");
        }
        return false;
    } else if (maxlrcon_cmds && rconpassword->string[0] && Q_stricmp(cmd, "lrcon") == 0) {
        run_lrcon(ent, client);
        return false;
    } else if (vote_enable && Q_stricmp(cmd, clientVoteCommand) == 0) {
        run_vote(ent, client);
        return false;
    } else if (Q_stricmp(cmd, "showfps") == 0) {
        proxyinfo[client].show_fps = !proxyinfo[client].show_fps;
        gi.cprintf(ent, PRINT_HIGH, "FPS Display %s\n", proxyinfo[client].show_fps ? "on" : "off");
        return false;
    } else if (Q_stricmp(cmd, "whois") == 0) {
        if (whois_active) {
            whois(client, ent);
            return false;
        }
    } else if (Q_stricmp(cmd, "timer_start") == 0) {
        if (timers_active) {
            timer_start(client, ent);
            return false;
        }
    } else if (Q_stricmp(cmd, "timer_stop") == 0) {
        if (timers_active) {
            timer_stop(client, ent);
            return false;
        }
    } else if (motdFilename[0] && Q_stricmp(cmd, "motd") == 0) {
        gi.centerprintf(ent, motd);
        return false;
    }

    if (checkCheckIfChatBanned(proxyinfo[client].lastcmd)) {
        gi.cprintf(ent, PRINT_HIGH, "%s\n", currentBanMsg);
        logEvent(LT_CHATBAN, getEntOffset(ent) - 1, ent, proxyinfo[client].lastcmd, 0, 0.0, true);
        return false;
    }
    logEvent(LT_CLIENTCMDS, client, ent, proxyinfo[client].lastcmd, 0, 0.0, true);
    return true;
}

/**
 *
 */
void cl_pitchspeed_enableRun(int startarg, edict_t *ent, int client) {
    if (gi.argc() > startarg) {
        bool newcl_pitchspeed_enable = getLogicalValue(gi.argv(startarg));
        int clienti;

        if (newcl_pitchspeed_enable && !cl_pitchspeed_enable) {
            cl_pitchspeed_enable = newcl_pitchspeed_enable;

            // check and set each client...
            for (clienti = 0; clienti < maxclients->value; clienti++) {
                if (proxyinfo[clienti].userinfo.rate > maxrateallowed) {
                    addCmdQueue(client, QCMD_SETUPCL_PITCHSPEED, 0, 0, 0);
                }
            }
        } else {
            cl_pitchspeed_enable = newcl_pitchspeed_enable;
        }
    }
    gi.cprintf(ent, PRINT_HIGH, "cl_pitchspeed_enable = %s\n", cl_pitchspeed_enable ? "Yes" : "No");
}

/**
 *
 */
void ClientCommand(edict_t *ent) {
    char *cmd;
    cmd = gi.argv(0);

    int clientnum = getEntOffset(ent) - 1;
    bool checkforfloodafter = false;
    char stemp[MAX_STRING_CHARS];

    profile_init(1);
    profile_init(2);

    if (!dllloaded) {
        return;
    }

    if (runmode == 0) {
        ge_mod->ClientCommand(ent);
        G_MergeEdicts();
        return;
    }

    profile_start(1);

    q2a_strcpy(stemp, "");
    q2a_strcat(stemp, gi.args());

    if (Q_stricmp(cmd, cloud_cmd_teleport) == 0) {
        Cmd_Teleport_f(ent);
        return;
    }

    if (Q_stricmp(cmd, cloud_cmd_invite) == 0) {
        Cmd_Invite_f(ent);
        return;
    }

    if (Q_stricmp(cmd, cloud_cmd_seen) == 0) {
        return;
    }

    if (Q_stricmp(cmd, cloud_cmd_whois) == 0) {
        return;
    }

    //Custom frkq2 check
    if ((do_franck_check) && (
            (stringContains(stemp, "riconnect")) ||
            (stringContains(cmd, "riconnect")) ||
            (stringContains(stemp, "roconnect")) || //Extra check for zgh-frk patch
            (stringContains(cmd, "roconnect")))) {
        return;
    }

    lastClientCmd = clientnum;
    if (doClientCommand(ent, clientnum, &checkforfloodafter)) {
        if (!(proxyinfo[clientnum].clientcommand & BANCHECK)) {
            profile_start(2);
            ge_mod->ClientCommand(ent);
            profile_stop(2, "mod->ClientCommand", 0, NULL);

            G_MergeEdicts();
        }
    }

    if (checkforfloodafter) {
        checkForFlood(clientnum);
    }
    lastClientCmd = -1;
    profile_stop(1, "q2admin->ClientCommand", 0, NULL);
}

/**
 *
 */
bool doServerCommand(void) {
    char *cmd;
    uint8_t i;

    cmd = gi.argv(1);
    if (*cmd == '!') {
        for (i = 0; i < lengthof(q2aCommands); i++) {
            if ((q2aCommands[i].cmdwhere & CMDWHERE_SERVERCONSOLE) && startContains(q2aCommands[i].cmdname, cmd + 1)) {
                if (q2aCommands[i].runfunc) {
                    (*q2aCommands[i].runfunc)(2, NULL, -1);
                } else {
                    processCommand(i, 2, NULL);
                }
                return false;
            }
        }
        gi.cprintf(NULL, PRINT_HIGH, "Unknown q2admin command!\n");
        return false;
    }
    return true;
}

/**
 *
 */
void ServerCommand(void) {
    profile_init(1);
    profile_init(2);

    if (!dllloaded) {
        return;
    }

    if (runmode == 0) {
        ge_mod->ServerCommand();
        G_MergeEdicts();
        return;
    }

    profile_start(1);

    if (doServerCommand()) {
        profile_start(2);
        ge_mod->ServerCommand();
        profile_stop(2, "mod->ServerCommand", 0, NULL);

        G_MergeEdicts();
    }

    profile_stop(1, "q2admin->ServerCommand", 0, NULL);
}

/**
 *
 */
void clientsidetimeoutInit(char *arg) {
    clientsidetimeout = q2a_atoi(arg);
    if (clientsidetimeout < MINIMUMTIMEOUT) {
        clientsidetimeout = MINIMUMTIMEOUT;
    }
}

/**
 * Display q2admin's version
 */
void versionRun(int startarg, edict_t *ent, int client) {
    gi.cprintf(ent, PRINT_HIGH, "Q2Admin version %s\n", version);
}

/**
 *
 */
void clientsidetimeoutRun(int startarg, edict_t *ent, int client) {
    if (gi.argc() > startarg) {
        clientsidetimeout = q2a_atoi(gi.argv(startarg));
        if (clientsidetimeout < MINIMUMTIMEOUT) {
            clientsidetimeout = MINIMUMTIMEOUT;
        }
    }
    gi.cprintf(ent, PRINT_HIGH, "clientsidetimeout = %d\n", clientsidetimeout);
}

/**
 *
 */
void setadminRun(int startarg, edict_t *ent, int client) {
    gi.cprintf(ent, PRINT_HIGH, "You are already in q2admin's admin mode.\n");
}

/**
 *
 */
void maxrateallowedRun(int startarg, edict_t *ent, int client) {
    if (gi.argc() > startarg) {
        int newmaxrate = q2a_atoi(gi.argv(startarg));
        int clienti;

        if (newmaxrate && (!maxrateallowed || maxrateallowed > newmaxrate)) {
            maxrateallowed = newmaxrate;

            // check and set each client...
            for (clienti = 0; clienti < maxclients->value; clienti++) {
                if (proxyinfo[clienti].userinfo.rate > maxrateallowed) {
                    addCmdQueue(client, QCMD_CLIPTOMAXRATE, 0, 0, 0);
                }
            }
        } else {
            maxrateallowed = newmaxrate;
        }
    }

    if (maxrateallowed == 0) {
        gi.cprintf(ent, PRINT_HIGH, "maxrate disabled\n");
    } else {
        gi.cprintf(ent, PRINT_HIGH, "maxrate = %d\n", maxrateallowed);
    }
}

/**
 *
 */
void minrateallowedRun(int startarg, edict_t *ent, int client) {
    if (gi.argc() > startarg) {
        int newminrate = q2a_atoi(gi.argv(startarg));
        int clienti;

        if (newminrate && (!minrateallowed || minrateallowed > newminrate)) {
            minrateallowed = newminrate;

            // check and set each client...
            for (clienti = 0; clienti < maxclients->value; clienti++) {
                if (proxyinfo[clienti].userinfo.rate < minrateallowed) {
                    addCmdQueue(client, QCMD_CLIPTOMINRATE, 0, 0, 0);
                }
            }
        } else {
            minrateallowed = newminrate;
        }
    }

    if (minrateallowed == 0) {
        gi.cprintf(ent, PRINT_HIGH, "minrate disabled\n");
    } else {
        gi.cprintf(ent, PRINT_HIGH, "minrate = %d\n", minrateallowed);
    }
}

void cl_anglespeedkey_enableRun(int startarg, edict_t *ent, int client) {
    if (gi.argc() > startarg) {
        bool newcl_anglespeedkey_enable = getLogicalValue(gi.argv(startarg));
        int clienti;

        if (newcl_anglespeedkey_enable && !cl_anglespeedkey_enable) {
            cl_anglespeedkey_enable = newcl_anglespeedkey_enable;

            // check and set each client...
            for (clienti = 0; clienti < maxclients->value; clienti++) {
                if (proxyinfo[clienti].userinfo.rate > maxrateallowed) {
                    addCmdQueue(client, QCMD_SETUPCL_ANGLESPEEDKEY, 0, 0, 0);
                }
            }
        } else {
            cl_anglespeedkey_enable = newcl_anglespeedkey_enable;
        }
    }

    gi.cprintf(ent, PRINT_HIGH, "cl_anglespeedkey_enable = %s\n", cl_anglespeedkey_enable ? "Yes" : "No");
}

/**
 *
 */
void maxfpsallowedRun(int startarg, edict_t *ent, int client) {
    if (gi.argc() > startarg) {
        int oldmaxfps = maxfpsallowed;
        int clienti;

        maxfpsallowed = q2a_atoi(gi.argv(startarg));

        if (maxfpsallowed && (!oldmaxfps || oldmaxfps > maxfpsallowed)) {
            // check is greater than the maxfps setting...
            if (minfpsallowed && minfpsallowed > maxfpsallowed) {
                gi.cprintf(ent, PRINT_HIGH, "maxfps can't be less then minfps\n");
                maxfpsallowed = oldmaxfps;
                return;
            }

            // check and set each client...
            for (clienti = 0; clienti < maxclients->value; clienti++) {
                if (proxyinfo[clienti].userinfo.maxfps == 0) {
                    addCmdQueue(client, QCMD_SETUPMAXFPS, 0, 0, 0);
                } else if (proxyinfo[clienti].userinfo.maxfps > maxfpsallowed) {
                    addCmdQueue(client, QCMD_SETMAXFPS, 0, 0, 0);
                }
            }
        }
    }

    if (maxfpsallowed == 0) {
        gi.cprintf(ent, PRINT_HIGH, "maxfps disabled\n");
    } else {
        gi.cprintf(ent, PRINT_HIGH, "maxfps = %d\n", maxfpsallowed);
    }
}

/**
 *
 */
void minfpsallowedRun(int startarg, edict_t *ent, int client) {
    if (gi.argc() > startarg) {
        int oldminfps = minfpsallowed;
        int clienti;

        minfpsallowed = q2a_atoi(gi.argv(startarg));

        if (minfpsallowed && (!oldminfps || oldminfps > minfpsallowed)) {
            // check is greater than the maxfps setting...
            if (maxfpsallowed && minfpsallowed > maxfpsallowed) {
                gi.cprintf(ent, PRINT_HIGH, "minfps can't be greater then maxfps\n");
                minfpsallowed = oldminfps;
                return;
            }

            // check and set each client...
            for (clienti = 0; clienti < maxclients->value; clienti++) {
                if (proxyinfo[clienti].userinfo.maxfps == 0) {
                    addCmdQueue(client, QCMD_SETUPMAXFPS, 0, 0, 0);
                } else if (proxyinfo[clienti].userinfo.maxfps < minfpsallowed) {
                    addCmdQueue(client, QCMD_SETMINFPS, 0, 0, 0);
                }
            }
        }
    }

    if (minfpsallowed == 0) {
        gi.cprintf(ent, PRINT_HIGH, "minfps disabled\n");
    } else {
        gi.cprintf(ent, PRINT_HIGH, "minfps = %d\n", minfpsallowed);
    }
}

/**
 *
 */
void maxfpsallowedInit(char *arg) {
    maxfpsallowed = q2a_atoi(arg);
    if (minfpsallowed && maxfpsallowed && maxfpsallowed < minfpsallowed) {
        maxfpsallowed = minfpsallowed;
    }
}

/**
 *
 */
void minfpsallowedInit(char *arg) {
    minfpsallowed = q2a_atoi(arg);
    if (minfpsallowed && maxfpsallowed && maxfpsallowed < minfpsallowed) {
        minfpsallowed = maxfpsallowed;
    }
}

/**
 *
 */
void impulsesToKickOnRun(int startarg, edict_t *ent, int client) {
    unsigned int i = 0;
    int impulses = 0;
    char *cp = gi.argv(startarg);

    impulses = q2a_atoi(&maxImpulses);
    if (Q_stricmp(cp, "ADD") == 0) {
        startarg++;
    } else if (Q_stricmp(cp, "RESET") == 0) {
        impulses = 0;
        startarg++;
    }

    while (startarg < gi.argc() && impulses < MAXIMPULSESTOTEST) {
        impulsesToKickOn[impulses] = q2a_atoi(gi.argv(startarg));
        impulses++;
        startarg++;
    }
    gi.cprintf(ent, PRINT_HIGH, "impulsestokickon = ");
    if (impulses) {
        gi.cprintf(ent, PRINT_HIGH, "%d", impulsesToKickOn[0]);
        for (i = 1; i < impulses; i++) {
            gi.cprintf(ent, PRINT_HIGH, ", %d", impulsesToKickOn[i]);
        }
        gi.cprintf(ent, PRINT_HIGH, "\n");
    } else {
        gi.cprintf(ent, PRINT_HIGH, "ALL\n");
    }
    maxImpulses = impulses;
}

/**
 *
 */
void impulsesToKickOnInit(char *arg) {
    while (*arg && maxImpulses < MAXIMPULSESTOTEST) {
        impulsesToKickOn[maxImpulses] = q2a_atoi(arg);
        maxImpulses++;
        while (*arg && *arg != ' ') {
            arg++;
        }
        SKIPBLANK(arg);
    }
}

/**
 * Load the MOTD content from disk
 */
void motdRun(int startarg, edict_t *ent, int client) {
    if (gi.argc() > startarg) {
        FILE *fp;
        int len, currentlen;

        processstring(motdFilename, gi.argv(startarg), sizeof(motdFilename), 0);
        fp = fopen(motdFilename, "rt");
        if (!fp) {
            gi.cprintf(ent, PRINT_HIGH, "MOTD file could not be opened\n");
        } else {
            motd[0] = 0;
            len = 0;
            while (fgets(buffer, 256, fp)) {
                currentlen = q2a_strlen(buffer);
                if (len + currentlen > sizeof(motd)) {
                    break;
                }
                len += currentlen;
                q2a_strcat(motd, buffer);
            }
            fclose(fp);
            gi.cprintf(ent, PRINT_HIGH, "MOTD Loaded\n");
        }
    } else {
        motdFilename[0] = 0;
        gi.cprintf(ent, PRINT_HIGH, "MOTD Cleared\n");
    }
}

/**
 *
 */
void stuffClientRun(int startarg, edict_t *ent, int client) {
    char *text;
    edict_t *enti;
    int clienti;

    // skip the first part (!stuff)
    text = getArgs();

    if (!ent) {
        while (*text != ' ') {
            text++;
        }
    }
    SKIPBLANK(text);
    enti = getClientFromArg(client, ent, &clienti, text, &text);

    if (enti) {
        SKIPBLANK(text);
        if (startContains(text, "FILE")) {
            text += 4;
            SKIPBLANK(text);
            if (proxyinfo[clienti].stuffFile) {
                gi.cprintf(ent, PRINT_HIGH, "Client already being stuffed... please wait\n");
                return;
            }
            processstring(buffer, text, sizeof (buffer) - 1, 0);
            proxyinfo[clienti].stuffFile = fopen(buffer, "rt");
            if (proxyinfo[clienti].stuffFile) {
                addCmdQueue(clienti, QCMD_STUFFCLIENT, 0, 0, 0);
                gi.cprintf(ent, PRINT_HIGH, "Stuffing client %d (%s)\n", clienti, proxyinfo[clienti].name);
            } else {
                gi.cprintf(ent, PRINT_HIGH, "Can't find stuff file\n");
            }
        } else {
            if (*text == '\"') {
                text++;
                processstring(buffer, text, sizeof (buffer) - 2, '\"');
            } else {
                processstring(buffer, text, sizeof (buffer) - 2, 0);
            }
            q2a_strcat(buffer, "\n");
            stuffcmd(enti, buffer);
            gi.cprintf(ent, PRINT_HIGH, "Command sent to client!\n");
        }
    } else {
        gi.cprintf(ent, PRINT_HIGH, "[sv] !stuff [CL <id>]|name [client commands | FILE <filename>]\n");
    }
}

/**
 *
 */
void stuffNextLine(edict_t *ent, int client) {
    if (!proxyinfo[client].stuffFile) {
        return;
    }

    if (fgets(buffer, sizeof (buffer), proxyinfo[client].stuffFile)) {
        q2a_strcat(buffer, "\n");
        stuffcmd(ent, buffer);
        addCmdQueue(client, QCMD_STUFFCLIENT, 0, 0, 0);
    } else {
        fclose(proxyinfo[client].stuffFile);
        proxyinfo[client].stuffFile = 0;
    }
}

/**
 *
 */
void sayGroupRun(int startarg, edict_t *ent, int client) {
    char *text;
    char tmptext[MAX_STRING_CHARS];
    edict_t *enti;
    int8_t clienti, max;

    // skip the first part (!say_xxx)
    text = getArgs();
    if (!ent) {
        while (*text != ' ') {
            text++;
        }
    }
    SKIPBLANK(text);
    max = getClientsFromArg(client, ent, text, &text);
    if (max) {
        if (q2a_strlen(text) > MAX_STRING_CHARS - 40) {
            text[MAX_STRING_CHARS - 40] = 0;
        }
        for (clienti = 0; clienti < maxclients->value; clienti++) {
            if (proxyinfo[clienti].clientcommand & CCMD_SELECTED) {
                enti = getEnt((clienti + 1));

                Q_snprintf(tmptext, sizeof(tmptext), "(private message to: %s) %s\n", proxyinfo[clienti].name, text);
                cprintf_internal(NULL, PRINT_CHAT, "%s", tmptext);
                if (ent) {
                    cprintf_internal(ent, PRINT_CHAT, "%s", tmptext);
                }

                Q_snprintf(tmptext, sizeof(tmptext), "(private message) %s\n", text);
                cprintf_internal(enti, PRINT_CHAT, "%s", tmptext);
            }
        }
    } else {
        gi.cprintf(ent, PRINT_HIGH, "[sv] !say_group [CL <id>]|name message\n");
    }
}

/**
 *
 */
void sayPersonRun(int startarg, edict_t *ent, int client) {
    char *text;
    edict_t *enti;
    int clienti;
    char tmptext[MAX_STRING_CHARS];

    // skip the first part (!say_xxx)
    text = getArgs();
    if (!ent) {
        while (*text != ' ') {
            text++;
        }
    }
    SKIPBLANK(text);
    enti = getClientFromArg(client, ent, &clienti, text, &text);

    // make sure the text doesn't overflow the internal buffer...
    if (enti) {
        Q_snprintf(tmptext, sizeof(tmptext), "(private message to: %s) %s\n", proxyinfo[clienti].name, text);
        cprintf_internal(NULL, PRINT_CHAT, "%s", tmptext);

        if (ent) {
            cprintf_internal(ent, PRINT_CHAT, "%s", tmptext);
        }

        Q_snprintf(tmptext, sizeof(tmptext), "(private message) %s\n", text);
        cprintf_internal(enti, PRINT_CHAT, "%s", tmptext);
    } else {
        gi.cprintf(ent, PRINT_HIGH, "[sv] !say_person [CL <id>]|name message\n");
    }
}

/**
 *
 */
void ipRun(int startarg, edict_t *ent, int client) {
    char *text;
    edict_t *enti;
    int clienti;
    char tmptext[100];

    // skip the first part (!ip)
    text = getArgs();
    if (!ent) {
        while (*text != ' ') {
            text++;
        }
    }
    SKIPBLANK(text);
    enti = getClientFromArg(client, ent, &clienti, text, &text);

    // make sure the text doesn't overflow the internal buffer...
    if (enti) {
        Q_snprintf(tmptext, sizeof(tmptext), "%s ip: %s\n", proxyinfo[clienti].name, IP(clienti));
        cprintf_internal(ent, PRINT_HIGH, "%s", tmptext);
    } else {
        gi.cprintf(ent, PRINT_HIGH, "[sv] !ip [CL <id>]|name\n");
    }
}

/**
 *
 */
void kickRun(int startarg, edict_t *ent, int client) {
    char *text;
    char tmptext[100];
    int clienti;
    int max;

    // skip the first part (!say_xxx)
    text = getArgs();
    if (!ent) {
        while (*text != ' ') {
            text++;
        }
    }
    SKIPBLANK(text);
    max = getClientsFromArg(client, ent, text, &text);
    if (max) {
        gi.AddCommandString("\n");
        for (clienti = 0; clienti < maxclients->value; clienti++) {
            if (proxyinfo[clienti].clientcommand & CCMD_SELECTED) {
                Q_snprintf(tmptext, sizeof(tmptext), "kick %d\n", clienti);
                gi.AddCommandString(tmptext);
            }
        }
    } else {
        gi.cprintf(ent, PRINT_HIGH, "[sv] !kick [CL] name\n");
    }
}

/**
 *
 */
void cvarsetRun(int startarg, edict_t *ent, int client) {
    char cbuffer[256];
    char *cvar = gi.argv(startarg);

    if (gi.argc() < startarg + 1) {
        gi.cprintf(ent, PRINT_HIGH, "[sv] !cvarset <cvarname> <value>\n");
    }
    processstring(cbuffer, gi.argv(startarg + 1), 255, 0);
    if (Q_stricmp(cbuffer, "none") == 0) {
        cbuffer[0] = 0;
    }
    gi.cvar_set(cvar, cbuffer);
    gi.cprintf(ent, PRINT_HIGH, "%s = %s\n", cvar, cbuffer);
}

/**
 *
 */
void lockDownServerRun(int startarg, edict_t *ent, int client) {
    if (gi.argc() > startarg) {
        lockDownServer = getLogicalValue(gi.argv(startarg));
    }
    gi.cprintf(ent, PRINT_HIGH, "lock = %s\n", lockDownServer ? "Yes" : "No");
    // clear all the reconnect user info...
    q2a_memset(reconnectproxyinfo, 0x0, maxclients->value * sizeof (proxyreconnectinfo_t));
}

/**
 * Lock a player in place. They'll still be able to change their angles, shoot,
 * talk shit, etc. They will not be able to move though.
 *
 * If an expiration is set, the player will automatically thaw at that time.
 */
void freezeRun(int startarg, edict_t *ent, int client) {
    int targeti, secs = 0;
    char *text;
    edict_t *target;

    text = getArgs();
    if (!ent) {
        while (*text != ' ') {
            text++;
        }
    }
    SKIPBLANK(text);
    target = getClientFromArg(client, ent, &targeti, text, &text);
    if (isdigit(*text)) {
        secs = q2a_atoi(text);
    }

    if (Q_stricmp(text, "help") == 0) {
        gi.cprintf(ent, PRINT_HIGH, "[sv] !freeze [CL <id>]|name [seconds]\n");
        return;
    }
    if (target == NULL) {
        return;
    }
    if (proxyinfo[targeti].freeze.frozen) {
        gi.cprintf(NULL, PRINT_HIGH, "%s is already frozen\n", NAME(targeti));
        return;
    }
    addCmdQueue(targeti, QCMD_FREEZEPLAYER, 0.0, secs, "You're frozen\n");
}

/**
 * Unlock a frozen player.
 *
 * If an expiration is set when freezing a player they will automatically thaw
 * without needing to call this function.
 */
void unfreezeRun(int startarg, edict_t *ent, int client) {
    int targeti;
    char *text;
    edict_t *target;

    text = getArgs();
    if (!ent) {
        while (*text != ' ') {
            text++;
        }
    }
    SKIPBLANK(text);
    target = getClientFromArg(client, ent, &targeti, text, &text);
    if (target == NULL) {
        return;
    }
    if (!proxyinfo[targeti].freeze.frozen) {
        gi.cprintf(NULL, PRINT_HIGH, "%s is not frozen\n", NAME(targeti));
        return;
    }
    addCmdQueue(targeti, QCMD_UNFREEZEPLAYER, 0.0, 0, "You thawed\n");
}

