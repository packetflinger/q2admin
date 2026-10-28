/**
 * Copyright (C) The Q2Admin contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#include "g_local.h"
#include <pthread.h>

cloud_t cloud;

static bool cloudStartLookup(void);
static void cloudAbandonLookup(void);

/**
 * Frees the backend address list from the last DNS lookup and clears both
 * pointers into it. cloud.addr points somewhere inside cloud.addr_list, so
 * the list has to be freed from its head rather than through cloud.addr.
 *
 * Takes no parameters and returns nothing. Safe to call when there's no
 * list.
 *
 * Called from cloudResetState(), cloudShutdown() and cloudParseError().
 */
static void cloudFreeAddress(void) {
    if (cloud.addr_list) {
        freeaddrinfo(cloud.addr_list);
    }
    cloud.addr_list = NULL;
    cloud.addr = NULL;
}

/**
 * Clears all connection state, ready for a fresh start, and releases what
 * the last connection held: its socket, resolved address, cipher contexts
 * and RSA keys. Without that, every reconnect would leak them. A DNS
 * lookup still running is abandoned, so it can't apply its result after
 * the reset.
 *
 * The map name, player slot count and game port are kept. SpawnEntities()
 * only sets them on a map change, so losing them here would send an empty
 * hello (port 0, no slots) until the next map.
 *
 * Takes no parameters and returns nothing. Leaves the state at
 * CLOUD_STATE_DISABLED.
 *
 * Called from cloudInit(), and from cloudRun() for "sv !cloud disconnect"
 * and "sv !cloud reconnect".
 */
static void cloudResetState(void) {
    cloud_connection_t *c = &cloud.connection;
    char mapname[sizeof(cloud.mapname)];
    uint8_t maxclients = cloud.maxclients;
    uint16_t port = cloud.port;

    q2a_memcpy(mapname, cloud.mapname, sizeof(mapname));

    // cloudDisconnect() only closes connected sockets, not one still
    // connecting
    if (cloud.state == CLOUD_STATE_CONNECTING) {
        closesocket(c->socket);
    }

    cloudAbandonLookup();
    cloudFreeAddress();
    EVP_CIPHER_CTX_free(c->e_ctx);
    EVP_CIPHER_CTX_free(c->d_ctx);
    EVP_PKEY_free(c->public_key);
    EVP_PKEY_free(c->private_key);
    EVP_PKEY_free(c->server_key);

    q2a_memset(&cloud, 0, sizeof(cloud));

    q2a_memcpy(cloud.mapname, mapname, sizeof(cloud.mapname));
    cloud.maxclients = maxclients;
    cloud.port = port;
}

/**
 * Sets up the Cloud Admin connection: reads q2a_cloud.cfg, resets all
 * connection state, loads the RSA keys and starts a background DNS lookup
 * for the backend's address. The connection itself isn't opened here.
 * cloudFrame() does that a few seconds later, once the lookup has had time
 * to finish, so a slow DNS server never holds up server start.
 *
 * Leaves the state at CLOUD_STATE_DISABLED (so cloudFrame() does nothing)
 * if Cloud Admin is turned off in the config, the keys can't be loaded,
 * cloud_addr or cloud_port isn't set, or the lookup thread can't start. Does nothing at all if Cloud Admin is
 * already running (any state but CLOUD_STATE_DISABLED).
 *
 * Takes no parameters and returns nothing; fills the global cloud.
 *
 * Called from InitGame() (g_init.c) at server start, and from cloudRun()
 * for "sv !cloud connect" and "sv !cloud reconnect".
 */
void cloudInit() {
    if (cloud.state != CLOUD_STATE_DISABLED) {
        cloudPrintf("already running, use \"sv !cloud reconnect\" to restart\n");
        return;
    }

    cloudReadConfigFile();

    cloudResetState();
    maxclients = gi.cvar("maxclients", "64", CVAR_LATCH);

    // Set before anything else, so the debug messages below show up when
    // the debug flag is on.
    cloud.flags = cloud_config.flags;
    
    if (!cloud_config.enabled) {
        gi.cprintf(NULL, PRINT_HIGH, "Cloud Admin is disabled in your config.\n");
        return;
    }
    cloud.state = CLOUD_STATE_DISCONNECTED;
    cloudPrintf("init...\n");

    if (!cryptoLoadKeys()) {
        cloud.state = CLOUD_STATE_DISABLED;
        return;
    }

    cloud.connection.encrypted = cloud_config.encryption;
    
    if (!cloud_config.address[0]) {
        cloudDPrintf("cloud_addr is not set, disabling\n");
        cloud.state = CLOUD_STATE_DISABLED;
        return;
    }

    if (!cloud_config.port) {
        cloudDPrintf("cloud_port is not set, disabling\n");
        cloud.state = CLOUD_STATE_DISABLED;
        return;
    }

    if (!cloudStartLookup()) {
        cloudPrintf("couldn't start the DNS lookup, disabling\n");
        cloud.state = CLOUD_STATE_DISABLED;
        return;
    }

    // delay connection by a few seconds
    cloud.connect_retry_frame = FUTURE_CA_FRAME(5);
}

/**
 * Loads the Cloud Admin settings from configfile_cloud (q2a_cloud.cfg by
 * default, renamed with the q2a_cloudfile cvar). The file uses the same
 * option "value" format as q2admin.cfg and goes through the same parser,
 * readCfgFile().
 *
 * The copy in the mod directory is read first and the one in the Quake 2
 * root directory second, so where a setting is in both, the root one wins.
 * That's the reverse of q2admin.cfg, which readCfgFiles() reads root first.
 *
 * Takes no parameters and returns nothing; fills cloud_config.
 *
 * Called from cloudInit().
 */
void cloudReadConfigFile() {
    Q_snprintf(buffer, sizeof(buffer), "%s/%s", moddir, configfile_cloud->string);
    readCfgFile(buffer);
    readCfgFile(configfile_cloud->string);
}

/**
 * Picks the backend address to connect to from getaddrinfo()'s results.
 * A name can resolve to several IPv4 and IPv6 addresses, so this takes the
 * first of each family and chooses between them using the cloud_dns
 * setting: a string of up to two characters, "4" and/or "6", in order of
 * preference ("64" means IPv6 if there is one, otherwise IPv4). An empty
 * setting is treated as "64".
 *
 * a: the list getaddrinfo() returned.
 *
 * Returns the chosen entry, which still belongs to that list, or NULL if a
 * is NULL or no address of a preferred family was found.
 *
 * Called from cloudFinishLookup().
 */
static struct addrinfo *cloudSelectAddrinfo(struct addrinfo *a) {
    // Not static: each lookup has to choose from its own results, not keep
    // pointers into an earlier list that has since been freed.
    struct addrinfo *v4 = NULL, *v6 = NULL;

    if (!a) {
        return NULL;
    }

    // Just in case it's set blank in the config
    if (!cloud_config.dns[0]) {
        Q_snprintf(cloud_config.dns, sizeof(cloud_config.dns), "64");
    }

    // Save the first one of each address family if more than 1. They'll be in
    // random order.
    for (; a != NULL; a = a->ai_next) {
        if (!v4 && a->ai_family == AF_INET) {
            v4 = a;
        }
        if (!v6 && a->ai_family == AF_INET6) {
            v6 = a;
        }
    }

    // select one based on preference
    if (cloud_config.dns[0] == '6' && v6) {
        return v6;
    } else if (cloud_config.dns[0] == '4' && v4) {
        return v4;
    } else if (cloud_config.dns[1] == '6' && v6) {
        return v6;
    } else if (cloud_config.dns[1] == '4' && v4) {
        return v4;
    }

    return NULL;
}


/**
 * Looks up the backend's address on its own thread, because getaddrinfo()
 * blocks: on the game thread a slow DNS server would freeze the whole
 * server, and every player on it, until the lookup finished.
 *
 * It works only on its cloud_lookup_t, never on cloud or cloud_config, and
 * prints nothing (gi.cprintf() isn't safe off the game thread). When it's
 * done it marks the lookup CLOUD_LOOKUP_DONE for cloudFinishLookup() to
 * pick up. If the game thread has abandoned the lookup in the meantime
 * (see cloudAbandonLookup()), nobody else will free it, so this does.
 *
 * arg: the cloud_lookup_t to fill.
 *
 * Returns NULL.
 *
 * Started from cloudStartLookup().
 */
static void *cloudLookupThread(void *arg) {
    cloud_lookup_t *l = arg;
    struct addrinfo hints;

    q2a_memset(&hints, 0, sizeof(hints));
    hints.ai_family         = AF_UNSPEC;     // either v6 or v4
    hints.ai_socktype       = SOCK_STREAM;   // TCP
    hints.ai_protocol       = 0;
    hints.ai_flags          = AI_ADDRCONFIG; // only for v6 addresses

    l->res = NULL;
    l->err = getaddrinfo(l->address, l->port, &hints, &l->res);

    // Publish the result. If the lookup was abandoned first, the game
    // thread has already let go of it and it's ours to free.
    if (__atomic_exchange_n(&l->status, CLOUD_LOOKUP_DONE, __ATOMIC_ACQ_REL) == CLOUD_LOOKUP_ABANDONED) {
        if (l->res) {
            freeaddrinfo(l->res);
        }
        free(l);
    }
    return NULL;
}

#ifdef _WIN32
/**
 * Adapts cloudLookupThread() to the signature CreateThread() expects.
 * Casting it instead would mix up calling conventions on 32-bit Windows
 * and unbalance the thread's stack when it returns.
 *
 * arg: the cloud_lookup_t to fill.
 *
 * Returns 0.
 *
 * Started from cloudStartLookup().
 */
static DWORD WINAPI cloudLookupThreadWin(LPVOID arg) {
    cloudLookupThread(arg);
    return 0;
}
#endif

/**
 * Starts a background lookup of cloud_addr and cloud_port, using
 * CreateThread() on Windows and pthreads everywhere else. The address and
 * port are copied into the lookup first, so the thread never reads
 * cloud_config, which "sv !cloud connect" can reload while it runs.
 *
 * Takes no parameters.
 *
 * Returns true if the thread started, in which case cloud.lookup is set
 * and cloudFinishLookup() collects the result; false if it couldn't.
 *
 * Called from cloudInit().
 */
static bool cloudStartLookup(void) {
    cloud_lookup_t *l = calloc(1, sizeof(*l));

    if (!l) {
        return false;
    }
    Q_strlcpy(l->address, cloud_config.address, sizeof(l->address));
    Q_snprintf(l->port, sizeof(l->port), "%d", cloud_config.port);
    l->status = CLOUD_LOOKUP_PENDING;

#ifdef _WIN32
    HANDLE thread = CreateThread(NULL, 0, cloudLookupThreadWin, l, 0, NULL);
    if (!thread) {
        free(l);
        return false;
    }
    CloseHandle(thread);
#else
    pthread_t thread;
    if (pthread_create(&thread, NULL, cloudLookupThread, l) != 0) {
        free(l);
        return false;
    }
    // nothing waits for it, so let its resources go when it finishes
    pthread_detach(thread);
#endif

    cloud.lookup = l;
    return true;
}

