#include "g_local.h"

remote_t remote;


/**
 * Sets up RA connection. Makes sure we have what we need:
 * - enabled in config
 * - rcon password is set
 *
 */
void RA_Init() {
    
    // we're already connected
    if (remote.connection.socket) {
        return;
    }

    memset(&remote, 0, sizeof(remote));
    maxclients = gi.cvar("maxclients", "64", CVAR_LATCH);
    
    if (!remoteEnabled) {
        gi.cprintf(NULL, PRINT_HIGH, "Remote Admin is disabled in your config.\n");
        return;
    }
    remote.state = RA_STATE_DISCONNECTED;
    gi.cprintf(NULL, PRINT_HIGH, "[RA] Remote Admin Init...\n");

    if (!G_LoadKeys()) {
        remote.state = RA_STATE_DISABLED;
        return;
    }

    remote.connection.encrypted = remoteEncryption;
    
    if (!remoteAddr[0]) {
        gi.cprintf(NULL, PRINT_HIGH, "[RA] remote_addr is not set...disabling\n");
        remote.state = RA_STATE_DISABLED;
        return;
    }

    if (!remotePort) {
        gi.cprintf(NULL, PRINT_HIGH, "[RA] remote_port is not set...disabling\n");
        remote.state = RA_STATE_DISABLED;
        return;
    }

    G_StartThread(&RA_LookupAddress, NULL);

    // delay connection by a few seconds
    remote.connect_retry_frame = FUTURE_FRAME(5);
}


/**
 * Build a new info string containing just want we need:
 *  name, ip, skin, fov
 */
static char *ra_userinfo(uint8_t player_index)
{
    static char newuserinfo[MAX_INFO_STRING];
    char *value;

    memset(newuserinfo, 0, MAX_INFO_STRING);
    value = Info_ValueForKey(proxyinfo[player_index].userinfo, "name");
    if (value) {
        q2a_strcpy(newuserinfo, va("\\name\\%s", value));
    }

    value = Info_ValueForKey(proxyinfo[player_index].userinfo, "ip");
    if (value) {
        q2a_strcat(newuserinfo, va("\\ip\\%s", value));
    }

    value = Info_ValueForKey(proxyinfo[player_index].userinfo, "skin");
    if (value) {
        q2a_strcat(newuserinfo, va("\\skin\\%s", value));
    }

    value = Info_ValueForKey(proxyinfo[player_index].userinfo, "fov");
    if (value) {
        q2a_strcat(newuserinfo, va("\\fov\\%s", value));
    }

    return newuserinfo;
}


/**
 * getaddrinfo result is a linked-list of struct addrinfo.
 * Figure out which result is the one we want (ipv6/ipv4)
 */
static struct addrinfo *select_addrinfo(struct addrinfo *a)
{
    static struct addrinfo *v4, *v6;

    if (!a) {
        return NULL;
    }

    // just in case it's set blank in the config
    if (!remoteDNS[0]) {
        q2a_strcpy(remoteDNS, "64");
    }

    // save the first one of each address family, if more than 1, they're
    // almost certainly in a random order anyway
    for (;a != NULL; a = a->ai_next) {
        if (!v4 && a->ai_family == AF_INET) {
            v4 = a;
        }

        if (!v6 && a->ai_family == AF_INET6) {
            v6 = a;
        }
    }

    // select one based on preference
    if (remoteDNS[0] == '6' && v6) {
        return v6;
    } else if (remoteDNS[0] == '4' && v4) {
        return v4;
    } else if (remoteDNS[1] == '6' && v6) {
        return v6;
    } else if (remoteDNS[1] == '4' && v4) {
        return v4;
    }

    return NULL;
}


/**
 * Do a DNS lookup for remote server.
 * This is done in a dedicated thread to prevent blocking
 */
