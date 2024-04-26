/*
Copyright (C) 1997-2001 Id Software, Inc.

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

// CURL functionality for GET/POST requests on the fly. Borrowed from OpenTDM

#include "g_local.h"

dlhandle_t downloads[MAX_DOWNLOADS];

static CURLM                *multi = NULL;
static unsigned             handleCount = 0;
static char                 otdm_api_ip[16];
static char                 hostHeader[64];
static struct curl_slist    *http_header_slist;
static time_t               last_dns_lookup;

/**
 *
 */
void HandleDownload(download_t *download, char *buff, int len, int code) {
    if (!download->initiator) {
        gi.dprintf("HandleDownload: NULL initiator");
    }
    download->onFinish(download, code, (byte *)buff, len);
}

/**
 * Properly escapes a path with HTTP %encoding. libcurl's function seems to
 * treat '/' and such as illegal chars and encodes almost the entire URL...
 */
static void HTTP_EscapePath(const char *filePath, char *escaped) {
    int     i;
    size_t  len;
    char    *p;

    p = escaped;

    len = strlen(filePath);
    for (i = 0; i < len; i++) {
        if (!isalnum (filePath[i]) && filePath[i] != ';' && filePath[i] != '/' &&
            filePath[i] != '?' && filePath[i] != ':' && filePath[i] != '@' && filePath[i] != '&' &&
            filePath[i] != '=' && filePath[i] != '+' && filePath[i] != '$' && filePath[i] != ',' &&
            filePath[i] != '[' && filePath[i] != ']' && filePath[i] != '-' && filePath[i] != '_' &&
            filePath[i] != '.' && filePath[i] != '!' && filePath[i] != '~' && filePath[i] != '*' &&
            filePath[i] != '\'' && filePath[i] != '(' && filePath[i] != ')') {
            sprintf (p, "%%%02x", filePath[i]);
            p += 3;
        } else {
            *p = filePath[i];
            p++;
        }
    }
    p[0] = 0;

    //using ./ in a url is legal, but all browsers condense the path and some IDS / request
    //filtering systems act a bit funky if http requests come in with uncondensed paths.
    len = strlen(escaped);
    p = escaped;
    while ((p = strstr (p, "./"))) {
        q2a_memmove(p, p+2, len - (p - escaped) - 1);
        len -= 2;
    }
}

/**
 * libcurl callback
 */
static size_t HTTP_Recv(void *ptr, size_t size, size_t nmemb, void *stream) {
    dlhandle_t  *dl;
    size_t      new_size, bytes;

    dl = (dlhandle_t *)stream;

    if (!nmemb) {
        return 0;
    }

    if (size > SIZE_MAX / nmemb) {
        goto oversize;
    }

    if (dl->position > MAX_DLSIZE) {
        goto oversize;
    }

    bytes = size * nmemb;
    if (bytes >= MAX_DLSIZE - dl->position) {
        goto oversize;
    }

    //grow buffer in MIN_DLSIZE chunks. +1 for NUL.
    new_size = (dl->position + bytes + MIN_DLSIZE) & ~(MIN_DLSIZE - 1);
    if (new_size > dl->fileSize) {
        char        *tmp;

        tmp = dl->tempBuffer;
        dl->tempBuffer = gi.TagMalloc((int)new_size, TAG_GAME);
        if (tmp) {
            q2a_memcpy(dl->tempBuffer, tmp, dl->fileSize);
            gi.TagFree(tmp);
        }
        dl->fileSize = new_size;
    }

    q2a_memcpy(dl->tempBuffer + dl->position, ptr, bytes);
    dl->position += bytes;
    dl->tempBuffer[dl->position] = 0;

    return bytes;

oversize:
    gi.dprintf("Suspiciously large file while trying to download %s!\n", dl->URL);
    return 0;
}

/**
 * Troubleshooting
 */
int CURL_Debug(CURL *c, curl_infotype type, char *data, size_t size, void * ptr) {
    if (type == CURLINFO_TEXT) {
        char    buff[4096];
        if (size > sizeof(buff)-1) {
            size = sizeof(buff)-1;
        }
        Q_strncpy(buff, data, size);
        gi.dprintf("  HTTP DEBUG: %s", buff);
        if (!strchr(buff, '\n')) {
            gi.dprintf ("\n");
        }
    }
    return 0;
}

/**
 *
 */