/**
 * Lets go of the lookup in progress, if any, when its result is no longer
 * wanted: on disconnect, reconnect or shutdown. A lookup can't be
 * cancelled, so if it's still running it's marked abandoned and the
 * thread frees it when getaddrinfo() returns. If it had already finished,
 * it's freed here. Either way its result is never applied, so it can't
 * reconnect after a disconnect or overwrite a newer lookup's address.
 *
 * Takes no parameters and returns nothing.
 *
 * Called from cloudResetState() and cloudShutdown().
 */
static void cloudAbandonLookup(void) {
    cloud_lookup_t *l = cloud.lookup;

    if (!l) {
        return;
    }
    cloud.lookup = NULL;

    if (__atomic_exchange_n(&l->status, CLOUD_LOOKUP_ABANDONED, __ATOMIC_ACQ_REL) == CLOUD_LOOKUP_DONE) {
        if (l->res) {
            freeaddrinfo(l->res);
        }
        free(l);
    }
}

/**
 * Collects the result of the background lookup once it has finished, on
 * the game thread, and fills cloud.addr from it, ready for cloudConnect().
 * The whole result list is kept in cloud.addr_list so cloudFreeAddress()
 * can free it later.
 *
 * Sets the state to CLOUD_STATE_DISABLED if the name doesn't resolve or
 * has no address of a family cloud_dns allows. Otherwise the state stays
 * CLOUD_STATE_DISCONNECTED and cloudFrame() starts connecting.
 *
 * Takes no parameters and returns nothing. Does nothing if no lookup is
 * running or it hasn't finished yet.
 *
 * Called from cloudFrame() every frame.
 */
static void cloudFinishLookup(void) {
    cloud_lookup_t *l = cloud.lookup;
    struct addrinfo *res, *chosen;
    char str_address[INET6_ADDRSTRLEN];
    int err;

    if (!l || __atomic_load_n(&l->status, __ATOMIC_ACQUIRE) != CLOUD_LOOKUP_DONE) {
        return;
    }
    cloud.lookup = NULL;
    err = l->err;
    res = l->res;
    free(l);

    if (err != 0) {
        if (res) {
            freeaddrinfo(res);
        }
        cloudDPrintf("DNS error looking up %s (%d), disabling\n", cloud_config.address, err);
        cloud.state = CLOUD_STATE_DISABLED;
        return;
    }

    // getaddrinfo can return multiple mixed v4/v6 results
    chosen = cloudSelectAddrinfo(res);
    if (!chosen) {
        if (res) {
            freeaddrinfo(res);
        }
        cloud.state = CLOUD_STATE_DISABLED;
        cloudDPrintf("problems resolving server address, disabling\n");
        return;
    }

    // Keep the head of the list too; it's what freeaddrinfo() needs.
    cloud.addr_list = res;
    cloud.addr = chosen;
    cloud.connection.ipv6 = (cloud.addr->ai_family == AF_INET6);

    if (cloud.addr->ai_family == AF_INET6) {
        q2a_inet_ntop(
                cloud.addr->ai_family,
                &((struct sockaddr_in6 *) cloud.addr->ai_addr)->sin6_addr,
                str_address,
                sizeof(str_address)
        );
    } else {
        q2a_inet_ntop(
                cloud.addr->ai_family,
                &((struct sockaddr_in *) cloud.addr->ai_addr)->sin_addr,
                str_address,
                sizeof(str_address)
        );
    }

    cloudDPrintf("server resolved to %s\n", str_address);
}

/**
 * Prints str and a newline to the server console, but only when the debug
 * flag (CFL_DEBUG) is set in cloud_flags.
 *
 * str: the text to print.
 *
 * Returns nothing.
 *
 * Nothing calls this at the moment; cloudDPrintf() does the same job with
 * printf-style formatting.
 */
void cloudDebugPrint(char *str) {
    if (!CFL(DEBUG)) {
        return;
    }
    gi.cprintf(NULL, PRINT_HIGH, "%s\n", str);
}

/**
 * Sends the backend a CMD_PING every PING_FREQ_SECS seconds while
 * connected, so a connection that has silently died gets noticed. The
 * backend answers with SCMD_PONG (see cloudParsePong()). If a ping is still
 * unanswered when the next one is due, it counts as a miss, and after
 * PING_MISS_MAX misses the connection is treated as lost and
 * cloudPeerDisconnected() starts a reconnect.
 *
 * TCP keepalives could do much the same, but they're off by default and
 * can take hours to notice a dead link, so this keeps the check in our
 * hands.
 *
 * Takes no parameters and returns nothing.
 *
 * Called from cloudFrame() once the connection is trusted.
 */
void cloudPing(void) {
    if (cloud.state < CLOUD_STATE_CONNECTED) {
        return;
    }

    // not time yet
    if (cloud.ping.frame_next > CA_FRAME) {
        return;
    }

    // there is already an outstanding ping
    if (cloud.ping.waiting) {
        if (cloud.ping.miss_count == PING_MISS_MAX) {
            cloudPeerDisconnected();
            return;
        }
        cloud.ping.miss_count++;
    }

    // state stuff
    cloud.ping.frame_sent = CA_FRAME;
    cloud.ping.waiting = true;
    cloud.ping.frame_next = CA_FRAME + SECS_TO_FRAMES(PING_FREQ_SECS);

    // send it
    cloudWriteByte(CMD_PING);
}

/**
 * Drives the Cloud Admin connection, once per server frame. It collects
 * the result of the DNS lookup once that finishes. Depending on
 * the state it sends anything queued and reads and acts on anything
 * received, pings the backend, checks whether a connection in progress
 * has finished, or starts a new connection attempt. Everything is
 * non-blocking, so each step either finishes straight away or is picked up
 * again on a later frame.
 *
 * The frame counter, cloud.frame_number, is advanced here and is the
 * clock all the connection's timers use. Servers can run faster than the
 * default 10 frames a second (q2pro goes up to 60), so timers are worked
 * out with SECS_TO_FRAMES() rather than assumed.
 *
 * Takes no parameters and returns nothing.
 *
 * Called from G_RunFrame() (g_main.c), right after the mod's own frame.
 */
void cloudFrame(void) {
    cloud.frame_number++;
    if (cloud.state == CLOUD_STATE_DISABLED) {
        return;
    }

    cloudFinishLookup();

    if (cloud.state >= CLOUD_STATE_CONNECTED) {
        cloudSendMessages();
        cloudReadMessages();
    }

    if (cloud.state == CLOUD_STATE_TRUSTED) {
        cloudPing();
    }

    // connection started already, check for completion
    if (cloud.state == CLOUD_STATE_CONNECTING) {
        cloudCheckConnection();
    }

    if (cloud.state == CLOUD_STATE_DISCONNECTED) {
        cloudConnect();
    }
}

/**
 * Tells the backend this server is going away (CMD_QUIT), closes the
 * connection and frees the resolved address. CMD_QUIT is sent directly
 * rather than left in the queue, because there won't be another frame to
 * send it.
 *
 * Takes no parameters and returns nothing. Does nothing if Cloud Admin is
 * disabled.
 *
 * Called from ShutdownGame() (g_main.c).
 */
void cloudShutdown(void) {
    if (cloud.state == CLOUD_STATE_DISABLED) {
        return;
    }
    // We have to call cloudSendMessages() specifically here because there won't
    // be another frame to send the buffered CMD_QUIT
    cloudWriteByte(CMD_QUIT);
    cloudSendMessages();
    cloudDisconnect();
    cloudAbandonLookup();
    cloudFreeAddress();
}

/**
 * Closes the connection from this end and sets the state back to
 * CLOUD_STATE_DISCONNECTED, logging the disconnect. A deliberate close,
 * unlike cloudPeerDisconnected(), so no reconnect is scheduled; cloudFrame()
 * will reconnect once cloud.connect_retry_frame passes, unless the caller
 * disables Cloud Admin or resets the state first.
 *
 * Takes no parameters and returns nothing. Does nothing if not connected.
 *
 * Called from cloudShutdown() and from cloudRun() for "sv !cloud
 * disconnect" and "sv !cloud reconnect".
 */
void cloudDisconnect(void) {
    if (cloud.state < CLOUD_STATE_CONNECTED) {
        return;
    }
    closesocket(cloud.connection.socket);
    cloud.state = CLOUD_STATE_DISCONNECTED;
    logEvent(LT_CLOUDDISCONNECT, 0, NULL, "closed locally", cloud.disconnect_count, 0.0, false);
}

/**
 * Works out when to try connecting again after a failed attempt. The wait
 * grows with the number of failures in a row: every 10 seconds for the
 * first 12 attempts (about 2 minutes), then every 30 seconds, every minute
 * from the 20th attempt and every 2 minutes from the 50th. A backend that's
 * down for a while isn't hammered, but a short outage is still recovered
 * from quickly.
 *
 * Takes no parameters.
 *
 * Returns the frame number (on the cloud.frame_number clock) of the next
 * attempt.
 *
 * Called from cloudConnect() when a socket can't be created, and from
 * cloudCheckConnection() when a connection attempt fails.
 */
static uint32_t cloudNextConnectFrame(void) {
    if (cloud.connection_attempts < 12) {  // 2 minutes
        return FUTURE_CA_FRAME(10);
    } else if (cloud.connection_attempts < 20) { // 10 minutes
        return FUTURE_CA_FRAME(30);
    } else if (cloud.connection_attempts < 50) { // 30 minutes
        return FUTURE_CA_FRAME(60);
    } else {
        return FUTURE_CA_FRAME(120);
    }
}

/**
 * Starts a non-blocking TCP connection to the backend, once
 * cloud.connect_retry_frame has passed. Both message queues are emptied
 * first, so nothing left over from an earlier connection is sent or
 * parsed. The socket is put in non-blocking mode so connect() returns
 * straight away instead of stalling the server until the backend answers.
 * The state is set to CLOUD_STATE_CONNECTING, and cloudCheckConnection()
 * finds out on a later frame whether the connection completed.
 *
 * Nothing happens until the DNS lookup has filled cloud.addr. If the
 * socket can't be created, the error is logged and the next attempt is
 * scheduled with cloudNextConnectFrame(). If it can't be made
 * non-blocking, it's closed and tried again in 30 seconds.
 *
 * Takes no parameters and returns nothing.
 *
 * Called from cloudFrame() while the state is CLOUD_STATE_DISCONNECTED.
 */