void RA_LookupAddress(void)
{
    char str_address[40];
    struct addrinfo hints, *res = 0;

    memset(&hints, 0, sizeof(hints));
    memset(&res, 0, sizeof(res));

    hints.ai_family         = AF_UNSPEC;     // either v6 or v4
    hints.ai_socktype       = SOCK_STREAM;     // TCP
    hints.ai_protocol       = 0;
    hints.ai_flags          = AI_ADDRCONFIG; // only return v6 addresses if interface is v6 capable

    // set to catch any specific errors
    errno = 0;

    // do the actual DNS lookup
    int err = getaddrinfo(remoteAddr, va("%d",remotePort), &hints, &res);

    if (err != 0) {
        gi.cprintf(NULL, PRINT_HIGH, "[RA] DNS error\n");
        remote.state = RA_STATE_DISABLED;
        return;
    } else {
        memset(&remote.addr, 0, sizeof(struct addrinfo));

        // getaddrinfo can return multiple mixed v4/v6 results, select an appropriate one
        remote.addr = select_addrinfo(res);

        if (!remote.addr) {
            remote.state = RA_STATE_DISABLED;
            gi.cprintf(NULL, PRINT_HIGH, "[RA] Problems resolving server address, disabling\n");
            return;
        }

        // for convenience
        if (res->ai_family == AF_INET6) {
            remote.connection.ipv6 = qtrue;
        }

        if (remote.addr->ai_family == AF_INET6) {
            q2a_inet_ntop(
                    remote.addr->ai_family,
                    &((struct sockaddr_in6 *) remote.addr->ai_addr)->sin6_addr,
                    str_address,
                    sizeof(str_address)
            );
        } else {
            q2a_inet_ntop(
                    remote.addr->ai_family,
                    &((struct sockaddr_in *) remote.addr->ai_addr)->sin_addr,
                    str_address,
                    sizeof(str_address)
            );
        }

        gi.cprintf(NULL, PRINT_HIGH, "[RA] Server resolved to %s\n", str_address);
    }

    remote.flags = remoteFlags;
    remote.state = RA_STATE_DISCONNECTED;
}

void G_StartThread(void *func, void *arg) {
#if _WIN32
    DWORD tid;
    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) func, 0, 0, &tid);
#else
    pthread_t dnsthread;
    pthread_create(&dnsthread, 0, func, 0);
#endif
}

/**
 * Replace the die() function pointer for each player edict.
 * For capturing frag events
 */
static void ra_replace_die(void)
{
    static uint8_t i;

    for (i=0; i<=remote.maxclients; i++) {
        if (proxyinfo[i].inuse) {

            // replace player edict's die() pointer
            if (proxyinfo[i].ent && *proxyinfo[i].ent->die != PlayerDie_Internal) {
                proxyinfo[i].die = *proxyinfo[i].ent->die;
                proxyinfo[i].ent->die = &PlayerDie_Internal;
            }
        }
    }
}


/**
 *
 */
void debug_print(char *str)
{
    if (!RFL(DEBUG)) {
        return;
    }

    gi.cprintf(NULL, PRINT_HIGH, "%s\n", str);
}

/**
 * Periodically ping the server to know if the connection is still open
 */
void RA_Ping(void)
{
    if (remote.state < RA_STATE_CONNECTED) {
        return;
    }

    // not time yet
    if (remote.ping.frame_next > CURFRAME) {
        return;
    }

    // there is already an outstanding ping
    if (remote.ping.waiting) {
        if (remote.ping.miss_count == PING_MISS_MAX) {
            RA_DisconnectedPeer();
            return;
        }
        remote.ping.miss_count++;
    }

    // state stuff
    remote.ping.frame_sent = CURFRAME;
    remote.ping.waiting = qtrue;
    remote.ping.frame_next = CURFRAME + SECS_TO_FRAMES(PING_FREQ_SECS);

    // send it
    RA_WriteByte(CMD_PING);
}

/**
 * Run once per server frame
 *
 */
void RA_RunFrame(void)
{
    // keep some time
    remote.frame_number++;

    // remote admin is disabled, don't do anything
    if (remote.state == RA_STATE_DISABLED) {
        return;
    }

    // everything we need to do while RA is connected
    if (remote.state >= RA_STATE_CONNECTED) {

        // send any buffered messages to the server
        RA_SendMessages();

        // receive any pending messages from server
        RA_ReadMessages();

        // update player die() pointers
        ra_replace_die();

        // periodically make sure connection is alive
        RA_Ping();
    }

    // connection started already, check for completion
    if (remote.state == RA_STATE_CONNECTING) {
        RA_CheckConnection();
    }

    // we're not connected, try again
    if (remote.state == RA_STATE_DISCONNECTED) {
        RA_Connect();
    }
}

void RA_Shutdown(void)
{
    if (remote.state == RA_STATE_DISABLED) {
        return;
    }

    /**
     * We have to call RA_SendMessages() specifically here because there won't be another
     * frame to send the buffered CMD_QUIT
     */
    RA_WriteByte(CMD_QUIT);
    RA_SendMessages();
    RA_Disconnect();

    freeaddrinfo(remote.addr);
}

/**
 *
 */
void RA_Disconnect(void)
{
    if (remote.state < RA_STATE_CONNECTED) {
        return;
    }

    closesocket(remote.connection.socket);
    remote.state = RA_STATE_DISCONNECTED;
}

/**
 * Get the frame number at which we should try another connection.
 * Increase the interval as attempts increase without a connection.
 */
static uint32_t next_connect_frame(void)
{
    if (remote.connection_attempts < 12) {  // 2 minutes
        return FUTURE_FRAME(10);
    } else if (remote.connection_attempts < 20) { // 10 minutes
        return FUTURE_FRAME(30);
    } else if (remote.connection_attempts < 50) { // 30 minutes
        return FUTURE_FRAME(60);
    } else {
        return FUTURE_FRAME(120);
    }
}

