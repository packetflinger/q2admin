/**
 * Q2Admin
 * CURL functions
 */

#pragma once

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

#include <curl/curl.h>

typedef enum {
    DL_NONE,
    DL_VPNAPI,
} dltype_t;

typedef struct download_s {
    edict_t     *initiator;
    dltype_t    type;
    char        path[1024];
    void        (*onFinish)(struct download_s *, int, byte *, int);
} download_t;

typedef struct dlhandle_s {
    CURL            *curl;
    size_t          fileSize;
    size_t          position;
    double          speed;
    char            filePath[1024];
    char            URL[2048];
    char            *tempBuffer;
    qboolean        inuse;
    download_t      *handle;
} dlhandle_t;

#define MAX_DOWNLOADS   16
#define MAX_DLSIZE      0x100000    // 1 MiB
#define MIN_DLSIZE      0x8000      // 32 KiB

int CURL_Debug(CURL *c, curl_infotype type, char *data, size_t size, void * ptr);
void HandleDownload(download_t *download, char *buff, int len, int code);
void HTTP_Init(void);
qboolean HTTP_QueueDownload(download_t *d);
void HTTP_ResolveVPNServer(void);
void HTTP_RunDownloads(void);
void HTTP_Shutdown(void);
void HTTP_StartDownload(dlhandle_t *dl);