void cloudConnect(void) {
    int flags, ret;

    if (cloud.frame_number < cloud.connect_retry_frame) {
        return;
    }

    // the DNS lookup hasn't finished yet
    if (!cloud.addr) {
        return;
    }

    q2a_memset(&cloud.queue_out, 0, sizeof(message_queue_t));
    q2a_memset(&cloud.queue_in, 0, sizeof(message_queue_t));
    q2a_memset(&cloud.queue_recv, 0, sizeof(message_queue_t));

    cloud.state = CLOUD_STATE_CONNECTING;
    cloud.connection_attempts++;
    cloud.connection.socket = socket(
            cloud.addr->ai_family,
            cloud.addr->ai_socktype,
            cloud.addr->ai_protocol
    );

    if (cloud.connection.socket == -1) {
        perror("connect");
        logEvent(LT_CLOUDERROR, 0, NULL,
            va("cannot create socket: %s", strerror(errno)), errno, 0.0, false);
        errno = 0;
        cloud.connect_retry_frame = cloudNextConnectFrame();
        return;
    }

// Make it non-blocking.
// preferred method is using POSIX O_NONBLOCK, if available
#if defined(O_NONBLOCK)
    flags = 0;
    ret = fcntl(cloud.connection.socket, F_SETFL, flags | O_NONBLOCK);
#else
    flags = 1;
    ret = ioctlsocket(cloud.connection.socket, FIONBIO, (long unsigned int *) &flags);
#endif

    if (ret == -1) {
        cloudPrintf("error setting socket to non-blocking: (%d) %s\n", errno, strerror(errno));
        logEvent(LT_CLOUDERROR, 0, NULL,
            va("cannot set socket non-blocking: %s", strerror(errno)), errno, 0.0, false);
        errno = 0;
        // A blocking connect() would freeze the server until the backend
        // answers, so give up on this attempt instead.
        closesocket(cloud.connection.socket);
        cloud.state = CLOUD_STATE_DISCONNECTED;
        cloud.connect_retry_frame = FUTURE_CA_FRAME(30);
        return;
    }
    errno = 0;
    ret = connect(cloud.connection.socket, cloud.addr->ai_addr, cloud.addr->ai_addrlen);
    if (ret == -1) {
        if (errno == EINPROGRESS) {
            // expected
        } else {
            perror("[cloud] connect error");
            errno = 0;
        }
    }

    // Since we're non-blocking, the connection won't complete in this single
    // server frame. We have to select() for it on a later runframe. See
    // cloudCheckConnection()
}

/**
 * Checks, without waiting, whether the connection cloudConnect() started
 * has finished. select() says when the socket is writable; on Linux the
 * socket's pending error (SO_ERROR) is checked as well, because a socket
 * whose connection failed also shows up as writable. getpeername() then
 * confirms a peer is really there.
 *
 * Once connected, the state becomes CLOUD_STATE_CONNECTED, the first ping
 * is scheduled and cloudSayHello() starts the handshake. If select() fails,
 * the attempt failed (Linux) or there's no peer, the socket is closed and
 * another attempt is scheduled. If the connection simply hasn't finished yet, nothing changes
 * and the check runs again next frame.
 *
 * Takes no parameters and returns nothing.
 *
 * Called from cloudFrame() while the state is CLOUD_STATE_CONNECTING.
 */
void cloudCheckConnection(void) {
    cloud_connection_t *c;
    uint32_t ret;
    bool connected = false;
    struct sockaddr_storage addr;
    socklen_t len = sizeof(addr);
    struct timeval tv;
    tv.tv_sec = tv.tv_usec = 0;

    c = &cloud.connection;

    FD_ZERO(&c->set_w);
    FD_ZERO(&c->set_e);
    FD_SET(c->socket, &c->set_w);
    FD_SET(c->socket, &c->set_e);

    // check if connection is fully established
    ret = select((int)c->socket + 1, NULL, &c->set_w, &c->set_e, &tv);
    if (ret) {

#ifdef LINUX
        int number = 0;
        socklen_t optlen = sizeof(number);

        getsockopt(c->socket, SOL_SOCKET, SO_ERROR, &number, &optlen);
        if (number == 0) {
            connected = true;
        } else {
            // the connection attempt failed (refused, unreachable...)
            cloudPrintf("connection failed: %s\n", strerror(number));
            logEvent(LT_CLOUDERROR, 0, NULL,
                va("connection failed: %s", strerror(number)), number, 0.0, false);
            closesocket(c->socket);
            cloud.state = CLOUD_STATE_DISCONNECTED;
            cloud.connect_retry_frame = cloudNextConnectFrame();
            return;
        }
#else
        if (FD_ISSET(c->socket, &c->set_w)) {
            connected = true;
        }
#endif
    }

    if (ret == -1) {
        perror("CheckConnection");
        cloudPrintf("connection unfinished: %s\n", strerror(errno));
        logEvent(LT_CLOUDERROR, 0, NULL,
            va("connection unfinished: %s", strerror(errno)), errno, 0.0, false);
        closesocket(c->socket);
        cloud.state = CLOUD_STATE_DISCONNECTED;
        cloud.connect_retry_frame = FUTURE_CA_FRAME(10);
        return;
    }

    // we need to make sure it's actually connected
    if (connected) {
        errno = 0;
        if (getpeername(c->socket, (struct sockaddr *)&addr, &len) == -1) {
            // Snapshot errno before logEvent, which writes to a file and can
            // clobber it.
            int err = errno;
            logEvent(LT_CLOUDERROR, 0, NULL,
                va("peer not connected: %s", strerror(err)), err, 0.0, false);
            cloud.connect_retry_frame = FUTURE_CA_FRAME(30);
            cloud.state = CLOUD_STATE_DISCONNECTED;
            closesocket(c->socket);
        } else {
            cloudPrintf("connected\n");
            cloud.state = CLOUD_STATE_CONNECTED;
            cloud.ping.frame_next = FUTURE_CA_FRAME(10);
            cloud.connected_frame = CA_FRAME;
            cloudSayHello();
        }
    }
}

/**
 * Turns everything in the outgoing queue into one frame (see
 * CLOUD_FRAME_HEADER in g_cloud.h): encrypts it if encryption is in use,
 * then puts its length in front. The frame replaces the queue's contents
 * and queue_out.framed is set to its size.
 *
 * encrypt: whether to encrypt the payload with the session key.
 *
 * Returns true if the frame was made, false if encryption failed, in which
 * case the queue is emptied, since its contents can't be sent.
 *
 * Called from cloudSendMessages().
 */
static bool cloudFrameQueue(bool encrypt) {
    message_queue_t *q = &cloud.queue_out;
    static byte frame[QUEUE_SIZE + AESBLOCK_LEN];
    size_t len;

    // The writes stop at QUEUE_WRITE_MAX, so the header and any padding
    // always fit.
    if (encrypt) {
        len = cryptoSymmetricEncrypt(frame + CLOUD_FRAME_HEADER, q->data, q->length);
        if (len == 0 || len > CLOUD_FRAME_MAX) {
            cloudDPrintf("encryption failed, dropping %" PRIz " bytes\n", q->length);
            q->length = 0;
            q->framed = 0;
            return false;
        }
    } else {
        len = q->length;
        q2a_memcpy(frame + CLOUD_FRAME_HEADER, q->data, len);
    }

    frame[0] = len & 0xff;
    frame[1] = (len >> 8) & 0xff;
    frame[2] = (len >> 16) & 0xff;
    frame[3] = (len >> 24) & 0xff;

    q->length = CLOUD_FRAME_HEADER + len;
    q->framed = q->length;
    q2a_memcpy(q->data, frame, q->length);
    return true;
}

/**
 * Sends as much of the outgoing queue (cloud.queue_out) as the socket will
 * take right now, without waiting. Messages are built up in the queue by
 * the cloudWrite*() functions during a frame and sent here in one go.
 * Whatever is sent is removed from the front of the queue, and anything
 * the socket couldn't take stays for the next frame.
 *
 * Before sending, the queue is made into a frame with its length in front
 * (see cloudFrameQueue()), encrypted with the session key once the
 * connection is trusted and encryption is on. queue_out.framed counts the
 * part of the frame still waiting at the front, so after a partial send
 * the rest goes out as it is, and anything queued since waits until it has
 * gone and then becomes the next frame.
 *
 * If the backend has closed its end (EPIPE), cloudPeerDisconnected()
 * handles the lost connection.
 *
 * Takes no parameters and returns nothing.
 *
 * Called from cloudFrame() every frame while connected, and directly from
 * cloudShutdown() and cloudVerifyServerAuth() when a message can't wait
 * for the next frame.
 */
void cloudSendMessages(void) {
    if (cloud.state < CLOUD_STATE_CONNECTING) {
        return;
    }

    if (!cloud.queue_out.length) {
        return;
    }

    int ret;
    bool encrypt;
    struct timeval tv;
    tv.tv_sec = tv.tv_usec = 0;
    cloud_connection_t *c;
    message_queue_t *q;

    c = &cloud.connection;
    q = &cloud.queue_out;
    encrypt = c->encrypted && c->have_keys && cloud.state == CLOUD_STATE_TRUSTED;

    while (q->length) {
        FD_ZERO(&c->set_w);
        FD_SET(c->socket, &c->set_w);

        // see if the socket is ready to send data
        ret = select((int) c->socket + 1, NULL, &c->set_w, NULL, &tv);
        if (ret == -1) {
            perror("send select");
            errno = 0;
            break;
        }

        // socket write buffer is full, try again next frame
        if (ret == 0) {
            break;
        }

        // Frame the queue only once the last frame has all gone. If send()
        // takes just part of a frame, the rest must go out as it is;
        // framing or encrypting it again would garble it.
        if (!q->framed && !cloudFrameQueue(encrypt)) {
            break;
        }

        // anything queued after the frame waits for the next pass
        ret = send(c->socket, (const char *) q->data, q->framed, 0);
        if (ret == -1) {
            if (errno == EPIPE) {
                gi.cprintf(NULL, PRINT_HIGH, "Remote side disconnected\n");
                cloudPeerDisconnected();
            } else {
                perror("send error");
            }
            errno = 0;
            break;
        }

        // shift off the data we just sent
        q2a_memmove(q->data, q->data + ret, q->length - ret);
        q->length -= ret;
        q->framed -= ret;
    }
}

/**
 * Handles one whole frame from the backend: decrypts its payload if
 * encryption is in use, puts the commands in the incoming queue
 * (cloud.queue_in) and has cloudParseMessage() act on them.
 *
 * Frames are handled one at a time, so whether a frame is encrypted is
 * decided after the frames before it have been acted on. That matters
 * for the frame after SCMD_TRUSTED, which is the first encrypted one,
 * even when both arrive in the same recv().
 *
 * After decrypting, the first ciphertext block becomes the IV, as it did
 * before frames were added.
 *
 * payload: the frame's payload.
 * len:     its length, at most CLOUD_FRAME_MAX.
 *
 * Returns nothing.
 *
 * Called from cloudReadFrames().
 */