/**
 * Make the connection to the remote admin server
 */
void RA_Connect(void)
{
    int flags, ret;

    // only if we've waited long enough
    if (remote.frame_number < remote.connect_retry_frame) {
        return;
    }

    // clear the in and out buffers
    q2a_memset(&remote.queue, 0, sizeof(message_queue_t));
    q2a_memset(&remote.queue_in, 0, sizeof(message_queue_t));

    remote.state = RA_STATE_CONNECTING;
    remote.connection_attempts++;

    // create the socket
    remote.connection.socket = socket(
            remote.addr->ai_family,
            remote.addr->ai_socktype,
            remote.addr->ai_protocol
    );

    if (remote.connection.socket == -1) {
        perror("connect");
        errno = 0;
        remote.connect_retry_frame = next_connect_frame();

        return;
    }

// Make it non-blocking.
// preferred method is using POSIX O_NONBLOCK, if available
#if defined(O_NONBLOCK)
    flags = 0;
    ret = fcntl(remote.connection.socket, F_SETFL, flags | O_NONBLOCK);
#else
    flags = 1;
    ret = ioctlsocket(remote.connection.socket, FIONBIO, (long unsigned int *) &flags);
#endif

    if (ret == -1) {
        gi.cprintf(NULL, PRINT_HIGH, "[RA] Error setting socket to non-blocking: (%d) %s\n", errno, strerror(errno));
        remote.state = RA_STATE_DISCONNECTED;
        remote.connect_retry_frame = FUTURE_FRAME(30);
    }

    errno = 0;

    // make the actual connection
    ret = connect(remote.connection.socket, remote.addr->ai_addr, remote.addr->ai_addrlen);

    if (ret == -1) {
        if (errno == EINPROGRESS) {
            // expected
        } else {
            perror("[RA] connect error");
            errno = 0;
        }
    }

    /**
     * since we're non-blocking, the connection won't complete in this single server frame.
     * We have to select() for it on a later runframe. See RA_CheckConnection()
     */
}

/**
 * Check to see if the connection initiated by RA_Connect() has finished
 */
void RA_CheckConnection(void)
{
    ra_connection_t *c;
    uint32_t ret;
    qboolean connected = qfalse;
    qboolean exception = qfalse;
    struct sockaddr_storage addr;
    socklen_t len;
    struct timeval tv;
    tv.tv_sec = tv.tv_usec = 0;

    c = &remote.connection;

    FD_ZERO(&c->set_w);
    FD_ZERO(&c->set_e);
    FD_SET(c->socket, &c->set_w);
    FD_SET(c->socket, &c->set_e);

    // check if connection is fully established
    ret = select((int)c->socket + 1, NULL, &c->set_w, &c->set_e, &tv);

    if (ret) {

#ifdef LINUX
        uint32_t number;
        socklen_t len;

        len = sizeof(number);
        getsockopt(c->socket, SOL_SOCKET, SO_ERROR, &number, &len);
        if (number == 0) {
            connected = qtrue;
        } else {
            exception = qtrue;
        }
#else
        if (FD_ISSET(c->socket, &c->set_w)) {
            connected = qtrue;
        }
#endif
    }

    if (ret == -1) {
        perror("CheckConnection");
        gi.cprintf(NULL, PRINT_HIGH, "[RA] Connection unfinished: %s\n", strerror(errno));
        closesocket(c->socket);
        remote.state = RA_STATE_DISCONNECTED;
        remote.connect_retry_frame = FUTURE_FRAME(10);
        return;
    }

    // we need to make sure it's actually connected
    if (connected) {
        errno = 0;
        getpeername(c->socket, (struct sockaddr *)&addr, &len);

        if (errno) {
            remote.connect_retry_frame = FUTURE_FRAME(30);
            remote.state = RA_STATE_DISCONNECTED;
            closesocket(c->socket);
        } else {
            gi.cprintf(NULL, PRINT_HIGH, "[RA] Connected\n");
            remote.state = RA_STATE_CONNECTED;
            remote.ping.frame_next = FUTURE_FRAME(10);
            remote.connected_frame = CURFRAME;
            RA_SayHello();
        }
    }
}

/**
 * Send the contents of our outgoing buffer to the server
 */
