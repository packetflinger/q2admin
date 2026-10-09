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
static struct curl_slist    *post_header_slist;
static time_t               last_dns_lookup;

/**
 * Hands a finished request back to whoever asked for it, by invoking the
 * onFinish callback stored on the download_t. That indirection is what
 * keeps this module generic: it knows how to move bytes but nothing
 * about VPN lookups or ban lists, and each caller supplies its own
 * handler when queuing (see httpQueueDownload()).
 *
 * download: the request being completed, carrying the callback and the
 *           edict that initiated it.
 * buff:     the response body, or NULL if the request failed or returned
 *           anything other than 200.
 * len:      length of buff, 0 when buff is NULL.
 * code:     the HTTP status, or 0 for a transport-level failure.
 *
 * Returns nothing.
 *
 * Called from httpFinishDownload() below, on every outcome - success,
 * HTTP error and transport error alike - so a caller's handler always
 * runs exactly once per request and can clean up its own state.
 *
 * Note the NULL-initiator check only logs; it still calls onFinish, and
 * the handlers in g_vpn.c dereference that edict immediately, so the
 * warning doesn't actually prevent the crash it's warning about.
 */
void httpHandleDownload(download_t *download, char *buff, int len, int code) {
    if (!download->initiator) {
        gi.dprintf("httpHandleDownload: NULL initiator");
    }
    download->onFinish(download, code, (byte *)buff, len);
}

/**
 * libcurl write callback for the asynchronous download path - curl calls
 * this repeatedly with chunks of the response as they arrive, and this
 * accumulates them into one buffer for the caller.
 *
 * Because the final size isn't known up front, the buffer is grown on
 * demand in MIN_DLSIZE steps (rounded to that boundary so a trickle of
 * small chunks doesn't reallocate constantly) and always kept NUL
 * terminated, so the assembled body can be handed straight to the string
 * and JSON parsers that consume it.
 *
 * The size guards matter because the response comes from a remote server
 * that could hand back anything: the multiply is checked for overflow
 * and the total is capped at MAX_DLSIZE, so a hostile or broken endpoint
 * can't make the server allocate without bound.
 *
 * ptr:    chunk of received data.
 * size:   size of each element, per the libcurl callback signature.
 * nmemb:  number of elements; the chunk is size * nmemb bytes.
 * stream: the dlhandle_t this data belongs to, set via CURLOPT_WRITEDATA.
 *
 * Returns the number of bytes consumed. Returning anything less than the
 * chunk size - 0 here - is how a libcurl write callback aborts the
 * transfer, which is what the oversize path relies on.
 *
 * Called by libcurl itself, wired up in httpStartDownload() below.
 */