static void cloudParseFrame(byte *payload, size_t len) {
    message_queue_t *in = &cloud.queue_in;
    static byte dec[QUEUE_SIZE + AESBLOCK_LEN];
    size_t declen;

    q2a_memset(in, 0, sizeof(message_queue_t));

    if (cloud.connection.encrypted && cloud.connection.have_keys && cloud.state == CLOUD_STATE_TRUSTED) {
        declen = cryptoSymmetricDecrypt(dec, payload, len);
        if (declen > len) {
            declen = 0;
        }
        q2a_memcpy(in->data, dec, declen);
        in->length = declen;
        if (len >= AES_IV_LEN) {
            q2a_memcpy(cloud.connection.initial_value, payload, AES_IV_LEN);
        }
    } else {
        q2a_memcpy(in->data, payload, len);
        in->length = len;
    }

    cloudParseMessage();
}

/**
 * Takes every whole frame out of the raw receive buffer (cloud.queue_recv)
 * and handles it with cloudParseFrame(). A frame that has only partly
 * arrived is left at the front of the buffer until the rest comes. A
 * frame longer than CLOUD_FRAME_MAX is a protocol error: there's no way to
 * find the next frame after it, so the connection is dropped.
 *
 * Takes no parameters.
 *
 * Returns false if the connection was dropped or closed while handling
 * the frames, so the caller should stop reading; true otherwise.
 *
 * Called from cloudReadMessages() after each recv().
 */
static bool cloudReadFrames(void) {
    message_queue_t *raw = &cloud.queue_recv;
    size_t offset = 0;
    size_t len;
    byte *p;

    while (raw->length - offset >= CLOUD_FRAME_HEADER) {
        p = raw->data + offset;
        len = p[0] | (p[1] << 8) | (p[2] << 16) | ((size_t) p[3] << 24);

        if (len > CLOUD_FRAME_MAX) {
            cloudPrintf("bad frame from backend (%" PRIz " bytes), dropping connection\n", len);
            logEvent(LT_CLOUDERROR, 0, NULL,
                va("bad frame length %" PRIz, len), (int) len, 0.0, false);
            cloudPeerDisconnected();
            return false;
        }

        // the rest of this frame hasn't arrived yet
        if (raw->length - offset - CLOUD_FRAME_HEADER < len) {
            break;
        }

        cloudParseFrame(p + CLOUD_FRAME_HEADER, len);
        offset += CLOUD_FRAME_HEADER + len;

        // a fatal error from the backend closes the connection
        if (cloud.state < CLOUD_STATE_CONNECTED) {
            return false;
        }
    }

    // keep the partial frame, if any, at the front for next time
    q2a_memmove(raw->data, raw->data + offset, raw->length - offset);
    raw->length -= offset;
    return true;
}

/**
 * Reads everything the backend has sent so far, without waiting, into the
 * raw receive buffer (cloud.queue_recv), and handles each whole frame as
 * soon as it has arrived (see cloudReadFrames()). Part of a frame stays in
 * the buffer until the rest arrives on a later read. Reading stops when
 * there's nothing more waiting.
 *
 * The backend closing the connection, or a read error, is handled by
 * cloudPeerDisconnected().
 *
 * Takes no parameters and returns nothing.
 *
 * Called from cloudFrame() every frame while connected.
 */
void cloudReadMessages(void) {
    int ret;
    struct timeval tv;
    message_queue_t *raw;

    if (cloud.state < CLOUD_STATE_CONNECTING) {
        return;
    }

    tv.tv_sec = tv.tv_usec = 0;
    raw = &cloud.queue_recv;

    while (true) {
        FD_ZERO(&cloud.connection.set_r);
        FD_SET(cloud.connection.socket, &cloud.connection.set_r);

        // see if there is data waiting in the buffer for us to read
        ret = select(cloud.connection.socket + 1, &cloud.connection.set_r, NULL, NULL, &tv);
        if (ret == -1) {
            if (errno != EINTR) {
                perror("select");
                cloudPeerDisconnected();
                errno = 0;
                return;
            }
            errno = 0;
            continue;
        }

        // no data has been sent to read
        if (ret == 0) {
            break;
        }

        // Can't happen: a frame is at most QUEUE_SIZE bytes with its
        // header, and whole frames are taken out after every read. Checked
        // anyway rather than overflowing raw->data[].
        if (raw->length >= QUEUE_SIZE) {
            break;
        }

        ret = recv(cloud.connection.socket, (char *) raw->data + raw->length,
                QUEUE_SIZE - raw->length, 0);

        if (ret == 0) {
            cloudPeerDisconnected();
            return;
        }
        if (ret == -1) {
            if (errno != EINTR) {
                perror("recv");
                cloudPeerDisconnected();
                return;
            }
            // interrupted before anything was read, try again
            errno = 0;
            continue;
        }
        raw->length += ret;

        if (!cloudReadFrames()) {
            return;
        }
    }
}

/**
 * Marks the connection as trusted, after the backend has confirmed this
 * server passed authentication (SCMD_TRUSTED). From here on, game events
 * are sent, backend commands are accepted, and traffic is encrypted if
 * encryption is on. This is also when the connection is logged as made,
 * rather than when the socket opened, because it isn't usable until now.
 *
 * Takes no parameters and returns nothing.
 *
 * Called from cloudParseMessage() on SCMD_TRUSTED, which then sends the
 * current map and player list.
 */
void cloudTrusted(void) {
    cloudDPrintf("connection trusted\n");
    cloud.state = CLOUD_STATE_TRUSTED;

    // Logged here rather than when the socket comes up in
    // cloudCheckConnection(), because the link isn't usable until the server
    // has authenticated us. A connection that gets that far and no further
    // shows up as a CLOUDERROR instead.
    logEvent(LT_CLOUDCONNECT, 0, NULL,
        va("%s:%d", cloud_config.address, cloud_config.port), 0, 0.0, false);
}

/**
 * Works through the commands from one frame in the incoming queue, one
 * command at a time.
 * Each command starts with a byte saying what it is (an SCMD_* value),
 * followed by its own data, which the matching handler reads. Unknown
 * command bytes are skipped. Parsing stops if a handler reads past the end
 * of the data received. The queue is emptied afterwards.
 *
 * Takes no parameters and returns nothing.
 *
 * Called from cloudParseFrame() for each frame received.
 */
void cloudParseMessage(void) {
    message_queue_t *msg = &cloud.queue_in;
    byte cmd;

    if (cloud.state == CLOUD_STATE_DISABLED) {
        return;
    }

    if (!cloud.queue_in.length) {
        return;
    }

    msg->overrun = false;
    while (msg->index < msg->length && !msg->overrun) {
        cmd = cloudReadByte();
        switch (cmd) {
        case SCMD_PONG:
            cloudParsePong();
            break;
        case SCMD_COMMAND:
            cloudParseCommand();
            break;
        case SCMD_HELLOACK:
            cloudVerifyServerAuth();
            break;
        case SCMD_TRUSTED:  // we just connected and authed successfully
            cloudTrusted();
            cloudMap(cloud.mapname);
            cloudPlayerList();
            break;
        case SCMD_ERROR:
            cloudParseError();
            break;
        case SCMD_SAYCLIENT:
            cloudSayClient();
            break;
        case SCMD_SAYALL:
            cloudSayAll();
            break;
        case SCMD_KEY:
            cloudRotateKeys();
            break;
        case SCMD_GETPLAYERS:
            cloudPlayerList();
            break;
        }
    }

    if (msg->overrun) {
        cloudDPrintf("message from backend was cut short, ignored the rest\n");
    }

    // reset queue back to zero
    q2a_memset(&cloud.queue_in, 0, sizeof(message_queue_t));
}

/**
 * Checks an RSA signature: whether MsgHash is a valid SHA-256 signature
 * of Msg made with the private key matching rsa.
 *
 * rsa:        the signer's public key. Ownership passes to an EVP_PKEY made
 *             here, so the caller mustn't free it.
 * MsgHash:    the signature to check.
 * MsgHashLen: length of MsgHash.
 * Msg:        the message that was signed.
 * MsgLen:     length of Msg.
 * Authentic:  set to true if the signature is valid, false otherwise.
 *
 * Returns true if the check could be carried out (whatever its answer, which
 * is in Authentic), or false if OpenSSL failed.
 *
 * Nothing calls this at the moment; the handshake proves identity with
 * encrypted challenges instead (see cloudVerifyServerAuth()).
 */
bool RSAVerifySignature( RSA* rsa,
                         unsigned char* MsgHash,
                         size_t MsgHashLen,
                         const char* Msg,
                         size_t MsgLen,
                         bool* Authentic) {
    *Authentic = false;
    EVP_PKEY* pubKey  = EVP_PKEY_new();
    EVP_PKEY_assign_RSA(pubKey, rsa);
    EVP_MD_CTX* m_RSAVerifyCtx = EVP_MD_CTX_new();

    if (EVP_DigestVerifyInit(m_RSAVerifyCtx,NULL, EVP_sha256(),NULL,pubKey)<=0) {
        return false;
    }

    if (EVP_DigestVerifyUpdate(m_RSAVerifyCtx, Msg, MsgLen) <= 0) {
      return false;
    }

    int AuthStatus = EVP_DigestVerifyFinal(m_RSAVerifyCtx, MsgHash, MsgHashLen);
    if (AuthStatus == 1) {
        *Authentic = true;
        EVP_MD_CTX_free(m_RSAVerifyCtx);
        return true;
    } else if (AuthStatus == 0) {
        *Authentic = false;
        EVP_MD_CTX_free(m_RSAVerifyCtx);
        return true;
    } else {
        *Authentic = false;
        EVP_MD_CTX_free(m_RSAVerifyCtx);
        return false;
    }
}

/**
 * Handles the backend's answer to our hello (SCMD_HELLOACK), the middle of
 * the handshake that proves each side is who it says it is. The answer is
 * encrypted with this server's public key, so only we can read it. Inside
 * are:
 *
 * 1. A hash of the random challenge we sent in cloudSayHello(). Only the
 *    real backend could have decrypted that challenge, so a matching hash
 *    proves the backend's identity.
 * 2. A random challenge from the backend, which we hash and send back
 *    encrypted with the backend's public key (CMD_AUTH), proving our
 *    identity in turn.
 * 3. When encryption is on, the AES session key and IV used for all
 *    traffic after this.
 *
 * On a mismatch the failure is logged, and after AUTH_FAIL_LIMIT failures
 * in a row Cloud Admin is disabled rather than retrying forever against a
 * backend that can't prove itself.
 *
 * Takes no parameters; reads the answer from the incoming queue.
 *
 * Returns true if the backend was authenticated and our answer was sent,
 * false if its answer was cut short, too large or too small, couldn't be
 * decrypted or didn't match.
 *
 * Called from cloudParseMessage() on SCMD_HELLOACK.
 */