void RA_SendMessages(void)
{
    if (remote.state < RA_STATE_CONNECTING) {
        return;
    }

    // nothing to send
    if (!remote.queue.length) {
        return;
    }

    uint32_t ret;
    struct timeval tv;
    tv.tv_sec = tv.tv_usec = 0;
    ra_connection_t *c;
    message_queue_t *q;
    message_queue_t e;

    c = &remote.connection;
    q = &remote.queue;

    while (qtrue) {
        FD_ZERO(&c->set_w);
        FD_SET(c->socket, &c->set_w);

        // see if the socket is ready to send data
        ret = select((int) c->socket + 1, NULL, &c->set_w, NULL, &tv);
        if (ret == -1) {
            //if (errno != EINTR) {
                perror("send select");
            //}
            errno = 0;
        }

        // socket write buffer is ready, send
        if (ret) {
            if (c->encrypted && c->have_keys) {
                memset(&e, 0, sizeof(message_queue_t));
                e.length = G_SymmetricEncrypt(e.data, q->data, q->length);
                memset(q, 0, sizeof(message_queue_t));
                memcpy(q->data, e.data, e.length);
                q->length = e.length;
            }

            //hexDump("Sending", q->data, q->length);

            ret = send(c->socket, q->data, q->length, 0);
            if (ret == -1) {
                if (errno == EPIPE) {
                    gi.cprintf(NULL, PRINT_HIGH, "Remote side disconnected\n");
                    RA_DisconnectedPeer();
                    errno = 0;
                    break;
                }

                perror("send error");
                errno = 0;
            } else {

                // shift off the data we just sent
                memmove(q->data, q->data + ret, q->length - ret);
                q->length -= ret;
            }

        } else {
            break;
        }

        // processed the whole queue, we're done for now
        if (!q->length) {
            break;
        }
    }
}


/**
 * Accept any incoming messages from the server
 */
void RA_ReadMessages(void)
{
    uint32_t ret;
    size_t expectedLength = 0;
    struct timeval tv;
    message_queue_t *in;
    message_queue_t dec;

    if (remote.state < RA_STATE_CONNECTING) {
        return;
    }

    tv.tv_sec = tv.tv_usec = 0;

    // save some typing
    in = &remote.queue_in;

    while (qtrue) {
        FD_ZERO(&remote.connection.set_r);
        FD_SET(remote.connection.socket, &remote.connection.set_r);

        // see if there is data waiting in the buffer for us to read
        ret = select(remote.connection.socket + 1, &remote.connection.set_r, NULL, NULL, &tv);
        if (ret == -1) {
            if (errno != EINTR) {
                perror("select");
                RA_DisconnectedPeer();
                errno = 0;
                return;
            }

            errno = 0;
        }

        // socket read buffer has data waiting in it
        if (ret) {
            ret = recv(remote.connection.socket, in->data + in->length, QUEUE_SIZE - 1, 0);

            if (ret == 0) {
                RA_DisconnectedPeer();
                return;
            }

            if (ret == -1) {
                if (errno != EINTR) {
                    perror("recv");
                    RA_DisconnectedPeer();
                    return;
                }

                errno = 0;
            }

            in->length += ret;

            // decrypt if necessary
            if (remote.connection.encrypted && remote.connection.have_keys) {
                memset(&dec, 0, sizeof(message_queue_t));
                dec.length = G_SymmetricDecrypt(dec.data, in->data, in->length);
                memset(in->data, 0, in->length);
                memcpy(in->data, dec.data, dec.length);
                in->length = dec.length;
            }
        } else {
            // no data has been sent to read
            break;
        }
    }

    // if we made it here, we have the whole message, parse it
    RA_ParseMessage();
}

/**
 * The server has let us know we are trusted.
 */
void RA_Trusted(void)
{
    gi.cprintf(NULL, PRINT_HIGH, "[RA] Connection Trusted\n");
    remote.state = RA_STATE_TRUSTED;
}

/**
 * Parse a newly received message and act accordingly
 */
void RA_ParseMessage(void)
{
    message_queue_t *msg = &remote.queue_in;
    byte cmd;

    if (remote.state == RA_STATE_DISABLED) {
        return;
    }

    // no data
    if (!remote.queue_in.length) {
        return;
    }

    //hexDump("New Message", remote.queue_in.data, remote.queue_in.length);

    while (msg->index < msg->length) {
        cmd = RA_ReadByte();

        switch (cmd) {
        case SCMD_PONG:
            RA_ParsePong();
            break;
        case SCMD_COMMAND:
            RA_ParseCommand();
            break;
        case SCMD_HELLOACK:
            RA_VerifyServerAuth();
            break;
        case SCMD_TRUSTED:  // we just connected and authed successfully, tell server about us
            RA_Trusted();
            RA_Map(remote.mapname);
            RA_PlayerList();
            break;
        case SCMD_ERROR:
            RA_ParseError();
            break;
        case SCMD_SAYCLIENT:
            RA_SayClient();
            break;
        case SCMD_SAYALL:
            RA_SayAll();
            break;
        case SCMD_KEY:
            RA_RotateKeys();
            break;
        }
    }

    // reset queue back to zero
    q2a_memset(&remote.queue_in, 0, sizeof(message_queue_t));
}