static size_t httpRecv(void *ptr, size_t size, size_t nmemb, void *stream) {
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
        dl->tempBuffer = G_Malloc((int)new_size);
        if (tmp) {
            q2a_memcpy(dl->tempBuffer, tmp, dl->fileSize);
            G_Free(tmp);
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
 * libcurl debug callback, for troubleshooting why a request isn't
 * working - TLS negotiation, redirects, connection reuse and so on -
 * routed to the server console rather than curl's default stderr, which
 * a dedicated server generally isn't watching.
 *
 * Only CURLINFO_TEXT (curl's own commentary) is printed; the raw header
 * and body payloads are ignored, which keeps credentials such as the
 * vpnapi.io API key out of the log.
 *
 * c:    the curl handle, unused here.
 * type: what kind of information this is; anything but CURLINFO_TEXT is
 *       ignored.
 * data: the text, which is not NUL terminated - hence the bounded copy.
 * size: length of data; truncated to the local buffer.
 * ptr:  user pointer, unused here.
 *
 * Returns 0, which libcurl requires from a debug callback.
 *
 * Called by libcurl itself, wired up in httpStartDownload() below only
 * when the http_debug setting is on.
 */
int curlDebug(CURL *c, curl_infotype type, char *data, size_t size, void * ptr) {
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
 * Resolves the VPN API hostname to an address and caches it, re-resolving
 * only if the last lookup was over a day ago. gethostbyname() blocks, so
 * the point of caching was to keep a DNS round trip off the server frame
 * on every lookup while still picking up DNS changes eventually.
 *
 * Takes no parameters; reads the vpn_host setting and updates the
 * module's cached address and timestamp. Returns nothing - failure is
 * reported to the console and leaves the cached address empty.
 *
 * Currently unreachable: nothing calls it, and nothing reads the address
 * it caches either. Requests are built from the hostname directly (see
 * httpStartDownload()) and libcurl does its own resolution and caching,
 * so this is a leftover from before that was the case.
 */
void httpResolveVPNServer(void) {
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
 *
 * Builds the URL, applies every curl option the request needs and hands
 * the easy handle to the multi interface. Adding it to the multi handle
 * rather than calling curl_easy_perform() is the crux of the whole
 * module: perform() would block the server for the length of the
 * request, freezing every player, whereas the multi interface just
 * registers the transfer and lets httpRunDownloads() advance it a
 * little each frame.
 *
 * Both request shapes go through here. A GET targets vpn_host with the
 * path as-is; a POST overrides the host from the download_t and sends
 * its body as JSON. The easy handle is reused across requests when the
 * slot already has one, since curl handles are relatively expensive to
 * create and hold connection state worth keeping.
 *
 * dl: the download slot to start, already populated by
 *     httpQueueDownload() with the request and its target path.
 *
 * Returns nothing; a failure to register is logged and leaves the slot
 * marked in use, so a slot lost this way isn't reclaimed until the
 * module is torn down.
 *
 * Called from httpQueueDownload() below, once a free slot is found.
 */
void httpStartDownload(dlhandle_t *dl) {
    dl->tempBuffer = NULL;
    dl->speed = 0;
    dl->fileSize = 0;
    dl->position = 0;

    if (!dl->curl) {
        dl->curl = curl_easy_init();
    }

    // GET format: https://vpnapi.io/api/<ipaddress>?key=<apikey>
    // POST format: https://<dl->handle->host><dl->handle->path>, body = dl->handle->body
    snprintf(dl->URL, sizeof(dl->URL), "https://%s%s", dl->handle->host[0] ? dl->handle->host : vpn_host, dl->filePath);

    if (dl->handle->post) {
        curl_easy_setopt(dl->curl, CURLOPT_HTTPHEADER, post_header_slist);
        curl_easy_setopt(dl->curl, CURLOPT_POST, 1L);
        curl_easy_setopt(dl->curl, CURLOPT_COPYPOSTFIELDS, dl->handle->body);
    } else {
        curl_easy_setopt(dl->curl, CURLOPT_HTTPHEADER, http_header_slist);
        curl_easy_setopt(dl->curl, CURLOPT_HTTPGET, 1L);
    }
    curl_easy_setopt(dl->curl, CURLOPT_ENCODING, "");

    if (http_debug) {
        curl_easy_setopt(dl->curl, CURLOPT_DEBUGFUNCTION, curlDebug);
        curl_easy_setopt(dl->curl, CURLOPT_VERBOSE, 1);
    } else {
        curl_easy_setopt(dl->curl, CURLOPT_DEBUGFUNCTION, NULL);
        curl_easy_setopt(dl->curl, CURLOPT_VERBOSE, 0);
    }

    curl_easy_setopt(dl->curl, CURLOPT_NOPROGRESS, 1);
    curl_easy_setopt(dl->curl, CURLOPT_WRITEDATA, dl);
    curl_easy_setopt(dl->curl, CURLOPT_INTERFACE, NULL);
    curl_easy_setopt(dl->curl, CURLOPT_WRITEFUNCTION, httpRecv);
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
        gi.dprintf("httpStartDownload: curl_multi_add_handle: error\n");
        return;
    }

    handleCount++;
}

/**
 * Brings up the HTTP layer: initialises libcurl, creates the multi
 * handle every asynchronous transfer is registered against, and
 * pre-builds the two header lists requests reuse - a Host header for the
 * VPN API and a JSON content type for POSTs. Building those once here
 * rather than per request avoids rebuilding an identical list on every
 * lookup.
 *
 * Takes no parameters. Returns nothing; reports the libcurl version to
 * the console so the log records which one is actually in use.
 *
 * Called from InitGame() (g_init.c) at server startup.
 *
 * Note InitGame() has already called curl_global_init() itself by this
 * point, so the call here is a second, redundant one - harmless, since
 * libcurl reference counts it, but it means the flags differ between the
 * two (CURL_GLOBAL_ALL there, CURL_GLOBAL_NOTHING here) and the first
 * call is the one that decides.
 */
void httpInit(void) {
    curl_global_init(CURL_GLOBAL_NOTHING);
    multi = curl_multi_init();
    snprintf(hostHeader, sizeof(hostHeader), "Host: %s", vpn_host);
    http_header_slist = curl_slist_append(http_header_slist, hostHeader);
    post_header_slist = curl_slist_append(post_header_slist, "Content-Type: application/json");
    gi.dprintf("%s initialized.\n", curl_version());
}

/**
 * Tears down everything httpInit() set up - the multi handle, both
 * header lists and libcurl itself - and is written to be safe to call
 * more than once, clearing the multi handle as it goes.
 *
 * Takes no parameters. Returns nothing.
 *
 * Currently unreachable: nothing calls it. ShutdownGame() (g_main.c)
 * calls curl_global_cleanup() directly instead, so on the way down the
 * multi handle and the two header lists are never freed. Harmless in
 * practice, since the process is exiting anyway, but it does mean this
 * is the intended teardown path and isn't wired up.
 */
void httpShutdown(void) {
    if (multi) {
        curl_multi_cleanup(multi);
        multi = NULL;
    }
    curl_slist_free_all(http_header_slist);
    curl_slist_free_all(post_header_slist);
    curl_global_cleanup();
}

/**
 * A download finished, find out what it was, whether there were any errors and
 * if so, how severe. If none, rename file and other such stuff.
 *
 * Drains libcurl's completion queue, matching each finished easy handle
 * back to its download slot, reporting the outcome to the requester via
 * httpHandleDownload() and releasing the slot for reuse. Every path calls
 * httpHandleDownload() exactly once - 200, 404, other statuses and transport
 * failures alike - so a caller's handler always runs and can't be left
 * waiting on a reply that never comes.
 *
 * The response buffer is freed here on every outcome, which means a
 * handler must copy anything it needs to keep rather than holding the
 * pointer it was given.
 *
 * Takes no parameters; works on the module's download slots and multi
 * handle. Returns nothing.
 *
 * Called from httpRunDownloads() below, when curl reports that the
 * number of active transfers has dropped.
 *
 * Note the "Handle not found" case logs but doesn't bail, so it goes on
 * to index downloads[] one past the end.
 */
static void httpFinishDownload(void) {
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
            gi.dprintf("httpFinishDownload: Odd, no message for us...\n");
            return;
        }

        if (msg->msg != CURLMSG_DONE) {
            gi.dprintf("httpFinishDownload: Got some weird message...\n");
            continue;
        }

        curl = msg->easy_handle;

        for (i = 0; i < MAX_DOWNLOADS; i++) {
            if (downloads[i].curl == curl) {
                break;
            }
        }

        if (i == MAX_DOWNLOADS) {
            gi.dprintf("httpFinishDownload: Handle not found!\n");
        }

        dl = &downloads[i];

        result = msg->data.result;

        switch (result) {
            //for some reason curl returns CURLE_OK for a 404...
            case CURLE_HTTP_RETURNED_ERROR:
            case CURLE_OK:

                curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &responseCode);
                if (responseCode == 404) {
                    httpHandleDownload(dl->handle, NULL, 0, responseCode);
                    //FinishVPNLookup(dl->handle, NULL, 0, responseCode, )
                    gi.dprintf ("HTTP: %s: 404 File Not Found\n", dl->URL);
                    curl_multi_remove_handle (multi, dl->curl);
                    dl->inuse = false;
                    continue;
                } else if (responseCode == 200) {
                    httpHandleDownload(dl->handle, dl->tempBuffer, dl->position, responseCode);
                    gi.TagFree(dl->tempBuffer);
                } else {
                    httpHandleDownload(dl->handle, NULL, 0, responseCode);
                    if (dl->tempBuffer) {
                        gi.TagFree (dl->tempBuffer);
                    }
                }
                break;

            //fatal error
            default:
                httpHandleDownload(dl->handle, NULL, 0, 0);
                gi.dprintf("HTTP Error: %s: %s\n", dl->URL, curl_easy_strerror (result));
                curl_multi_remove_handle(multi, dl->curl);
                dl->inuse = false;
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
        dl->inuse = false;
        gi.dprintf("HTTP: Finished %s: %.f bytes, %.2fkB/sec\n", dl->URL, fileSize, (fileSize / 1024.0) / timeTaken);
    } while (msgs_in_queue > 0);
}

/**
 * Submits a request to be fetched in the background: finds a free
 * download slot, attaches the caller's request to it and starts it.
 *
 * This is the module's entry point for asynchronous work, and the reason
 * it's asynchronous is that the callers are on the connect path - a
 * player joining triggers a VPN lookup against a third-party API, and
 * blocking the server on that round trip would stall everyone. Instead
 * this returns immediately and the caller's onFinish runs a few frames
 * later once the reply lands.
 *
 * d: the request to run - target host and path, GET or POST plus body,
 *    and the onFinish callback. The caller retains ownership and it must
 *    stay valid until that callback fires, which is why callers keep it
 *    in their own proxyinfo slot rather than on the stack.
 *
 * Returns true if the request was accepted, false if HTTP is disabled or
 * all MAX_DOWNLOADS slots are busy. A false return means onFinish will
 * never be called, so callers that track pending state need to undo it.
 *
 * Called from the VPN lookup paths in g_vpn.c - the vpnapi.io GET and
 * the IPLogs POST - as a player connects.
 */
bool httpQueueDownload(download_t *d) {
    unsigned    i;

    if (handleCount == MAX_DOWNLOADS) {
        gi.dprintf("Another download is already pending, please try again later.\n");
        return false;
    }

    if (!http_enable) {
        gi.dprintf("HTTP functions are disabled on this server.\n");
        return false;
    }

    for (i = 0; i < MAX_DOWNLOADS; i++) {
        if (!downloads[i].inuse) {
            break;
        }
    }

    if (i == MAX_DOWNLOADS) {
        gi.dprintf("The server is too busy to download configs right now.\n");
        return false;
    }

    downloads[i].handle = d;
    downloads[i].inuse = true;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-truncation"
    Q_strncpy(downloads[i].filePath, d->path, sizeof(downloads[i].filePath)-1);
#pragma GCC diagnostic pop
    httpStartDownload(&downloads[i]);

    return true;
}

/**
 * This calls curl_multi_perform to actually do stuff. Called every frame to
 * process downloads.
 *
 * Gives libcurl a slice of time to advance any in-flight transfers and
 * returns straight away - it never waits on the network. That's what
 * makes the whole asynchronous scheme work: progress happens a frame at
 * a time alongside normal server work, rather than the server stopping
 * to wait for a remote API.
 *
 * When curl reports fewer active transfers than last time, at least one
 * has finished, so httpFinishDownload() is called to collect the
 * results. Returns immediately when nothing is pending, which is the
 * usual case.
 *
 * Takes no parameters; works on the module's multi handle. Returns
 * nothing.
 *
 * Called from G_RunFrame() (g_main.c) every server frame.
 */
void httpRunDownloads(void) {
    int         newHandleCount;
    CURLMcode   ret;

    //nothing to do!
    if (!handleCount) {
        return;
    }

    do {
        ret = curl_multi_perform(multi, &newHandleCount);
        if (newHandleCount < handleCount) {
            httpFinishDownload();
            handleCount = newHandleCount;
        }
    } while (ret == CURLM_CALL_MULTI_PERFORM);

    if (ret != CURLM_OK) {
        gi.dprintf("httpRunDownloads: curl_multi_perform error.\n");
    }
}

/**
 * libcurl write callback for the synchronous httpGetFile() path,
 * copying received data into the caller's fixed buffer. The counterpart
 * to httpRecv() above, but for a caller that has already decided how
 * much room to provide.
 *
 * ptr:   chunk of received data.
 * size:  size of each element, per the libcurl callback signature.
 * nmemb: number of elements; the chunk is size * nmemb bytes.
 * out:   the generic_file_t to fill, set via CURLOPT_WRITEDATA.
 *
 * Returns the number of bytes consumed, which libcurl compares against
 * the chunk size to decide whether to continue.
 *
 * Called by libcurl itself, wired up in httpGetFile() below.
 *
 * Two things to be aware of, since libcurl delivers a response in as
 * many chunks as it likes rather than one: every chunk is copied to the
 * *start* of the buffer rather than appended at the running index, so
 * for a multi-chunk response only the final chunk survives while index
 * still reports the full length; and nothing checks the incoming size
 * against the buffer's, so a response larger than the caller allocated
 * overruns it. Both matter here because the bodies come from a remote
 * server whose size and chunking aren't under q2admin's control.
 */
static size_t httpGetFileCallback(void *ptr, size_t size, size_t nmemb, void *out) {
    generic_file_t *gf = (generic_file_t *)out;
    size_t total = size * nmemb;
    q2a_memcpy(gf->data, ptr, total);
    gf->index += total;
    return total;
}

/**
 * Download any file.
 *
 * The synchronous counterpart to httpQueueDownload(): this uses
 * curl_easy_perform() and so blocks the whole server until the transfer
 * finishes or times out. That's tolerable only because its callers run
 * at map load, where a pause is expected anyway and the fetched data is
 * needed before play starts - it would not be acceptable on the connect
 * path, which is exactly why the asynchronous machinery above exists.
 *
 * Uses its own one-off easy handle rather than the shared multi handle,
 * so it's independent of the download slots and can't be blocked by
 * them being busy.
 *
 * output: caller-provided buffer to fill. data and size must be set
 *         before the call, and index reset to 0; on return index holds
 *         how many bytes arrived.
 * url:    full URL to fetch, including scheme.
 *
 * Returns the number of bytes received, i.e. output->index. Note there's
 * no way to tell a genuine empty response from a failed request - the
 * curl result is discarded - so 0 should be treated as "no usable data"
 * rather than proof the server replied.
 *
 * Also note SSL peer verification is disabled here, unlike the
 * asynchronous path which honours the http_verifyssl setting.
 *
 * Called from readRemoteBanFile() (g_ban.c) for a remote ban list and
 * from the anticheat hash list loader (g_anticheat.c), both at map load.
 *
 * The "returned char pointer needs to be free'd" note above refers to
 * output->data, which the caller allocated and still owns - this
 * function neither allocates nor frees it.
 */
size_t httpGetFile(generic_file_t *output, const char *url) {
    CURL *curl_handle;

    q2a_memset(output->data, 0, output->size);
    curl_handle = curl_easy_init();
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 0L);
    curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, httpGetFileCallback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, output);
    curl_easy_perform(curl_handle);
    curl_easy_cleanup(curl_handle);
    return output->index;
}