bool cloudVerifyServerAuth(void) {
    cloud_connection_t *c = &cloud.connection;
    size_t dec_len;                     // Length of decrypted cleartext
    size_t enc_len;                     // Length of encrypted ciphertext
    byte response[RSA_LEN];             // Ciphertext
    byte response_plain[RSA_LEN];       // Cleartext version of response
    byte challenge_hash[DIGEST_LEN];    // The hash we calculate
    byte response_hash[DIGEST_LEN];     // Message digest of a challenge
    byte sv_challenge[CHALLENGE_LEN];   // The nonce sent from the server
    uint8_t offset = 0;                 // Used to find the parts of
                                        // server's auth response
    uint16_t resp_len;                  // Peer-supplied length, must be
                                        // validated before use below

    q2a_memset(response, 0, sizeof(response));
    resp_len = cloudReadShort();
    if (resp_len > sizeof(response)) {
        cloudDPrintf("server auth response too large (%u bytes), dropping\n", resp_len);
        return false;
    }
    cloudReadData(response, resp_len);
    if (cloud.queue_in.overrun) {
        cloudDPrintf("server auth response cut short, dropping\n");
        return false;
    }
    q2a_memset(response_plain, 0, sizeof(response_plain));
    dec_len = cryptoPrivateDecrypt(response_plain, response, resp_len);
    if (dec_len == 0) {
        cloudDPrintf("zero bytes decrypted for server authentication\n");
        return false;
    }

    // make sure every part read below was actually sent
    size_t needed = DIGEST_LEN + CHALLENGE_LEN;
    if (cloud_config.encryption) {
        needed += AESKEY_LEN + AES_IV_LEN;
    }
    if (dec_len < needed) {
        cloudDPrintf("server auth response too short (%" PRIz " bytes), dropping\n", dec_len);
        return false;
    }

    // this is our original nonce hashed by the server and sent back to us
    q2a_memset(response_hash, 0, sizeof(response_hash));
    q2a_memcpy(response_hash, response_plain + offset, sizeof(response_hash));
    offset += sizeof(response_hash);

    // random nonce generated by server (we hash it and send back)
    q2a_memset(sv_challenge, 0, sizeof(sv_challenge));
    q2a_memcpy(sv_challenge, response_plain + offset, sizeof(sv_challenge));
    offset += sizeof(sv_challenge);

    // transit will be encrypted so the keys will be next in the buffer
    if (cloud_config.encryption) {
        q2a_memset(c->session_key, 0, sizeof(c->session_key));
        q2a_memcpy(c->session_key, response_plain + offset, sizeof(c->session_key));
        offset += sizeof(c->session_key);

        q2a_memset(c->initial_value, 0, sizeof(c->initial_value));
        q2a_memcpy(c->initial_value, response_plain + offset, sizeof(c->initial_value));

        c->have_keys = true;

        // reuse encrypt/decrypt contexts, creating them the first time
        if (!c->e_ctx) {
            c->e_ctx = EVP_CIPHER_CTX_new();
        }
        if (!c->d_ctx) {
            c->d_ctx = EVP_CIPHER_CTX_new();
        }
    }

    // to compare with what server sent back to us
    cryptoMessageDigest(challenge_hash, c->cl_nonce, CHALLENGE_LEN);

    // if the hashes match, server is authenticated
    if (q2a_memcmp(challenge_hash, response_hash, DIGEST_LEN) == 0) {
        cloudDPrintf("server authenticated\n");

        // reuse response and challenge_hash for our auth to server
        q2a_memset(response, 0, sizeof(response));
        q2a_memset(challenge_hash, 0, sizeof(challenge_hash));
        cryptoMessageDigest(challenge_hash, sv_challenge, sizeof(sv_challenge));
        enc_len = cryptoPublicEncrypt(cloud.connection.server_key, response, challenge_hash, DIGEST_LEN);

        // send our response to server's challenge
        cloudWriteByte(CMD_AUTH);
        cloudWriteShort(enc_len);
        cloudWriteData(response, enc_len);
        cloudSendMessages();

        cloud.connection_attempts = 0;
        cloud.connection.auth_fail_count = 0;
        return true;
    } else {
        // ERR_get_error() pops the error off the queue, so read it once and
        // reuse it for both the console and the log.
        char *sslerr = ERR_error_string(ERR_get_error(), NULL);

        cloudDPrintf("server auth error: %s\n", sslerr);
        logEvent(LT_CLOUDERROR, 0, NULL, va("server auth error: %s", sslerr),
            cloud.connection.auth_fail_count, 0.0, false);
        cloud.connection.auth_fail_count++;

        if (cloud.connection.auth_fail_count > AUTH_FAIL_LIMIT) {
            cloudDPrintf("too many auth failures, giving up\n");
            logEvent(LT_CLOUDERROR, 0, NULL, "too many auth failures, giving up",
                cloud.connection.auth_fail_count, 0.0, false);
            cloud.state = CLOUD_STATE_DISABLED;
        }
        return false;
    }
}

/**
 * Runs a console command sent by the backend (SCMD_COMMAND), exactly as
 * if it had been typed at the server console. That's how the backend
 * manages the server remotely. Commands are only accepted once the
 * connection is trusted, which is the only check: whatever a trusted
 * backend sends is run.
 *
 * Takes no parameters and returns nothing; reads the command from the
 * incoming queue.
 *
 * Called from cloudParseMessage() on SCMD_COMMAND.
 */
void cloudParseCommand(void) {
    char *cmd;

    if (cloud.state < CLOUD_STATE_TRUSTED) {
        return;
    }
    cmd = cloudReadString();
    // a command cut short could do something other than what was sent
    if (cloud.queue_in.overrun) {
        return;
    }
    gi.AddCommandString(cmd);
}

/**
 * Handles the backend's answer to a ping (SCMD_PONG): clears the
 * outstanding ping and the missed-ping count, so cloudPing() knows the
 * connection is alive.
 *
 * Takes no parameters and returns nothing.
 *
 * Called from cloudParseMessage() on SCMD_PONG.
 */
void cloudParsePong(void) {
    cloud.ping.waiting = false;
    cloud.ping.miss_count = 0;
}

/**
 * Replaces the AES session key and IV with new ones from the backend
 * (SCMD_KEY). Changing keys now and then limits how much traffic is ever
 * encrypted with any one key. They take effect from the next message sent
 * or received.
 *
 * Takes no parameters and returns nothing; reads the key and IV from the
 * incoming queue.
 *
 * Called from cloudParseMessage() on SCMD_KEY.
 */
void cloudRotateKeys(void) {
    cloud_connection_t *c;
    byte key[AESKEY_LEN], iv[AESBLOCK_LEN];
    c = &cloud.connection;

    // read into temporaries so a short message can't leave half a key
    cloudReadData(key, sizeof(key));
    cloudReadData(iv, sizeof(iv));
    if (cloud.queue_in.overrun) {
        return;
    }
    q2a_memcpy(c->session_key, key, sizeof(key));
    q2a_memcpy(c->initial_value, iv, sizeof(iv));
}

/**
 * Cleans up after the connection was lost unexpectedly: the backend closed
 * it, a read or send failed, or too many pings went unanswered. The
 * socket is closed, the session keys are wiped, the disconnect is logged
 * and counted, and a reconnect is scheduled for 10 seconds plus a random
 * extra delay. The random part keeps many servers that lost the same
 * backend at once from all reconnecting at the same moment.
 *
 * Takes no parameters and returns nothing. Does nothing if not connected.
 *
 * Called from cloudPing(), cloudSendMessages(), cloudReadMessages() and
 * cloudReadFrames().
 */
void cloudPeerDisconnected(void) {
    uint8_t secs;

    if (cloud.state < CLOUD_STATE_CONNECTED) {
        return;
    }

    cloudPrintf("connection lost\n");

    cloud.state = CLOUD_STATE_DISCONNECTED;
    cloud.connection.trusted = false;
    cloud.connection.have_keys = false;
    cloud.disconnect_count++;
    logEvent(LT_CLOUDDISCONNECT, 0, NULL, 
        va("%s:%d connection lost", cloud_config.address, cloud_config.port),
        cloud.disconnect_count, 0.0, false);

    closesocket(cloud.connection.socket);
    FD_CLR(cloud.connection.socket, &cloud.connection.set_r);
    FD_CLR(cloud.connection.socket, &cloud.connection.set_w);
    FD_CLR(cloud.connection.socket, &cloud.connection.set_e);

    q2a_memset(&cloud.connection.session_key[0], 0, AESKEY_LEN);
    q2a_memset(&cloud.connection.initial_value[0], 0, AESBLOCK_LEN);

    // try reconnecting a reasonably random amount of time later
    srand((unsigned) time(NULL));
    secs = rand() & 0xff;
    cloud.connect_retry_frame = FUTURE_CA_FRAME(10) + secs;
    cloudDPrintf("trying to reconnect in %d seconds\n",
        FRAMES_TO_SECS(cloud.connect_retry_frame - cloud.frame_number)
    );
}

/**
 * Queues a CMD_PLAYERLIST message describing every connected player, so
 * the backend has a full picture of the server. It holds the number of
 * players, then for each one their slot number, raw userinfo string and
 * client version string.
 *
 * Takes no parameters and returns nothing. Does nothing unless the
 * connection is trusted.
 *
 * Called from cloudParseMessage(): right after the connection becomes
 * trusted, and whenever the backend asks for it (SCMD_GETPLAYERS).
 */
void cloudPlayerList(void) {
    uint8_t count, i;
    count = 0;

    if (cloud.state < CLOUD_STATE_TRUSTED) {
        return;
    }

    for (i=0; i<cloud.maxclients; i++) {
        if (proxyinfo[i].inuse) {
            count++;
        }
    }

    cloudWriteByte(CMD_PLAYERLIST);
    cloudWriteByte(count);
    for (i=0; i<cloud.maxclients; i++) {
        if (proxyinfo[i].inuse) {
            cloudWriteByte(i);
            cloudWriteString("%s", proxyinfo[i].userinfo.raw);
            cloudWriteString("%s", proxyinfo[i].client_version);
        }
    }
}

/**
 * Queues the first message of the handshake (CMD_HELLO), sent as soon as
 * the connection opens. It tells the backend who this server is: its
 * UUID, q2admin revision, game port, player slots and whether it wants
 * encryption. It ends with a random challenge encrypted with the
 * backend's public key. Only the real backend can decrypt it, and its
 * reply is checked by cloudVerifyServerAuth().
 *
 * The challenge is kept in cloud.connection.cl_nonce for that check.
 *
 * Takes no parameters and returns nothing. Does nothing if the connection
 * is already trusted.
 *
 * Called from cloudCheckConnection() once the connection completes.
 */
void cloudSayHello(void) {
    if (cloud.state == CLOUD_STATE_TRUSTED) {
        return;
    }

    // random data to challenge backend with
    RAND_bytes((unsigned char *)cloud.connection.cl_nonce, sizeof(cloud.connection.cl_nonce));

    byte challenge[RSA_LEN];
    q2a_memset(challenge, 0, sizeof(challenge));
    cryptoPublicEncrypt(cloud.connection.server_key, challenge,
            cloud.connection.cl_nonce, CHALLENGE_LEN);

    cloudWriteLong(MAGIC_CLIENT);
    cloudWriteByte(CMD_HELLO);
    cloudWriteString("%s", cloud_config.uuid);
    cloudWriteLong(Q2A_REVISION);
    cloudWriteShort(cloud.port);
    cloudWriteByte(cloud.maxclients);
    cloudWriteByte(cloud_config.encryption ? 1 : 0);
    cloudWriteData(challenge, RSA_LEN);
}