qboolean RSAVerifySignature( RSA* rsa,
                         unsigned char* MsgHash,
                         size_t MsgHashLen,
                         const char* Msg,
                         size_t MsgLen,
                         qboolean* Authentic) {
    *Authentic = qfalse;
    EVP_PKEY* pubKey  = EVP_PKEY_new();
    EVP_PKEY_assign_RSA(pubKey, rsa);
    EVP_MD_CTX* m_RSAVerifyCtx = EVP_MD_CTX_new();

    if (EVP_DigestVerifyInit(m_RSAVerifyCtx,NULL, EVP_sha256(),NULL,pubKey)<=0) {
        return qfalse;
    }

    if (EVP_DigestVerifyUpdate(m_RSAVerifyCtx, Msg, MsgLen) <= 0) {
      return qfalse;
    }

    int AuthStatus = EVP_DigestVerifyFinal(m_RSAVerifyCtx, MsgHash, MsgHashLen);
    if (AuthStatus == 1) {
        *Authentic = qtrue;
        EVP_MD_CTX_free(m_RSAVerifyCtx);
        return qtrue;
    } else if (AuthStatus == 0) {
        *Authentic = qfalse;
        EVP_MD_CTX_free(m_RSAVerifyCtx);
        return qtrue;
    } else {
        *Authentic = qfalse;
        EVP_MD_CTX_free(m_RSAVerifyCtx);
        return qfalse;
    }
}

/**
 * 1. Decrypt the nonce sent back from the server and check if it matches
 * 2. If the encryption is requested, parse the AES 128 key and IV
 * 3. Read the plaintext nonce from the server, encrypt and send back
 *    to auth the client
 */
qboolean RA_VerifyServerAuth(void)
{
    ra_connection_t *c;
    uint16_t len;
    byte digest[DIGEST_LEN];
    byte aeskey_cipher[RSA_LEN];
    byte key_plus_iv[AESKEY_LEN + AESBLOCK_LEN];
    qboolean servertrusted = qfalse;
    int verified;
    byte signature[RSA_LEN];
    unsigned int siglen;
    int chalsigned;

    c = &remote.connection;

    q2a_memset(signature, 0, RSA_LEN);
    len = RA_ReadShort();
    RA_ReadData(signature, len);

    if (remoteEncryption) {
        RA_ReadData(aeskey_cipher, RSA_LEN);
    }

    RA_ReadData(c->sv_nonce, CHALLENGE_LEN);
    q2a_memset(digest, 0, DIGEST_LEN);
    G_SHA256Hash(digest, c->cl_nonce, CHALLENGE_LEN);
    verified = RSA_verify(NID_sha256, digest, DIGEST_LEN, signature, len, c->rsa_sv_pu);

    if (verified) {
        servertrusted = qtrue;
        printf("[RA] server signature verified\n");
    } else {
        printf("[RA] Error: %s\n", ERR_error_string(ERR_get_error(), NULL));
    }

    // encrypt the server's nonce and send back to auth ourselves
    if (servertrusted) {
        q2a_memset(digest, 0, DIGEST_LEN);
        q2a_memset(signature, 0, RSA_LEN);
        G_SHA256Hash(digest, c->sv_nonce, CHALLENGE_LEN);
        //hexDump("Digest", digest, DIGEST_LEN);

        chalsigned = RSA_sign(NID_sha256, digest, DIGEST_LEN, signature, &siglen, c->rsa_pr);
        if (!chalsigned) {

        }

        //hexDump("Signature", sig, RSA_LEN);

        RA_WriteByte(CMD_AUTH);
        RA_WriteShort(siglen);
        RA_WriteData(signature, siglen);
        RA_SendMessages();

        if (remoteEncryption) {
            len = G_PrivateDecrypt(key_plus_iv, aeskey_cipher);
            if (!len) {
                gi.cprintf(NULL, PRINT_HIGH, "[RA] couldn't decrypt symmetric keys, connection will NOT be encrypted\n");
                remoteEncryption = qfalse;
                c->have_keys = qfalse;
            } else {
                q2a_memcpy(c->aeskey, key_plus_iv, AESKEY_LEN);
                q2a_memcpy(c->iv, key_plus_iv + AESKEY_LEN, AESBLOCK_LEN);
                c->have_keys = qtrue;
                c->e_ctx = EVP_CIPHER_CTX_new();
                c->d_ctx = EVP_CIPHER_CTX_new();
            }
        }

        remote.connection_attempts = 0;
        remote.connection.auth_fail_count = 0;
        return qtrue;
    } else {
        gi.cprintf(NULL, PRINT_HIGH, "[RA] Server authentication failed\n");
        remote.connection.auth_fail_count++;

        if (remote.connection.auth_fail_count > AUTH_FAIL_LIMIT) {
            gi.cprintf(NULL, PRINT_HIGH, "[RA] Too many auth failures, giving up\n");
            remote.state = RA_STATE_DISABLED;
        }
        return qfalse;
    }
}