void HTTP_ResolveVPNServer(void) {
    if (!http_enable) {
        return;
    }

    //re-resolve if its been more than one day since we last did it
    if (time(NULL) - last_dns_lookup > 86400) {
        gi.cprintf(NULL, PRINT_HIGH, "Resolving VPN API server %s -> ", vpn_host);
        struct hostent  *h;
        h = gethostbyname(vpn_host);

        if (!h) {
            otdm_api_ip[0] = '\0';
            gi.dprintf ("WARNING: Could not resolve VPN API server '%s'. HTTP functions unavailable.\n", vpn_host);
            return;
        }
        time(&last_dns_lookup);
        Q_strncpy(otdm_api_ip, inet_ntoa (*(struct in_addr *)h->h_addr_list[0]), sizeof(otdm_api_ip)-1);
        gi.cprintf(NULL, PRINT_HIGH, "%s\n", otdm_api_ip);
    }
}

/**
 * Actually starts a download by adding it to the curl multi handle.
 */
void HTTP_StartDownload(dlhandle_t *dl) {
    char escapedFilePath[1024*3];

    dl->tempBuffer = NULL;
    dl->speed = 0;
    dl->fileSize = 0;
    dl->position = 0;

    if (!dl->curl) {
        dl->curl = curl_easy_init();
    }

    // format: https://vpnapi.io/api/<ipaddress>?key=<apikey>
    snprintf(dl->URL, sizeof(dl->URL), "https://%s%s", vpn_host, dl->filePath);

    curl_easy_setopt(dl->curl, CURLOPT_HTTPHEADER, http_header_slist);
    curl_easy_setopt(dl->curl, CURLOPT_ENCODING, "");

    if (http_debug) {
        curl_easy_setopt(dl->curl, CURLOPT_DEBUGFUNCTION, CURL_Debug);
        curl_easy_setopt(dl->curl, CURLOPT_VERBOSE, 1);
    } else {
        curl_easy_setopt(dl->curl, CURLOPT_DEBUGFUNCTION, NULL);
        curl_easy_setopt(dl->curl, CURLOPT_VERBOSE, 0);
    }

    curl_easy_setopt(dl->curl, CURLOPT_NOPROGRESS, 1);
    curl_easy_setopt(dl->curl, CURLOPT_WRITEDATA, dl);
    curl_easy_setopt(dl->curl, CURLOPT_INTERFACE, NULL);
    curl_easy_setopt(dl->curl, CURLOPT_WRITEFUNCTION, HTTP_Recv);
    curl_easy_setopt(dl->curl, CURLOPT_PROXY, NULL);
    curl_easy_setopt(dl->curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(dl->curl, CURLOPT_MAXREDIRS, 5);
    curl_easy_setopt(dl->curl, CURLOPT_USERAGENT, "q2admin");
    curl_easy_setopt(dl->curl, CURLOPT_REFERER, "");
    curl_easy_setopt(dl->curl, CURLOPT_CAPATH, http_cacert_path);

    curl_easy_setopt(dl->curl, CURLOPT_URL, dl->URL);
    if (http_verifyssl) {
        curl_easy_setopt(dl->curl, CURLOPT_SSL_VERIFYPEER, 1);
    } else {
        curl_easy_setopt(dl->curl, CURLOPT_SSL_VERIFYPEER, 0);
    }


    if (curl_multi_add_handle(multi, dl->curl) != CURLM_OK) {
        gi.dprintf("HTTP_StartDownload: curl_multi_add_handle: error\n");
        return;
    }

    handleCount++;
}

/**
 *
 */
void HTTP_Init(void) {
    curl_global_init(CURL_GLOBAL_NOTHING);
    multi = curl_multi_init();
    snprintf(hostHeader, sizeof(hostHeader), "Host: %s", vpn_host);
    http_header_slist = curl_slist_append(http_header_slist, hostHeader);
    gi.dprintf("%s initialized.\n", curl_version());
}

/**
 *
 */
void HTTP_Shutdown(void) {
    if (multi) {
        curl_multi_cleanup(multi);
        multi = NULL;
    }
    curl_slist_free_all(http_header_slist);
    curl_global_cleanup();
}

/**
 * A download finished, find out what it was, whether there were any errors and
 * if so, how severe. If none, rename file and other such stuff.
 */
static void HTTP_FinishDownload(void) {
    int         msgs_in_queue;
    CURLMsg     *msg;
    CURLcode    result;
    dlhandle_t  *dl;
    CURL        *curl;
    long        responseCode;
    double      timeTaken;
    double      fileSize;
    unsigned    i;

    do {
        msg = curl_multi_info_read(multi, &msgs_in_queue);

        if (!msg) {
            gi.dprintf("HTTP_FinishDownload: Odd, no message for us...\n");
            return;
        }

        if (msg->msg != CURLMSG_DONE) {
            gi.dprintf("HTTP_FinishDownload: Got some weird message...\n");
            continue;
        }

        curl = msg->easy_handle;

        for (i = 0; i < MAX_DOWNLOADS; i++) {
            if (downloads[i].curl == curl) {
                break;
            }
        }

        if (i == MAX_DOWNLOADS) {
            gi.dprintf("HTTP_FinishDownload: Handle not found!\n");
        }

        dl = &downloads[i];

        result = msg->data.result;

        switch (result) {
            //for some reason curl returns CURLE_OK for a 404...
            case CURLE_HTTP_RETURNED_ERROR:
            case CURLE_OK:

                curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &responseCode);
                if (responseCode == 404) {
                    HandleDownload(dl->handle, NULL, 0, responseCode);
                    //FinishVPNLookup(dl->handle, NULL, 0, responseCode, )
                    gi.dprintf ("HTTP: %s: 404 File Not Found\n", dl->URL);
                    curl_multi_remove_handle (multi, dl->curl);
                    dl->inuse = qfalse;
                    continue;
                } else if (responseCode == 200) {
                    HandleDownload(dl->handle, dl->tempBuffer, dl->position, responseCode);
                    gi.TagFree(dl->tempBuffer);
                } else {
                    HandleDownload(dl->handle, NULL, 0, responseCode);
                    if (dl->tempBuffer) {
                        gi.TagFree (dl->tempBuffer);
                    }
                }
                break;

            //fatal error
            default:
                HandleDownload(dl->handle, NULL, 0, 0);
                gi.dprintf("HTTP Error: %s: %s\n", dl->URL, curl_easy_strerror (result));
                curl_multi_remove_handle(multi, dl->curl);
                dl->inuse = qfalse;
                continue;
        }

        //show some stats
        curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &timeTaken);
        curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &fileSize);

        //FIXME:
        //technically i shouldn't need to do this as curl will auto reuse the
        //existing handle when you change the URL. however, the handleCount goes
        //all weird when reusing a download slot in this way. if you can figure
        //out why, please let me know.
        curl_multi_remove_handle(multi, dl->curl);
        dl->inuse = qfalse;
        gi.dprintf("HTTP: Finished %s: %.f bytes, %.2fkB/sec\n", dl->URL, fileSize, (fileSize / 1024.0) / timeTaken);
    } while (msgs_in_queue > 0);
}