/**
 * Handles an error reported by the backend (SCMD_ERROR). The message holds
 * the player it's about (-1 if none), an error number and a text reason.
 * The error is logged, and the reason is shown to that player if they're
 * still connected, otherwise on the server console. Error numbers 200 and
 * above are fatal: the connection is closed and Cloud Admin is disabled
 * until it's started again by hand.
 *
 * Takes no parameters and returns nothing; reads the error from the
 * incoming queue.
 *
 * Called from cloudParseMessage() on SCMD_ERROR.
 */
void cloudParseError(void) {
    int client_id;
    uint8_t reason_id;
    char *reason;

    gi.dprintf("parsing error\n");

    // The server sends -1 (0xFF) when the error isn't about a particular
    // player. cloudReadByte() returns it unsigned, so read it back as a
    // signed byte or it would come through as 255 and never equal -1.
    client_id = (int8_t) cloudReadByte();
    reason_id = cloudReadByte();
    reason = cloudReadString();
    if (cloud.queue_in.overrun) {
        return;
    }

    logEvent(LT_CLOUDERROR, 0, NULL, reason ? reason : "", reason_id, 0.0, false);

    // Errors about a player (an !invite or !teleport quota, for example)
    // go to that player, who caused it. Anything else, or an id that no
    // longer belongs to a connected player, goes to the server console.
    if (VALIDCLIENT(client_id) && proxyinfo[client_id].inuse) {
        gi.cprintf(getEnt(client_id + 1), PRINT_HIGH, "%s\n", reason ? reason : "");
    } else {
        gi.cprintf(NULL, PRINT_HIGH, "[cloud] error %d: %s\n", reason_id, reason ? reason : "");
    }

    // serious enough to disconnect
    if (reason_id >= 200) {
        closesocket(cloud.connection.socket);
        cloud.state = CLOUD_STATE_DISABLED;
        cloud.connection.have_keys = false;
        cloudFreeAddress();
        logEvent(LT_CLOUDDISCONNECT, 0, NULL,
            va("disabled by server: %s", reason ? reason : ""),
            cloud.disconnect_count, 0.0, false);
    }
}

/**
 * Finds the UDP port this game server is listening on. Most servers keep
 * it in the "port" cvar, but q2pro uses "net_port", so both are checked,
 * falling back to the Quake 2 default of 27910.
 *
 * Takes no parameters.
 *
 * Returns the port number.
 *
 * Called from SpawnEntities() (g_init.c) to fill cloud.port, which is sent
 * in the hello, and from expandOutPortNum() (g_log.c) for "%p" in log file
 * names.
 */
uint16_t getport(void) {
    static cvar_t *port;

    port = gi.cvar("port", "0", 0);
    if ((int) port->value) {
        return (int) port->value;
    }
    
    port = gi.cvar("net_port", "0", 0);
    if ((int) port->value) {
        return (int) port->value;
    }
    
    // fallback to default
    port = gi.cvar("port", "27910", 0);
    return (int) port->value;
}

/**
 * Checks there are at least n more bytes of received data to read. If not,
 * the incoming queue is marked as overrun and its position moved to the
 * end, so the read returns zeros instead of going past the data received,
 * and cloudParseMessage() stops. Handlers check queue_in.overrun before
 * acting on what they read, so a message cut short is ignored rather than
 * half-applied.
 *
 * n: how many bytes the caller is about to read.
 *
 * Returns true if they're all there.
 *
 * Called from the cloudRead*() functions.
 */
static bool cloudReadAvailable(size_t n) {
    message_queue_t *q = &cloud.queue_in;

    if (q->index > q->length || n > q->length - q->index) {
        q->index = q->length;
        q->overrun = true;
        return false;
    }
    return true;
}

/**
 * Reads one byte from the incoming queue and moves past it. This and the
 * other cloudRead*() functions are how the message handlers take their
 * data apart.
 *
 * Takes no parameters.
 *
 * Returns the byte, or 0 if there's no more data (see
 * cloudReadAvailable()).
 *
 * Called from the message handlers, from cloudParseMessage() for each
 * command byte, and from cloudReadString().
 */
uint8_t cloudReadByte(void) {
    if (!cloudReadAvailable(1)) {
        return 0;
    }
    unsigned char b = cloud.queue_in.data[cloud.queue_in.index++];
    return b & 0xff;
}

/**
 * Adds one byte to the outgoing queue. This and the other cloudWrite*()
 * functions are how outgoing messages are built; cloudSendMessages() sends
 * the queue on the next frame.
 *
 * b: the byte to add.
 *
 * Returns nothing. If the queue is full (QUEUE_WRITE_MAX, which leaves room
 * for encryption padding), the byte is dropped with a debug message.
 *
 * Called throughout this file wherever a message is built.
 */
void cloudWriteByte(uint8_t b) {
    if (cloud.queue_out.length >= QUEUE_WRITE_MAX) {
        cloudDPrintf("outgoing queue full, dropping byte\n");
        return;
    }
    cloud.queue_out.data[cloud.queue_out.length++] = b & 0xff;
}

/**
 * Reads a 2-byte number from the incoming queue and moves past it.
 * Numbers on the wire are little-endian (low byte first).
 *
 * Takes no parameters.
 *
 * Returns the number, or 0 if fewer than 2 bytes are left.
 *
 * Called from cloudVerifyServerAuth() for the length of the backend's
 * answer.
 */
uint16_t cloudReadShort(void) {
    message_queue_t *q = &cloud.queue_in;
    if (!cloudReadAvailable(2)) {
        return 0;
    }
    int s = q->data[q->index] + (q->data[q->index + 1] << 8);
    q->index += 2;
    return s & 0xffff;
}

/**
 * Adds a 2-byte number to the outgoing queue, little-endian (low byte
 * first).
 *
 * s: the number to add.
 *
 * Returns nothing. If there isn't room for both bytes, neither is added.
 *
 * Called from cloudSayHello() and cloudVerifyServerAuth().
 */
void cloudWriteShort(uint16_t s) {
    if (cloud.queue_out.length + 2 > QUEUE_WRITE_MAX) {
        cloudDPrintf("outgoing queue full, dropping short\n");
        return;
    }
    cloud.queue_out.data[cloud.queue_out.length++] = s & 0xff;
    cloud.queue_out.data[cloud.queue_out.length++] = (s >> 8) & 0xff;
}

/**
 * Reads a 4-byte number from the incoming queue, little-endian, and moves
 * past it.
 *
 * Takes no parameters.
 *
 * Returns the number, or 0 if fewer than 4 bytes are left.
 *
 * Nothing calls this at the moment; it's here to match cloudWriteLong().
 */
int32_t cloudReadLong(void) {
    message_queue_t *q = &cloud.queue_in;
    if (!cloudReadAvailable(4)) {
        return 0;
    }
    int num = q->data[q->index] + (q->data[q->index + 1] << 8) +
            (q->data[q->index + 2] << 16) + (q->data[q->index + 3] << 24);
    q->index += 4;
    return num;
}

/**
 * Adds a 4-byte number to the outgoing queue, little-endian (low byte
 * first).
 *
 * i: the number to add.
 *
 * Returns nothing. If there isn't room for all 4 bytes, none are added.
 *
 * Called from cloudSayHello().
 */
void cloudWriteLong(uint32_t i) {
    if (cloud.queue_out.length + 4 > QUEUE_WRITE_MAX) {
        cloudDPrintf("outgoing queue full, dropping long\n");
        return;
    }
    cloud.queue_out.data[cloud.queue_out.length++] = i & 0xff;
    cloud.queue_out.data[cloud.queue_out.length++] = (i >> 8) & 0xff;
    cloud.queue_out.data[cloud.queue_out.length++] = (i >> 16) & 0xff;
    cloud.queue_out.data[cloud.queue_out.length++] = (i >> 24) & 0xff;
}

/**
 * Adds a block of raw bytes to the outgoing queue, such as an encrypted
 * challenge. Unlike cloudWriteString(), no terminator or length is added,
 * so the reader has to know the size already.
 *
 * data:   the bytes to add.
 * length: how many bytes.
 *
 * Returns nothing. Bytes that don't fit are dropped one by one, so a block
 * can be cut short if the queue fills up.
 *
 * Called from cloudSayHello() and cloudVerifyServerAuth().
 */
void cloudWriteData(const void *data, size_t length) {
    uint32_t i;
    for (i=0; i<length; i++) {
        cloudWriteByte(((byte *) data)[i]);
    }
}

/**
 * Reads a NUL-terminated string from the incoming queue and moves past it.
 * Reading stops at the end of the received data, or after
 * MAX_STRING_CHARS - 1 characters, so a string cut short or never
 * terminated can't overrun either buffer. A string that's too long is
 * truncated and the rest skipped; one with no terminator before the end of
 * the data marks the queue as overrun (see cloudReadAvailable()). The top bit of each character is
 * cleared, which turns Quake 2's highlighted text into plain text.
 *
 * Takes no parameters.
 *
 * Returns the string in a static buffer, which the next call overwrites.
 * Copy it if it has to be kept.
 *
 * Called from the handlers for backend commands, errors and messages to
 * players.
 */
char *cloudReadString(void) {
    static char str[MAX_STRING_CHARS];
    message_queue_t *q = &cloud.queue_in;
    size_t i, len = 0;
    bool terminated = false;

    // Never scan past what was actually received, and never past what str[]
    // can hold. The original unbounded scan could walk arbitrarily far past
    // valid data if a message ever arrives split across recv() calls (there's
    // no length-prefix framing on this wire for arbitrary strings), reading
    // out of bounds and then overflowing str[] when reconstructing it below.
    while (q->index + len < q->length && len < MAX_STRING_CHARS - 1) {
        if (q->data[q->index + len] == 0) {
            terminated = true;
            break;
        }
        len++;
    }
    q2a_memset(str, 0, MAX_STRING_CHARS);
    for (i=0; i<len; i++) {
        str[i] = cloudReadByte() & 0x7f;
    }
    if (terminated) {
        cloudReadByte(); // consume the actual NUL terminator
    } else if (q->index >= q->length) {
        // ran out of data before the terminator: the message was cut short
        q->overrun = true;
    } else {
        // Too long for str[]. Skip the rest of it, or it would be parsed as
        // the next command.
        while (q->index < q->length && q->data[q->index] != 0) {
            q->index++;
        }
        if (q->index < q->length) {
            q->index++;
        } else {
            q->overrun = true;
        }
    }
    return str;
}


/**
 * Formats a string printf-style and adds it to the outgoing queue,
 * followed by a NUL terminator. Text longer than MAX_MSG_LEN - 1
 * characters is cut short.
 *
 * fmt: printf-style format.
 * ...: values for fmt.
 *
 * Returns nothing. If the whole string won't fit in the queue, an empty
 * string (just the terminator) is added instead, so the reader still finds
 * a string where it expects one.
 *
 * Called wherever a message includes text: player info, chat, map names
 * and player commands.
 */