/**
 * Server sent over a command.
 */
void RA_ParseCommand(void)
{
    char *cmd;

    // we should never get here if we're not trusted, but just in case
    if (remote.state < RA_STATE_TRUSTED) {
        return;
    }

    cmd = RA_ReadString();

    // cram it into the command buffer
    gi.AddCommandString(cmd);
}

/**
 * Not really much to parse...
 */
void RA_ParsePong(void)
{
    remote.ping.waiting = qfalse;
    remote.ping.miss_count = 0;
}

/**
 * The server sent us new symmetric encryption keys, parse them and
 * start using them
 */
void RA_RotateKeys(void)
{
    ra_connection_t *c;
    c = &remote.connection;

    RA_ReadData(c->aeskey, AESKEY_LEN);
    RA_ReadData(c->iv, AESBLOCK_LEN);

    //debug_print("[RA] Encryption keys changed");
}

/**
 * There was a sudden disconnection mid-stream. Reconnect after an
 * appropriate pause
 */
void RA_DisconnectedPeer(void)
{
    if (remote.state < RA_STATE_CONNECTED) {
        return;
    }

    gi.cprintf(NULL, PRINT_HIGH, "[RA] Connection lost\n");

    remote.state = RA_STATE_DISCONNECTED;
    remote.connection.trusted = qfalse;
    remote.connection.have_keys = qfalse;
    memset(&remote.connection.aeskey[0], 0, AESKEY_LEN);
    memset(&remote.connection.iv[0], 0, AESBLOCK_LEN);

    // try reconnecting a reasonably random amount of time later
    remote.connect_retry_frame = FUTURE_FRAME(10) + ((rand() & 0xf) * 2);
}

/**
 * Send info about all the players connected
 */
void RA_PlayerList(void)
{
    uint8_t count, i;
    count = 0;

    if (remote.state < RA_STATE_TRUSTED) {
        return;
    }

    for (i=0; i<remote.maxclients; i++) {
        if (proxyinfo[i].inuse) {
            count++;
        }
    }

    RA_WriteByte(CMD_PLAYERLIST);
    RA_WriteByte(count);

    for (i=0; i<remote.maxclients; i++) {
        if (proxyinfo[i].inuse) {
            RA_WriteByte(i);
            RA_WriteString("%s", ra_userinfo(i));
        }
    }
}

/**
 * Allows for RA to send frag notifications
 *
 * Self is the fragged player
 * inflictor is what did the damage (rocket, bolt, grenade)
 *   in the case of hitscan weapons, the inflictor will be the attacking player
 * attacker is the player doing the attacking
 * damage is how much dmg was done
 * point is where on the map the damage was dealt
 */
void PlayerDie_Internal(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point) {
    uint8_t id = getEntOffset(self) - 1;
    uint8_t aid = getEntOffset(attacker) - 1;
    gitem_t *weapon;

    proxyinfo[id].die(self, inflictor, attacker, damage, point);
    return;

    /*
    if (self->deadflag != DEAD_DEAD) {
        if (strcmp(attacker->classname,"player") == 0) {
            RA_Frag(id, aid, proxyinfo[id].name, proxyinfo[aid].name);
        } else {
            RA_Frag(id, aid, proxyinfo[id].name, "");
        }
    }

    weapon = (gitem_t *)((attacker->client) + OTDM_CL_WEAPON_OFFSET);

    gi.dprintf("MOD: %s\n", weapon->classname);

    // call the player's real die() function
    proxyinfo[id].die(self, inflictor, attacker, damage, point);
    */
}

/**
 * Immediately after connecting, we have to say hi, giving the server our
 * information. Once the server acknowledges we can start sending game data.
 *
 * The last section of data is a random nonce. The server will encrypt this
 * and send it back to us. We then decrypt and check if it matches, if so,
 * the server is who we think it is and is considered trusted.
 */
void RA_SayHello(void)
{
    // don't bother if we're not fully connected yet
    if (remote.state == RA_STATE_TRUSTED) {
        return;
    }

    // random data to check server auth
    RAND_bytes(remote.connection.cl_nonce, sizeof(remote.connection.cl_nonce));

    RA_WriteLong(MAGIC_CLIENT);
    RA_WriteByte(CMD_HELLO);
    RA_WriteLong(remoteKey);
    RA_WriteLong(Q2A_REVISION);
    RA_WriteShort(remote.port);
    RA_WriteByte(remote.maxclients);
    RA_WriteByte(remoteEncryption ? 1 : 0);
    RA_WriteData(remote.connection.cl_nonce, sizeof(remote.connection.cl_nonce));
}

/**
 * The server replied negatively to something
 */