/**
 *
 */
qboolean HTTP_QueueDownload(download_t *d) {
    unsigned    i;

    if (handleCount == MAX_DOWNLOADS) {
        gi.dprintf("Another download is already pending, please try again later.\n");
        return qfalse;
    }

    if (!http_enable) {
        gi.dprintf("HTTP functions are disabled on this server.\n");
        return qfalse;
    }

    for (i = 0; i < MAX_DOWNLOADS; i++) {
        if (!downloads[i].inuse) {
            break;
        }
    }

    if (i == MAX_DOWNLOADS) {
        gi.dprintf("The server is too busy to download configs right now.\n");
        return qfalse;
    }

    downloads[i].handle = d;
    downloads[i].inuse = qtrue;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-truncation"
    Q_strncpy(downloads[i].filePath, d->path, sizeof(downloads[i].filePath)-1);
#pragma GCC diagnostic pop
    HTTP_StartDownload(&downloads[i]);

    return qtrue;
}

/**
 * This calls curl_multi_perform to actually do stuff. Called every frame to
 * process downloads.
 */
void HTTP_RunDownloads(void) {
    int         newHandleCount;
    CURLMcode   ret;

    //nothing to do!
    if (!handleCount) {
        return;
    }

    do {
        ret = curl_multi_perform(multi, &newHandleCount);
        if (newHandleCount < handleCount) {
            HTTP_FinishDownload();
            handleCount = newHandleCount;
        }
    } while (ret == CURLM_CALL_MULTI_PERFORM);

    if (ret != CURLM_OK) {
        gi.dprintf("HTTP_RunDownloads: curl_multi_perform error.\n");
    }
}

static size_t http_GetFile_callback(void *ptr, size_t size, size_t nmemb, void *out) {
    generic_file_t *gf = (generic_file_t *)out;
    size_t total = size * nmemb;
    memcpy(gf->data, ptr, total);
    gf->index += total;
    return total;
}

/**
 * Download any file.
 *
 * Returned char pointer needs to be free'd!
 */
size_t HTTP_GetFile(generic_file_t *output, const char *url) {
    CURL *curl_handle;

    q2a_memset(output->data, 0, output->size);
    curl_handle = curl_easy_init();
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 0L);
    curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, http_GetFile_callback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, output);
    curl_easy_perform(curl_handle);
    curl_easy_cleanup(curl_handle);
    return output->index;
}