void cloudWriteString(const char *fmt, ...) {
    
    uint16_t i;
    size_t len;
    char str[MAX_MSG_LEN];
    va_list argptr;
    
    va_start(argptr, fmt);
    len = vsnprintf(str, sizeof(str), fmt, argptr);
    va_end(argptr);

    len = strlen(str);
    
    if (!*str || len == 0) {
        cloudWriteByte(0);
        return;
    }
    
    // MAX_MSG_LEN only bounds this function's local scratch buffer above
    // (vsnprintf already truncates safely, and len is re-derived via
    // strlen() from the truncated result) - the real constraint is the
    // shared outgoing queue's actual capacity. Checked as addition, never
    // subtraction, so it can't underflow if cloud.queue_out.length is ever
    // unexpectedly large.
    if (cloud.queue_out.length + len + 1 > QUEUE_WRITE_MAX) {
        cloudWriteByte(0);
        return;
    }

    for (i=0; i<len; i++) {
        cloud.queue_out.data[cloud.queue_out.length++] = str[i];
    }

    cloudWriteByte(0);
}

/**
 * Copies a block of raw bytes from the incoming queue and moves past it,
 * the reverse of cloudWriteData().
 *
 * out: where to copy the bytes. Must hold at least len bytes.
 * len: how many bytes to copy.
 *
 * Returns nothing. If fewer than len bytes are left, out is zero-filled
 * instead.
 *
 * Called from cloudVerifyServerAuth() and cloudRotateKeys().
 */
void cloudReadData(void *out, size_t len) {
    if (!cloudReadAvailable(len)) {
        q2a_memset(out, 0, len);
        return;
    }
    q2a_memcpy(out, &(cloud.queue_in.data[cloud.queue_in.index]), len);
    cloud.queue_in.index += len;
}


/**
 * Tells the backend a player has joined (CMD_CONNECT), with their slot
 * number, raw userinfo and client version string. The backend makes
 * decisions from things like the client version, so this is sent only
 * once that's known.
 *
 * ent: the player who joined.
 *
 * Returns nothing. Does nothing unless the connection is trusted.
 *
 * Called from doClientCommand() (g_cmd.c) when the client's version reply
 * arrives and the player has passed the ban checks. That reply can take a
 * moment, which is why this isn't sent from ClientConnect() or
 * ClientBegin().
 */
void cloudPlayerConnect(edict_t *ent) {
    int8_t cl = getEntOffset(ent) - 1;

    if (cloud.state < CLOUD_STATE_TRUSTED) {
        return;
    }
    cloudWriteByte(CMD_CONNECT);
    cloudWriteByte(cl);
    cloudWriteString("%s", proxyinfo[cl].userinfo.raw);
    cloudWriteString("%s", proxyinfo[cl].client_version);
}

/**
 * Tells the backend a player has left (CMD_DISCONNECT), by slot number.
 *
 * ent: the player who left.
 *
 * Returns nothing. Does nothing unless the connection is trusted.
 *
 * Called from ClientDisconnect() (g_init.c).
 */
void cloudPlayerDisconnect(edict_t *ent) {
    int8_t cl = getEntOffset(ent) - 1;

    if (cloud.state < CLOUD_STATE_TRUSTED) {
        return;
    }
    cloudWriteByte(CMD_DISCONNECT);
    cloudWriteByte(cl);
}

/**
 * Forwards a line of server output, such as chat or frag messages, to the
 * backend (CMD_PRINT), so it can be followed remotely. Only sent when the
 * chat flag (CFL_CHAT) is set in cloud_flags.
 *
 * level: the print level the text was printed at (PRINT_CHAT and so on).
 * text:  the text.
 *
 * Returns nothing. Does nothing unless the connection is trusted.
 *
 * Called from bprintf_internal() for broadcast prints, and from
 * cprintf_internal() for prints to the server console (both g_cmd.c).
 * Only a dedicated server's console prints arrive there, so this only
 * happens on dedicated servers.
 */
void cloudPrint(uint8_t level, char *text) {
    if (cloud.state < CLOUD_STATE_TRUSTED) {
        return;
    }
    if ((cloud.flags & CFL_CHAT) == 0) {
        return;
    }
    cloudWriteByte(CMD_PRINT);
    cloudWriteByte(level);
    cloudWriteString("%s", text);
}

/**
 * Passes a player's teleport request to the backend (a CMD_COMMAND with
 * CMD_COMMAND_TELEPORT). The backend knows the other servers, so it
 * answers with either the list of servers or the address to go to. Only
 * sent when the teleport flag (CFL_TELEPORT) is set in cloud_flags.
 *
 * client_id: slot number of the player asking.
 * location:  the destination they gave, or empty to ask for the list.
 *
 * Returns nothing. Does nothing unless the connection is trusted.
 *
 * Called from Cmd_Teleport_f() once it has checked the request.
 */
void cloudTeleport(uint8_t client_id, char *location) {
    if (cloud.state < CLOUD_STATE_TRUSTED) {
        return;
    }
    if ((cloud.flags & CFL_TELEPORT) == 0) {
        return;
    }
    cloudWriteByte(CMD_COMMAND);
    cloudWriteByte(CMD_COMMAND_TELEPORT);
    cloudWriteByte(client_id);
    cloudWriteString("%s", location);
}

/**
 * Tells the backend a player's userinfo has changed (CMD_PLAYERUPDATE),
 * for example their name, skin or rate, sending their slot number, raw
 * userinfo and client version string.
 *
 * cl: slot number of the player.
 * ui: their new userinfo. Not used; proxyinfo[cl].userinfo.raw, which the
 *     caller has just updated, is sent instead.
 *
 * Returns nothing. Does nothing unless the connection is trusted.
 *
 * Called from ClientUserinfoChanged() (g_init.c).
 */
void cloudPlayerUpdate(uint8_t cl, const char *ui) {
    if (cloud.state < CLOUD_STATE_TRUSTED) {
        return;
    }
    cloudWriteByte(CMD_PLAYERUPDATE);
    cloudWriteByte(cl);
    cloudWriteString("%s", proxyinfo[cl].userinfo.raw);
    cloudWriteString("%s", proxyinfo[cl].client_version);
}

/**
 * Passes a player's invite to the backend (a CMD_COMMAND with
 * CMD_COMMAND_INVITE), which advertises it to players on other servers to
 * get them to join this one. Only sent when the invite flag (CFL_INVITE) is
 * set in cloud_flags.
 *
 * cl:   slot number of the player sending the invite.
 * text: an optional message to go with it.
 *
 * Returns nothing. Does nothing unless the connection is trusted.
 *
 * Called from Cmd_Invite_f().
 */
void cloudInvite(uint8_t cl, const char *text) {
    if (cloud.state < CLOUD_STATE_TRUSTED) {
        return;
    }
    if ((cloud.flags & CFL_INVITE) == 0) {
        return;
    }
    cloudWriteByte(CMD_COMMAND);
    cloudWriteByte(CMD_COMMAND_INVITE);
    cloudWriteByte(cl);
    cloudWriteString("%s", text);
}

/**
 * Passes a player's whois lookup to the backend (a CMD_COMMAND with
 * CMD_COMMAND_WHOIS), which can look a name up across every server it
 * knows. Only sent when the whois flag (CFL_WHOIS) is set in cloud_flags.
 *
 * cl:   slot number of the player asking.
 * name: the name to look up.
 *
 * Returns nothing. Does nothing unless the connection is trusted.
 *
 * Nothing calls this at the moment; "whois" is answered locally by
 * g_whois.c.
 */
void cloudWhois(uint8_t cl, const char *name) {
    if (cloud.state < CLOUD_STATE_TRUSTED) {
        return;
    }
    if ((cloud.flags & CFL_WHOIS) == 0) {
        return;
    }
    cloudWriteByte(CMD_COMMAND);
    cloudWriteByte(CMD_COMMAND_WHOIS);
    cloudWriteByte(cl);
    cloudWriteString("%s", name);
}

/**
 * Tells the backend one player killed another (CMD_FRAG), for stats. Only
 * sent when the frags flag (CFL_FRAGS) is set in cloud_flags.
 *
 * victim:   slot number of the player killed.
 * attacker: slot number of the killer.
 *
 * Returns nothing. Does nothing unless the connection is trusted.
 *
 * Nothing calls this at the moment. Frags only reach the backend as text,
 * through cloudPrint(); the note in bprintf_internal() (g_cmd.c) explains
 * why working out who killed whom is hard from there.
 */
void cloudFrag(uint8_t victim, uint8_t attacker) {
    if (cloud.state < CLOUD_STATE_TRUSTED) {
        return;
    }
    if ((cloud.flags & CFL_FRAGS) == 0) {
        return;
    }
    cloudWriteByte(CMD_FRAG);
    cloudWriteByte(victim);
    cloudWriteByte(attacker);
}

/**
 * Tells the backend which map the server is now on (CMD_MAP).
 *
 * mapname: the map's name.
 *
 * Returns nothing. Does nothing unless the connection is trusted.
 *
 * Called from SpawnEntities() (g_init.c) on each map change, and from
 * cloudParseMessage() right after the connection becomes trusted.
 */
void cloudMap(const char *mapname) {
    if (cloud.state < CLOUD_STATE_TRUSTED) {
        return;
    }
    cloudWriteByte(CMD_MAP);
    cloudWriteString("%s", mapname);
}


/**
 * Prints a message from the backend to one player (SCMD_SAYCLIENT), for
 * example the answer to their teleport request. The message holds the
 * player's slot number, the print level and the text. An out-of-range slot
 * number or an empty slot is ignored.
 *
 * Takes no parameters and returns nothing; reads the message from the
 * incoming queue.
 *
 * Called from cloudParseMessage() on SCMD_SAYCLIENT.
 */
void cloudSayClient(void) {
    uint8_t client_id;
    uint8_t level;
    char *string;
    edict_t *ent;

    if (cloud.state < CLOUD_STATE_TRUSTED) {
        return;
    }
    client_id = cloudReadByte();
    level = cloudReadByte();
    string = cloudReadString();
    if (cloud.queue_in.overrun) {
        return;
    }
    if (client_id >= cloud.maxclients) {
        return;
    }
    ent = proxyinfo[client_id].ent;
    if (!ent) {
        return;
    }
    gi.cprintf(ent, level, "%s", string);
}

/**
 * Prints a message from the backend to every player (SCMD_SAYALL). The
 * message holds the print level and the text. Each player is printed to
 * separately rather than with a broadcast. A broadcast also goes to the
 * server console, which cloudPrint() forwards to the backend, so the
 * message would be sent straight back.
 *
 * Takes no parameters and returns nothing; reads the message from the
 * incoming queue.
 *
 * Called from cloudParseMessage() on SCMD_SAYALL.
 */