void RA_ParseError(void)
{
    uint8_t client_id, reason_id;
    char *reason;

    gi.dprintf("parsing error\n");

    client_id = RA_ReadByte(); // will be -1 if not player specific
    reason_id = RA_ReadByte();
    reason = RA_ReadString();

    // Where to output the error msg
    if (client_id == -1) {
        gi.cprintf(NULL, PRINT_HIGH, "%s\n", reason);
    } else {
        //gi.cprintf(proxyinfo[client_id].ent, PRINT_HIGH, "%s\n", reason);
        gi.cprintf(NULL, PRINT_HIGH, "error msg here\n");
    }

    // serious enough to disconnect
    if (reason_id >= 200) {
        closesocket(remote.connection.socket);
        remote.state = RA_STATE_DISABLED;
        freeaddrinfo(remote.addr);
    }
}

/**
 * q2pro uses "net_port" while most other servers use "port". This
 * returns the value regardless.
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
 * Reset the outgoing message buffer to zero to start a new msg
 */
void RA_InitBuffer() {
    q2a_memset(&remote.queue, 0, sizeof(message_queue_t));
}

/**
 * Read a single byte from the message buffer
 */
uint8_t RA_ReadByte(void)
{
    unsigned char b = remote.queue_in.data[remote.queue_in.index++];
    return b & 0xff;
}

/**
 * Write a single byte to the message buffer
 */
void RA_WriteByte(uint8_t b)
{
    remote.queue.data[remote.queue.length++] = b & 0xff;
}

/**
 * Read a short (2 bytes) from the message buffer
 */
uint16_t RA_ReadShort(void)
{
    return    (remote.queue_in.data[remote.queue_in.index++] +
            (remote.queue_in.data[remote.queue_in.index++] << 8)) & 0xffff;
}

/**
 * Write 2 bytes to the message buffer
 */
void RA_WriteShort(uint16_t s)
{
    remote.queue.data[remote.queue.length++] = s & 0xff;
    remote.queue.data[remote.queue.length++] = (s >> 8) & 0xff;
}

/**
 * Read 4 bytes from the message buffer
 */
int32_t RA_ReadLong(void)
{
    return    remote.queue_in.data[remote.queue_in.index++] +
            (remote.queue_in.data[remote.queue_in.index++] << 8) +
            (remote.queue_in.data[remote.queue_in.index++] << 16) +
            (remote.queue_in.data[remote.queue_in.index++] << 24);
}

/**
 * Write 4 bytes (long) to the message buffer
 */
void RA_WriteLong(uint32_t i)
{
    remote.queue.data[remote.queue.length++] = i & 0xff;
    remote.queue.data[remote.queue.length++] = (i >> 8) & 0xff;
    remote.queue.data[remote.queue.length++] = (i >> 16) & 0xff;
    remote.queue.data[remote.queue.length++] = (i >> 24) & 0xff;
}

/**
 * Write an arbitrary amount of data from the message buffer
 */
void RA_WriteData(const void *data, size_t length)
{
    uint32_t i;
    for (i=0; i<length; i++) {
        RA_WriteByte(((byte *) data)[i]);
    }
}

/**
 * Read a null terminated string from the buffer
 */
char *RA_ReadString(void)
{
    static char str[MAX_STRING_CHARS];
    static char character;
    size_t i, len = 0;

    do {
        len++;
    } while (remote.queue_in.data[(remote.queue_in.index + len)] != 0);

    memset(&str, 0, MAX_STRING_CHARS);

    for (i=0; i<=len; i++) {
        character = RA_ReadByte() & 0x7f;
        strcat(str,  &character);
    }

    return str;
}


// printf-ish
void RA_WriteString(const char *fmt, ...) {
    
    uint16_t i;
    size_t len;
    char str[MAX_MSG_LEN];
    va_list argptr;
    
    va_start(argptr, fmt);
    len = vsnprintf(str, sizeof(str), fmt, argptr);
    va_end(argptr);

    len = strlen(str);
    
    if (!str || len == 0) {
        RA_WriteByte(0);
        return;
    }
    
    if (len > MAX_MSG_LEN - remote.queue.length) {
        RA_WriteByte(0);
        return;
    }

    for (i=0; i<len; i++) {
        remote.queue.data[remote.queue.length++] = str[i];
    }

    RA_WriteByte(0);
}

/**
 * Read an arbitrary amount of data from the message buffer
 */
void RA_ReadData(void *out, size_t len)
{
    memcpy(out, &(remote.queue_in.data[remote.queue_in.index]), len);
    remote.queue_in.index += len;
}


/**
 * Called when a player connects
 */
void RA_PlayerConnect(edict_t *ent)
{
    int8_t cl;
    cl = getEntOffset(ent) - 1;

    if (remote.state < RA_STATE_TRUSTED) {
        return;
    }

    RA_WriteByte(CMD_CONNECT);
    RA_WriteByte(cl);
    RA_WriteString("%s", ra_userinfo(cl));
}

/**
 * Called when a player disconnects
 */
void RA_PlayerDisconnect(edict_t *ent)
{
    int8_t cl;
    cl = getEntOffset(ent) - 1;

    if (remote.state < RA_STATE_TRUSTED) {
        return;
    }

    RA_WriteByte(CMD_DISCONNECT);
    RA_WriteByte(cl);
}

void RA_PlayerCommand(edict_t *ent) {
    
}

/**
 * Called for every broadcast print (bprintf), but only
 * on dedicated servers
 */
void RA_Print(uint8_t level, char *text)
{
    if (remote.state < RA_STATE_TRUSTED) {
        return;
    }
    
    if (!(remote.flags & RFL_CHAT)) {
        return;
    }

    RA_WriteByte(CMD_PRINT);
    RA_WriteByte(level);
    RA_WriteString("%s",text);
}

/**
 * Called when a player issues the teleport command
 */
void RA_Teleport(uint8_t client_id)
{
    if (remote.state < RA_STATE_TRUSTED) {
        return;
    }

    if (!(remote.flags & RFL_TELEPORT)) {
        return;
    }

    char *srv;
    if (gi.argc() > 1) {
        srv = gi.argv(1);
    } else {
        srv = "";
    }

    RA_WriteByte(CMD_COMMAND);
    RA_WriteByte(CMD_COMMAND_TELEPORT);
    RA_WriteByte(client_id);
    RA_WriteString("%s", srv);
}

/**
 * Called when a player changes part of their userinfo.
 * ex: name, skin, gender, rate, etc
 */
void RA_PlayerUpdate(uint8_t cl, const char *ui)
{
    if (remote.state < RA_STATE_TRUSTED) {
        return;
    }

    RA_WriteByte(CMD_PLAYERUPDATE);
    RA_WriteByte(cl);
    RA_WriteString("%s", ra_userinfo(cl));
}

/**
 * Called when a player issues the invite command
 */
void RA_Invite(uint8_t cl, const char *text)
{
    if (remote.state < RA_STATE_TRUSTED) {
        return;
    }

    if (!(remote.flags & RFL_INVITE)) {
        return;
    }

    RA_WriteByte(CMD_COMMAND);
    RA_WriteByte(CMD_COMMAND_INVITE);
    RA_WriteByte(cl);
    RA_WriteString(text);
}

/**
 * Called when a player issues the whois command
 */
void RA_Whois(uint8_t cl, const char *name)
{
    if (remote.state < RA_STATE_TRUSTED) {
        return;
    }

    if (!(remote.flags & RFL_WHOIS)) {
        return;
    }

    RA_WriteByte(CMD_COMMAND);
    RA_WriteByte(CMD_COMMAND_WHOIS);
    RA_WriteByte(cl);
    RA_WriteString(name);
}

/**
 * Called when a player dies
 */
void RA_Frag(uint8_t victim, uint8_t attacker, const char *vname, const char *aname)
{
    if (remote.state < RA_STATE_TRUSTED) {
        return;
    }

    if (!(remote.flags & RFL_FRAGS)) {
        return;
    }

    RA_WriteByte(CMD_FRAG);
    RA_WriteByte(victim);
    RA_WriteString("%s", vname);
    RA_WriteByte(attacker);
    RA_WriteString("%s", aname);
}

/**
 * Called when the map changes
 */
void RA_Map(const char *mapname)
{
    if (remote.state < RA_STATE_TRUSTED) {
        return;
    }

    RA_WriteByte(CMD_MAP);
    RA_WriteString("%s", mapname);
}


/**
 * Write something to a client
 */
void RA_SayClient(void)
{
    uint8_t client_id;
    uint8_t level;
    char *string;
    edict_t *ent;

    if (remote.state < RA_STATE_TRUSTED) {
        return;
    }

    client_id = RA_ReadByte();
    level = RA_ReadByte();
    string = RA_ReadString();

    ent = proxyinfo[client_id].ent;

    if (!ent) {
        return;
    }

    gi.cprintf(ent, level, string);
}

/**
 * Say something to everyone on the server
 */
void RA_SayAll(void)
{
    uint8_t i;
    char *string;

    if (remote.state < RA_STATE_TRUSTED) {
        return;
    }

    string = RA_ReadString();

    for (i=0; i<remote.maxclients; i++) {
        if (!proxyinfo[i].inuse) {
            continue;
        }

        /**
         * This way we send directly to the clients and
         * not to the dedicated server console triggering
         * a print to be sent back to the q2a server.
         *
         * Using gi.bprintf() instead would cause that.
         */
        gi.cprintf(
                proxyinfo[i].ent,
                PRINT_CHAT,
                "%s\n",
                string
        );
    }
}