void cloudSayAll(void) {
    uint8_t i, level;
    char *string;

    if (cloud.state < CLOUD_STATE_TRUSTED) {
        return;
    }
    level = cloudReadByte();
    string = cloudReadString();
    if (cloud.queue_in.overrun) {
        return;
    }
    for (i=0; i<cloud.maxclients; i++) {
        if (!proxyinfo[i].inuse) {
            continue;
        }

        // Print to each client directly rather than with gi.bprintf(),
        // which would also print to the server console and so send the
        // message straight back to the backend through cloudPrint().
        gi.cprintf(
                proxyinfo[i].ent,
                level,
                "%s\n",
                string
        );
    }
}

/**
 * Formats a number of seconds as "hh:mm:ss", with "1 day, " or "N days, "
 * in front once it reaches a day, for "sv !cloud status".
 *
 * out:  where to write the text. Not bounds checked; it needs room for
 *       the day count plus 20 characters, and callers use 25 bytes.
 * secs: the number of seconds.
 *
 * Returns nothing.
 *
 * Called from cloudRun().
 */
static void secsToTime(char *out, uint32_t secs) {
    uint32_t    days = 0,
                hours = 0,
                minutes = 0,
                seconds = 0;
    uint32_t f = secs;

    days = f / 86400;
    f -= days * 86400;

    hours = f / 3600;
    f -= hours * 3600;

    minutes = f / 60;
    f -= minutes * 60;

    seconds = f;

    if (days == 0) {
        sprintf(out, "%02d:%02d:%02d", hours, minutes, seconds);
    } else if (days == 1) {
        sprintf(out, "1 day, %02d:%02d:%02d", hours, minutes, seconds);
    } else {
        sprintf(out, "%d days, %02d:%02d:%02d", days, hours, minutes, seconds);
    }
}

/**
 * Gets the address and ports of the backend connection for display, the
 * address in brackets ("[192.0.2.1]" or "[2001:db8::1]").
 *
 * remoteip:   where to write the address.
 * size:       size of remoteip. INET6_ADDRSTRLEN + 2 always fits.
 * remoteport: set to the backend's port.
 * localport:  set to this end's port, or 0 if it can't be found.
 *
 * Returns nothing.
 *
 * Called from cloudRun() for "sv !cloud status".
 */
static void cloudGetIP(char *remoteip, size_t size, int *remoteport, int *localport) {
    char addr[INET6_ADDRSTRLEN];
    struct sockaddr_storage local;
    socklen_t len = sizeof(local);

    // the lookup thread hasn't finished, or the address was freed
    if (!cloud.addr) {
        Q_snprintf(remoteip, size, "[unknown]");
        *remoteport = cloud_config.port;
        *localport = 0;
        return;
    }

    if (cloud.addr->ai_family == AF_INET6) {
        q2a_inet_ntop(
            cloud.addr->ai_family,
            &((struct sockaddr_in6 *) cloud.addr->ai_addr)->sin6_addr,
            addr,
            sizeof(addr)
        );
    } else {  // IPv4
        q2a_inet_ntop(
            cloud.addr->ai_family,
            &((struct sockaddr_in *) cloud.addr->ai_addr)->sin_addr,
            addr,
            sizeof(addr)
        );
    }

    Q_snprintf(remoteip, size, "[%s]", addr);

    // The port is at the same place in both address families. It's stored
    // in network byte order, so it has to be converted to be readable.
    *remoteport = ntohs(((struct sockaddr_in *) cloud.addr->ai_addr)->sin_port);

    *localport = 0;
    if (getsockname(cloud.connection.socket, (struct sockaddr *) &local, &len) == 0) {
        *localport = ntohs(((struct sockaddr_in *) &local)->sin_port);
    }
}

/**
 * Runs the "sv !cloud <command>" console command:
 *
 * status:     shows the backend's address, this server's UUID, the
 *             connection state and, once trusted, whether traffic is
 *             encrypted and how long it has been connected.
 * connect:    starts Cloud Admin, as at server start.
 * disconnect: closes the connection and resets the connection state,
 *             keeping the map name, slot count and port.
 * reconnect:  disconnect, then connect.
 *
 * startarg: index of the command word among the arguments.
 * ent:      who to print to, NULL for the server console.
 * client:   not used.
 *
 * Returns nothing. Prints the usage if no command is given; an unknown one
 * does nothing.
 *
 * Called through the "cloud" entry in q2aCommands[] (g_cmd.c).
 */
void cloudRun(int startarg, edict_t *ent, int client) {
    char *command;
    bool connected;
    char connected_time[25];
    char connected_ip[INET6_ADDRSTRLEN + 2];   // room for the brackets
    int local_port;
    int remote_port;

    if (gi.argc() <= startarg) {
        gi.cprintf(ent, PRINT_HIGH, "Usage: %s\n", CLOUDCMD_LAYOUT);
        return;
    }

    connected = cloud.state == CLOUD_STATE_TRUSTED;
    command = gi.argv(startarg);

    if (Q_stricmp(command, "status") == 0) {
        gi.cprintf(ent, PRINT_HIGH, "[cloud admin status]\n");
        if (connected) {
            cloudGetIP(connected_ip, sizeof(connected_ip), &remote_port, &local_port);
            gi.cprintf(ent, PRINT_HIGH, "%-20s%s\n", "connected to:", va("%s:%d", connected_ip, remote_port));
            gi.cprintf(ent, PRINT_HIGH, "%-20s%d\n", "local port:", local_port);
        } else {
            gi.cprintf(ent, PRINT_HIGH, "%-20s%s\n", "host:", va("%s:%d", cloud_config.address, cloud_config.port));
        }
        gi.cprintf(ent, PRINT_HIGH, "%-20s%s\n", "client uuid:", cloud_config.uuid);
        if (cloud.state == CLOUD_STATE_DISABLED) {
            gi.cprintf(ent, PRINT_HIGH, "%-20s%s\n", "state:", "disabled");
            return;
        }

        gi.cprintf(ent, PRINT_HIGH, "%-20s%s\n", "state:", (connected)? "trusted" : "disconnected");
        gi.cprintf(ent, PRINT_HIGH, "%-20s%d\n", "disconnects:", cloud.disconnect_count);
        if (connected) {
            secsToTime(connected_time, FRAMES_TO_SECS(cloud.frame_number - cloud.connected_frame));
            gi.cprintf(ent, PRINT_HIGH, "%-20s%s\n", "transit:", (cloud.connection.encrypted) ? "encrypted" : "clear text");
            gi.cprintf(ent, PRINT_HIGH, "%-20s%s\n", "connected time:", connected_time);
        }
        return;
    }

    // cloudResetState() rather than wiping cloud outright, so the map name,
    // slot count and port set by SpawnEntities() survive for the next hello.
    if (Q_stricmp(command, "reconnect") == 0) {
        cloudDisconnect();
        cloudResetState();
        cloudPrintf("disconnected\n");
        cloudInit();
        return;
    }

    if (Q_stricmp(command, "disconnect") == 0) {
        cloudDisconnect();
        cloudResetState();
        cloudPrintf("disconnected\n");
        return;
    }

    if (Q_stricmp(command, "connect") == 0) {
        cloudInit();
        return;
    }
}

/**
 * Prints a printf-style message to the server console with "[cloud] " in
 * front, so Cloud Admin messages are easy to pick out.
 *
 * fmt: printf-style format.
 * ...: values for fmt.
 *
 * Returns nothing.
 *
 * Called throughout this file for messages an admin should always see.
 */
void cloudPrintf(char *fmt, ...) {
    char cbuffer[8192];
    va_list arglist;

    // convert to string
    va_start(arglist, fmt);
    Q_vsnprintf(cbuffer, sizeof(cbuffer), fmt, arglist);
    va_end(arglist);

    gi.cprintf(NULL, PRINT_HIGH, "[cloud] %s", cbuffer);
}

/**
 * The debug version of cloudPrintf(): prints a "[cloud] " message to the
 * server console only when the debug flag (CFL_DEBUG) is set in
 * cloud_flags. Used for detail that helps when setting up or
 * troubleshooting the connection but would clutter the console otherwise.
 *
 * fmt: printf-style format.
 * ...: values for fmt.
 *
 * Returns nothing.
 *
 * Called throughout this file.
 */
void cloudDPrintf(char *fmt, ...) {
    char cbuffer[8192];
    va_list arglist;

    if (!CFL(DEBUG)) {
        return;
    }
    // convert to string
    va_start(arglist, fmt);
    Q_vsnprintf(cbuffer, sizeof(cbuffer), fmt, arglist);
    va_end(arglist);

    gi.cprintf(NULL, PRINT_HIGH, "[cloud] %s", cbuffer);
}

/**
 * Handles a player's teleport command (named by cloud_cmd_teleport). The
 * request goes to the backend, which answers with either the list of
 * servers or the address of the one asked for. The player is told instead
 * if the backend isn't connected, teleport isn't enabled in cloud_flags,
 * or the destination is longer than TELE_NAME_MAX characters.
 *
 * ent: the player who used the command.
 *
 * Returns nothing.
 *
 * Called from doClientCommand() (g_cmd.c). The command's name is set in
 * the config and needn't start with "!", which is why it isn't in
 * q2aCommands[].
 */
void Cmd_Teleport_f(edict_t *ent) {
    if (!CLOUD_OK) {
        gi.cprintf(ent, PRINT_HIGH, "Not currently connected to a CloudAdmin server.\n");
        return;
    }
    if (!(cloud.flags & CFL_TELEPORT)) {
        gi.cprintf(ent, PRINT_HIGH, "Teleport command is not enabled in server config.\n");
        return;
    }
    if (q2a_strlen(gi.argv(1)) > TELE_NAME_MAX) {
        gi.cprintf(ent, PRINT_HIGH, "Invalid teleport destination.\n");
        return;
    }
    cloudTeleport(getEntOffset(ent) - 1, gi.argv(1));
}

/**
 * Handles a player's invite command (named by cloud_cmd_invite). The
 * invite, with any text the player added, goes to the backend to be shown
 * to players on other servers. The player is told instead if the backend
 * isn't connected or invite isn't enabled in cloud_flags.
 *
 * ent: the player who used the command.
 *
 * Returns nothing.
 *
 * Called from doClientCommand() (g_cmd.c), for the same reason as
 * Cmd_Teleport_f().
 */
void Cmd_Invite_f(edict_t *ent) {
    if (!CLOUD_OK) {
        gi.cprintf(ent, PRINT_HIGH, "Not currently connected to a CloudAdmin server.\n");
        return;
    }
    if (!(cloud.flags & CFL_INVITE)) {
        gi.cprintf(ent, PRINT_HIGH, "Invite command is not enabled in server config.\n");
        return;
    }
    char *invitetext;
    uint8_t id = getEntOffset(ent) - 1;
    if (gi.argc() > 1) {
        invitetext = gi.args();
    } else {
        invitetext = "";
    }
    cloudInvite(id, invitetext);
}
