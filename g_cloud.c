/**
 * The cloud admin client maintains a separate TCP connection
 * to a remote management server. The client (q2 server) and
 * the cloud admin server mutually authenticate via asymmetrically
 * encrypted challenges. Once trusted, the client feeds the server
 * game information (player data, chats, frags, etc) and will do
 * as the cloud admin server instructs (enforcing bans/mutes,
 * telling players about other servers, etc).
 *
 * Server operators can connect to a cloud admin server by
 * registering using it's web interface, exchanging public keys,
 * and obtaining a unique identifier used in the connection
 * handshake.
 *
 * The tcp connection is encrypted using AES-CBC with keys rotating
 * about once an hour.
 */

#include "g_local.h"

cloud_t cloud;

/**
 * Sets up the cloud admin connection.
 *
 */
void CA_Init() {
    if (cloud.connection.socket) {
        return;
    }

    ReadCloudConfigFile();

    memset(&cloud, 0, sizeof(cloud));
    maxclients = gi.cvar("maxclients", "64", CVAR_LATCH);
    
    if (!cloud_enabled) {
        gi.cprintf(NULL, PRINT_HIGH, "Cloud Admin is disabled in your config.\n");
        return;
    }
    cloud.state = CA_STATE_DISCONNECTED;
    CA_printf("init...\n");

    if (!G_LoadKeys()) {
        cloud.state = CA_STATE_DISABLED;
        return;
    }

    cloud.connection.encrypted = cloud_encryption;
    
    if (!cloud_address[0]) {
        CA_dprintf("cloud_addr is not set, disabling\n");
        cloud.state = CA_STATE_DISABLED;
        return;
    }

    if (!cloud_port) {
        CA_dprintf("cloud_port is not set, disabling\n");
        cloud.state = CA_STATE_DISABLED;
        return;
    }

    G_StartThread(&CA_LookupAddress, NULL);

    // delay connection by a few seconds
    cloud.connect_retry_frame = FUTURE_CA_FRAME(5);
}

/**
 * Load config from disk. First load from q2 folder,
 * then the mod folder.
 */
void ReadCloudConfigFile()
{
    Q_snprintf(buffer, sizeof(buffer), "%s/%s", moddir, configfile_cloud->string);

    // read config from mod dir first, then global
    readCfgFile(buffer);
    readCfgFile(configfile_cloud->string);
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
    if (!cloud_dns[0]) {
        q2a_strcpy(cloud_dns, "64");
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
    if (cloud_dns[0] == '6' && v6) {
        return v6;
    } else if (cloud_dns[0] == '4' && v4) {
        return v4;
    } else if (cloud_dns[1] == '6' && v6) {
        return v6;
    } else if (cloud_dns[1] == '4' && v4) {
        return v4;
    }

    return NULL;
}


/**
 * Do a DNS lookup for remote server.
 * This is done in a dedicated thread to prevent blocking
 */
void CA_LookupAddress(void)
{
    char str_address[40];
    struct addrinfo hints, *res = 0;

    memset(&hints, 0, sizeof(hints));
    memset(&res, 0, sizeof(res));

    hints.ai_family         = AF_UNSPEC;     // either v6 or v4
    hints.ai_socktype       = SOCK_STREAM;   // TCP
    hints.ai_protocol       = 0;
    hints.ai_flags          = AI_ADDRCONFIG; // only return v6 addresses if interface is v6 capable

    // set to catch any specific errors
    errno = 0;

    // do the actual DNS lookup
    int err = getaddrinfo(cloud_address, va("%d",cloud_port), &hints, &res);

    if (err != 0) {
        CA_dprintf("DNS error\n");
        cloud.state = CA_STATE_DISABLED;
        return;
    } else {
        memset(&cloud.addr, 0, sizeof(struct addrinfo));

        // getaddrinfo can return multiple mixed v4/v6 results, select an appropriate one
        cloud.addr = select_addrinfo(res);

        if (!cloud.addr) {
            cloud.state = CA_STATE_DISABLED;
            CA_dprintf("problems resolving server address, disabling\n");
            return;
        }

        // for convenience
        if (res->ai_family == AF_INET6) {
            cloud.connection.ipv6 = qtrue;
        }

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

        CA_dprintf("server resolved to %s\n", str_address);
    }

    cloud.flags = cloud_flags;
    cloud.state = CA_STATE_DISCONNECTED;
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
void CA_Ping(void)
{
    if (cloud.state < CA_STATE_CONNECTED) {
        return;
    }

    // not time yet
    if (cloud.ping.frame_next > CA_FRAME) {
        return;
    }

    // there is already an outstanding ping
    if (cloud.ping.waiting) {
        if (cloud.ping.miss_count == PING_MISS_MAX) {
            CA_DisconnectedPeer();
            return;
        }
        cloud.ping.miss_count++;
    }

    // state stuff
    cloud.ping.frame_sent = CA_FRAME;
    cloud.ping.waiting = qtrue;
    cloud.ping.frame_next = CA_FRAME + SECS_TO_FRAMES(PING_FREQ_SECS);

    // send it
    CA_WriteByte(CMD_PING);
}

/**
 * Run once per server frame
 *
 */
void CA_RunFrame(void)
{
    // keep some time
    cloud.frame_number++;

    // remote admin is disabled, don't do anything
    if (cloud.state == CA_STATE_DISABLED) {
        return;
    }

    // everything we need to do while RA is connected
    if (cloud.state >= CA_STATE_CONNECTED) {

        // send any buffered messages to the server
        CA_SendMessages();

        // receive any pending messages from server
        CA_ReadMessages();

        // periodically make sure connection is alive
        CA_Ping();
    }

    // connection started already, check for completion
    if (cloud.state == CA_STATE_CONNECTING) {
        CA_CheckConnection();
    }

    // we're not connected, try again
    if (cloud.state == CA_STATE_DISCONNECTED) {
        CA_Connect();
    }
}

void CA_Shutdown(void)
{
    if (cloud.state == CA_STATE_DISABLED) {
        return;
    }

    /**
     * We have to call RA_SendMessages() specifically here because there won't be another
     * frame to send the buffered CMD_QUIT
     */
    CA_WriteByte(CMD_QUIT);
    CA_SendMessages();
    CA_Disconnect();

    freeaddrinfo(cloud.addr);
}

/**
 *
 */
void CA_Disconnect(void)
{
    if (cloud.state < CA_STATE_CONNECTED) {
        return;
    }

    closesocket(cloud.connection.socket);
    cloud.state = CA_STATE_DISCONNECTED;
}

/**
 * Get the frame number at which we should try another connection.
 * Increase the interval as attempts increase without a connection.
 */
static uint32_t next_connect_frame(void)
{
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
 * Make the connection to the cloud admin server
 */
void CA_Connect(void)
{
    int flags, ret;

    // only if we've waited long enough
    if (cloud.frame_number < cloud.connect_retry_frame) {
        return;
    }

    // clear the in and out buffers
    q2a_memset(&cloud.queue, 0, sizeof(message_queue_t));
    q2a_memset(&cloud.queue_in, 0, sizeof(message_queue_t));

    cloud.state = CA_STATE_CONNECTING;
    cloud.connection_attempts++;

    // create the socket
    cloud.connection.socket = socket(
            cloud.addr->ai_family,
            cloud.addr->ai_socktype,
            cloud.addr->ai_protocol
    );

    if (cloud.connection.socket == -1) {
        perror("connect");
        errno = 0;
        cloud.connect_retry_frame = next_connect_frame();

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
        CA_printf("error setting socket to non-blocking: (%d) %s\n", errno, strerror(errno));
        cloud.state = CA_STATE_DISCONNECTED;
        cloud.connect_retry_frame = FUTURE_CA_FRAME(30);
    }

    errno = 0;

    // make the actual connection
    ret = connect(cloud.connection.socket, cloud.addr->ai_addr, cloud.addr->ai_addrlen);

    if (ret == -1) {
        if (errno == EINPROGRESS) {
            // expected
        } else {
            perror("[cloud] connect error");
            errno = 0;
        }
    }

    /**
     * since we're non-blocking, the connection won't complete in this single server frame.
     * We have to select() for it on a later runframe. See CA_CheckConnection()
     */
}

/**
 * Check to see if the connection initiated by RA_Connect() has finished
 */
void CA_CheckConnection(void)
{
    ca_connection_t *c;
    uint32_t ret;
    qboolean connected = qfalse;
    struct sockaddr_storage addr;
    socklen_t len;
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
        uint32_t number;
        socklen_t len;

        len = sizeof(number);
        getsockopt(c->socket, SOL_SOCKET, SO_ERROR, &number, &len);
        if (number == 0) {
            connected = qtrue;
        }
#else
        if (FD_ISSET(c->socket, &c->set_w)) {
            connected = qtrue;
        }
#endif
    }

    if (ret == -1) {
        perror("CheckConnection");
        CA_printf("connection unfinished: %s\n", strerror(errno));
        closesocket(c->socket);
        cloud.state = CA_STATE_DISCONNECTED;
        cloud.connect_retry_frame = FUTURE_CA_FRAME(10);
        return;
    }

    // we need to make sure it's actually connected
    if (connected) {
        errno = 0;
        getpeername(c->socket, (struct sockaddr *)&addr, &len);

        if (errno) {
            cloud.connect_retry_frame = FUTURE_CA_FRAME(30);
            cloud.state = CA_STATE_DISCONNECTED;
            closesocket(c->socket);
        } else {
            CA_printf("connected\n");
            cloud.state = CA_STATE_CONNECTED;
            cloud.ping.frame_next = FUTURE_CA_FRAME(10);
            cloud.connected_frame = CA_FRAME;
            CA_SayHello();
        }
    }
}

/**
 * Send the contents of our outgoing buffer to the server
 */
void CA_SendMessages(void)
{
    if (cloud.state < CA_STATE_CONNECTING) {
        return;
    }

    // nothing to send
    if (!cloud.queue.length) {
        return;
    }

    uint32_t ret;
    struct timeval tv;
    tv.tv_sec = tv.tv_usec = 0;
    ca_connection_t *c;
    message_queue_t *q;
    message_queue_t e;

    c = &cloud.connection;
    q = &cloud.queue;

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
            if (c->encrypted && c->have_keys && cloud.state == CA_STATE_TRUSTED) {
                memset(&e, 0, sizeof(message_queue_t));
                e.length = G_SymmetricEncrypt(e.data, q->data, q->length);
                memset(q, 0, sizeof(message_queue_t));
                memcpy(q->data, e.data, e.length);
                q->length = e.length;
            }

            ret = send(c->socket, q->data, q->length, 0);
            if (ret == -1) {
                if (errno == EPIPE) {
                    gi.cprintf(NULL, PRINT_HIGH, "Remote side disconnected\n");
                    CA_DisconnectedPeer();
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
void CA_ReadMessages(void)
{
    uint32_t ret;
    struct timeval tv;
    message_queue_t *in;
    message_queue_t dec;

    if (cloud.state < CA_STATE_CONNECTING) {
        return;
    }

    tv.tv_sec = tv.tv_usec = 0;

    // save some typing
    in = &cloud.queue_in;

    while (qtrue) {
        FD_ZERO(&cloud.connection.set_r);
        FD_SET(cloud.connection.socket, &cloud.connection.set_r);

        // see if there is data waiting in the buffer for us to read
        ret = select(cloud.connection.socket + 1, &cloud.connection.set_r, NULL, NULL, &tv);
        if (ret == -1) {
            if (errno != EINTR) {
                perror("select");
                CA_DisconnectedPeer();
                errno = 0;
                return;
            }

            errno = 0;
        }

        // socket read buffer has data waiting in it
        if (ret) {
            ret = recv(cloud.connection.socket, in->data + in->length, QUEUE_SIZE - 1, 0);

            if (ret == 0) {
                CA_DisconnectedPeer();
                return;
            }

            if (ret == -1) {
                if (errno != EINTR) {
                    perror("recv");
                    CA_DisconnectedPeer();
                    return;
                }

                errno = 0;
            }

            in->length += ret;

            // decrypt if necessary
            if (cloud.connection.encrypted && cloud.connection.have_keys && cloud.state == CA_STATE_TRUSTED) {
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
    CA_ParseMessage();
}

/**
 * The server has let us know we are trusted.
 */
void CA_Trusted(void)
{
    CA_dprintf("connection trusted\n");
    cloud.state = CA_STATE_TRUSTED;
}

/**
 * Parse a newly received message and act accordingly
 */
void CA_ParseMessage(void)
{
    message_queue_t *msg = &cloud.queue_in;
    byte cmd;

    if (cloud.state == CA_STATE_DISABLED) {
        return;
    }

    // no data
    if (!cloud.queue_in.length) {
        return;
    }

    //hexDump("New Message", remote.queue_in.data, remote.queue_in.length);

    while (msg->index < msg->length) {
        cmd = CA_ReadByte();

        switch (cmd) {
        case SCMD_PONG:
            CA_ParsePong();
            break;
        case SCMD_COMMAND:
            CA_ParseCommand();
            break;
        case SCMD_HELLOACK:
            CA_VerifyServerAuth();
            break;
        case SCMD_TRUSTED:  // we just connected and authed successfully, tell server about us
            CA_Trusted();
            CA_Map(cloud.mapname);
            CA_PlayerList();
            break;
        case SCMD_ERROR:
            CA_ParseError();
            break;
        case SCMD_SAYCLIENT:
            CA_SayClient();
            break;
        case SCMD_SAYALL:
            CA_SayAll();
            break;
        case SCMD_KEY:
            CA_RotateKeys();
            break;
        case SCMD_GETPLAYERS:
            CA_PlayerList();
            break;
        }
    }

    // reset queue back to zero
    q2a_memset(&cloud.queue_in, 0, sizeof(message_queue_t));
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
qboolean CA_VerifyServerAuth(void)
{
    ca_connection_t *c = &cloud.connection;
    size_t dec_len;                     // Length of decrypted cleartext
    size_t enc_len;                     // Length of encrypted ciphertext
    byte response[RSA_LEN];             // Ciphertext
    byte response_plain[RSA_LEN];       // Cleartext version of response
    byte challenge_hash[DIGEST_LEN];    // The hash we calculate
    byte response_hash[DIGEST_LEN];     // Message digest of a challenge
    byte sv_challenge[CHALLENGE_LEN];   // The nonce sent from the server
    uint8_t offset = 0;                 // Used to find the parts of
                                        // server's auth response

    q2a_memset(response, 0, sizeof(response));
    CA_ReadData(response, CA_ReadShort());

    q2a_memset(response_plain, 0, sizeof(response_plain));
    dec_len = G_PrivateDecrypt(response_plain, response, sizeof(response));
    if (dec_len == 0) {
        CA_dprintf("zero bytes decrypted for server authentication\n");
        return qfalse;
    }

    // this is our original nonce hashed by the server and sent back to us
    q2a_memset(response_hash, 0, sizeof(response_hash));
    q2a_memcpy(response_hash, response_plain + offset, sizeof(response_hash));
    offset += sizeof(response_hash);

    // random nonce generated by server (we hash it and send back)
    q2a_memset(sv_challenge, 0, sizeof(sv_challenge));
    q2a_memcpy(sv_challenge, response_plain + offset, sizeof(sv_challenge));
    offset += sizeof(sv_challenge);

    if (cloud_encryption) {
        // the symmetric key
        q2a_memset(c->session_key, 0, sizeof(c->session_key));
        q2a_memcpy(c->session_key, response_plain + offset, sizeof(c->session_key));
        offset += sizeof(c->session_key);

        // the initial value for symmetric encryption
        q2a_memset(c->initial_value, 0, sizeof(c->initial_value));
        q2a_memcpy(c->initial_value, response_plain + offset, sizeof(c->initial_value));

        c->have_keys = qtrue;

        // reuse encrypt/decrypt contexts
        c->e_ctx = EVP_CIPHER_CTX_new();
        c->d_ctx = EVP_CIPHER_CTX_new();
    }

    // to compare with what server sent back to us
    G_MessageDigest(challenge_hash, c->cl_nonce, CHALLENGE_LEN);

    // if the hashes match, server is authenticated
    if (q2a_memcmp(challenge_hash, response_hash, DIGEST_LEN) == 0) {
        CA_dprintf("server authenticated\n");

        // reuse response and challenge_hash for our auth to server
        q2a_memset(response, 0, sizeof(response));
        q2a_memset(challenge_hash, 0, sizeof(challenge_hash));
        G_MessageDigest(challenge_hash, sv_challenge, sizeof(sv_challenge));
        enc_len = G_PublicEncrypt(cloud.connection.server_key, response, challenge_hash, DIGEST_LEN);

        // send our response to server's challenge
        CA_WriteByte(CMD_AUTH);
        CA_WriteShort(enc_len);
        CA_WriteData(response, enc_len);
        CA_SendMessages();

        cloud.connection_attempts = 0;
        cloud.connection.auth_fail_count = 0;
        return qtrue;
    } else {
        CA_dprintf("server auth error: %s\n", ERR_error_string(ERR_get_error(), NULL));
        cloud.connection.auth_fail_count++;

        if (cloud.connection.auth_fail_count > AUTH_FAIL_LIMIT) {
            CA_dprintf("too many auth failures, giving up\n");
            cloud.state = CA_STATE_DISABLED;
        }
        return qfalse;
    }
}

/**
 * Server sent over a command.
 */
void CA_ParseCommand(void)
{
    char *cmd;

    // we should never get here if we're not trusted, but just in case
    if (cloud.state < CA_STATE_TRUSTED) {
        return;
    }

    cmd = CA_ReadString();

    // cram it into the command buffer
    gi.AddCommandString(cmd);
}

/**
 * Not really much to parse...
 */
void CA_ParsePong(void)
{
    cloud.ping.waiting = qfalse;
    cloud.ping.miss_count = 0;
}

/**
 * The server sent us new symmetric encryption keys, parse them and
 * start using them
 */
void CA_RotateKeys(void)
{
    ca_connection_t *c;
    c = &cloud.connection;

    CA_ReadData(c->session_key, AESKEY_LEN);
    CA_ReadData(c->initial_value, AESBLOCK_LEN);
}

/**
 * There was a sudden disconnection mid-stream. Reconnect after an
 * appropriate pause
 */
void CA_DisconnectedPeer(void)
{
    uint8_t secs;

    if (cloud.state < CA_STATE_CONNECTED) {
        return;
    }

    CA_printf("connection lost\n");

    cloud.state = CA_STATE_DISCONNECTED;
    cloud.connection.trusted = qfalse;
    cloud.connection.have_keys = qfalse;
    cloud.disconnect_count++;
    memset(&cloud.connection.session_key[0], 0, AESKEY_LEN);
    memset(&cloud.connection.initial_value[0], 0, AESBLOCK_LEN);

    // try reconnecting a reasonably random amount of time later
    srand((unsigned) time(NULL));
    secs = rand() & 0xff;
    cloud.connect_retry_frame = FUTURE_CA_FRAME(10) + secs;
    CA_dprintf("trying to reconnect in %d seconds\n",
        FRAMES_TO_SECS(cloud.connect_retry_frame - cloud.frame_number)
    );
}

/**
 * Send info about all the players connected. First send the number of players
 * to follow, then for each: the player id followed by the userinfo and finally
 * the version string for the client the player is using.
 */
void CA_PlayerList(void)
{
    uint8_t count, i;
    count = 0;

    if (cloud.state < CA_STATE_TRUSTED) {
        return;
    }

    for (i=0; i<cloud.maxclients; i++) {
        if (proxyinfo[i].inuse) {
            count++;
        }
    }

    CA_WriteByte(CMD_PLAYERLIST);
    CA_WriteByte(count);

    for (i=0; i<cloud.maxclients; i++) {
        if (proxyinfo[i].inuse) {
            CA_WriteByte(i);
            CA_WriteString("%s", proxyinfo[i].userinfo);
        }
    }
}

/**
 * Immediately after connecting, we have to say hi, giving the server our
 * information. Once the server acknowledges we can start sending game data.
 *
 * The last section of data is a random nonce. The server will encrypt this
 * and send it back to us. We then decrypt and check if it matches, if so,
 * the server is who we think it is and is considered trusted.
 */
void CA_SayHello(void)
{
    // don't bother if we're fully connected
    if (cloud.state == CA_STATE_TRUSTED) {
        return;
    }

    // random data to check server auth
    RAND_bytes(cloud.connection.cl_nonce, sizeof(cloud.connection.cl_nonce));

    byte challenge[RSA_LEN];
    q2a_memset(challenge, 0, sizeof(challenge));
    G_PublicEncrypt(cloud.connection.server_key, challenge,
            cloud.connection.cl_nonce, CHALLENGE_LEN);

    CA_WriteLong(MAGIC_CLIENT);
    CA_WriteByte(CMD_HELLO);
    CA_WriteString(cloud_uuid);
    CA_WriteLong(Q2A_REVISION);
    CA_WriteShort(cloud.port);
    CA_WriteByte(cloud.maxclients);
    CA_WriteByte(cloud_encryption ? 1 : 0);
    CA_WriteData(challenge, RSA_LEN);
}

/**
 * The server replied negatively to something
 */
void CA_ParseError(void)
{
    uint8_t client_id, reason_id;
    char *reason;

    gi.dprintf("parsing error\n");

    client_id = CA_ReadByte(); // will be -1 if not player specific
    reason_id = CA_ReadByte();
    reason = CA_ReadString();

    // Where to output the error msg
    if (client_id == -1) {
        gi.cprintf(NULL, PRINT_HIGH, "%s\n", reason);
    } else {
        //gi.cprintf(proxyinfo[client_id].ent, PRINT_HIGH, "%s\n", reason);
        gi.cprintf(NULL, PRINT_HIGH, "error msg here\n");
    }

    // serious enough to disconnect
    if (reason_id >= 200) {
        closesocket(cloud.connection.socket);
        cloud.state = CA_STATE_DISABLED;
        freeaddrinfo(cloud.addr);
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
void CA_InitBuffer() {
    q2a_memset(&cloud.queue, 0, sizeof(message_queue_t));
}

/**
 * Read a single byte from the message buffer
 */
uint8_t CA_ReadByte(void)
{
    unsigned char b = cloud.queue_in.data[cloud.queue_in.index++];
    return b & 0xff;
}

/**
 * Write a single byte to the message buffer
 */
void CA_WriteByte(uint8_t b)
{
    cloud.queue.data[cloud.queue.length++] = b & 0xff;
}

/**
 * Read a short (2 bytes) from the message buffer
 */
uint16_t CA_ReadShort(void)
{
    return    (cloud.queue_in.data[cloud.queue_in.index++] +
            (cloud.queue_in.data[cloud.queue_in.index++] << 8)) & 0xffff;
}

/**
 * Write 2 bytes to the message buffer
 */
void CA_WriteShort(uint16_t s)
{
    cloud.queue.data[cloud.queue.length++] = s & 0xff;
    cloud.queue.data[cloud.queue.length++] = (s >> 8) & 0xff;
}

/**
 * Read 4 bytes from the message buffer
 */
int32_t CA_ReadLong(void)
{
    return    cloud.queue_in.data[cloud.queue_in.index++] +
            (cloud.queue_in.data[cloud.queue_in.index++] << 8) +
            (cloud.queue_in.data[cloud.queue_in.index++] << 16) +
            (cloud.queue_in.data[cloud.queue_in.index++] << 24);
}

/**
 * Write 4 bytes (long) to the message buffer
 */
void CA_WriteLong(uint32_t i)
{
    cloud.queue.data[cloud.queue.length++] = i & 0xff;
    cloud.queue.data[cloud.queue.length++] = (i >> 8) & 0xff;
    cloud.queue.data[cloud.queue.length++] = (i >> 16) & 0xff;
    cloud.queue.data[cloud.queue.length++] = (i >> 24) & 0xff;
}

/**
 * Write an arbitrary amount of data from the message buffer
 */
void CA_WriteData(const void *data, size_t length)
{
    uint32_t i;
    for (i=0; i<length; i++) {
        CA_WriteByte(((byte *) data)[i]);
    }
}

/**
 * Read a null terminated string from the buffer
 */
char *CA_ReadString(void)
{
    static char str[MAX_STRING_CHARS];
    static char character;
    size_t i, len = 0;

    do {
        len++;
    } while (cloud.queue_in.data[(cloud.queue_in.index + len)] != 0);

    memset(&str, 0, MAX_STRING_CHARS);

    for (i=0; i<=len; i++) {
        character = CA_ReadByte() & 0x7f;
        strcat(str,  &character);
    }

    return str;
}


// printf-ish
void CA_WriteString(const char *fmt, ...) {
    
    uint16_t i;
    size_t len;
    char str[MAX_MSG_LEN];
    va_list argptr;
    
    va_start(argptr, fmt);
    len = vsnprintf(str, sizeof(str), fmt, argptr);
    va_end(argptr);

    len = strlen(str);
    
    if (!*str || len == 0) {
        CA_WriteByte(0);
        return;
    }
    
    if (len > MAX_MSG_LEN - cloud.queue.length) {
        CA_WriteByte(0);
        return;
    }

    for (i=0; i<len; i++) {
        cloud.queue.data[cloud.queue.length++] = str[i];
    }

    CA_WriteByte(0);
}

/**
 * Read an arbitrary amount of data from the message buffer
 */
void CA_ReadData(void *out, size_t len)
{
    memcpy(out, &(cloud.queue_in.data[cloud.queue_in.index]), len);
    cloud.queue_in.index += len;
}


/**
 * Report when a player connects. The Cloud Admin server will make decisions
 * based on information like VPN status and client version string, so this
 * data needs to be available before sending this message.
 *
 * Called from doClientCommand() upon receiving the client version string. This
 * can be delayed slightly, so we can't call this from ClientConnect() or even
 * ClientBegin().
 */
void CA_PlayerConnect(edict_t *ent)
{
    int8_t cl;
    cl = getEntOffset(ent) - 1;

    if (cloud.state < CA_STATE_TRUSTED) {
        return;
    }

    CA_WriteByte(CMD_CONNECT);
    CA_WriteByte(cl);
    CA_WriteString("%s", proxyinfo[cl].userinfo);
}

/**
 * Called when a player disconnects
 */
void CA_PlayerDisconnect(edict_t *ent)
{
    int8_t cl;
    cl = getEntOffset(ent) - 1;

    if (cloud.state < CA_STATE_TRUSTED) {
        return;
    }

    CA_WriteByte(CMD_DISCONNECT);
    CA_WriteByte(cl);
}

void CA_PlayerCommand(edict_t *ent) {
    
}

/**
 * Called for every broadcast print (bprintf), but only
 * on dedicated servers
 */
void CA_Print(uint8_t level, char *text)
{
    if (cloud.state < CA_STATE_TRUSTED) {
        return;
    }
    
    if (!(cloud.flags & RFL_CHAT)) {
        return;
    }

    CA_WriteByte(CMD_PRINT);
    CA_WriteByte(level);
    CA_WriteString("%s",text);
}

/**
 * Called when a player issues the teleport command
 */
void CA_Teleport(uint8_t client_id)
{
    if (cloud.state < CA_STATE_TRUSTED) {
        return;
    }

    if (!(cloud.flags & RFL_TELEPORT)) {
        return;
    }

    char *srv;
    if (gi.argc() > 1) {
        srv = gi.argv(1);
    } else {
        srv = "";
    }

    CA_WriteByte(CMD_COMMAND);
    CA_WriteByte(CMD_COMMAND_TELEPORT);
    CA_WriteByte(client_id);
    CA_WriteString("%s", srv);
}

/**
 * Called when a player changes part of their userinfo.
 * ex: name, skin, gender, rate, etc
 */
void CA_PlayerUpdate(uint8_t cl, const char *ui)
{
    if (cloud.state < CA_STATE_TRUSTED) {
        return;
    }

    CA_WriteByte(CMD_PLAYERUPDATE);
    CA_WriteByte(cl);
    CA_WriteString("%s", proxyinfo[cl].userinfo);
}

/**
 * Called when a player issues the invite command
 */
void CA_Invite(uint8_t cl, const char *text)
{
    if (cloud.state < CA_STATE_TRUSTED) {
        return;
    }

    if (!(cloud.flags & RFL_INVITE)) {
        return;
    }

    CA_WriteByte(CMD_COMMAND);
    CA_WriteByte(CMD_COMMAND_INVITE);
    CA_WriteByte(cl);
    CA_WriteString(text);
}

/**
 * Called when a player issues the whois command
 */
void CA_Whois(uint8_t cl, const char *name)
{
    if (cloud.state < CA_STATE_TRUSTED) {
        return;
    }

    if (!(cloud.flags & RFL_WHOIS)) {
        return;
    }

    CA_WriteByte(CMD_COMMAND);
    CA_WriteByte(CMD_COMMAND_WHOIS);
    CA_WriteByte(cl);
    CA_WriteString(name);
}

/**
 * Called when a player dies
 */
void CA_Frag(uint8_t victim, uint8_t attacker)
{
    if (cloud.state < CA_STATE_TRUSTED) {
        return;
    }

    if (!(cloud.flags & RFL_FRAGS)) {
        return;
    }

    CA_WriteByte(CMD_FRAG);
    CA_WriteByte(victim);
    CA_WriteByte(attacker);
}

/**
 * Called when the map changes
 */
void CA_Map(const char *mapname)
{
    if (cloud.state < CA_STATE_TRUSTED) {
        return;
    }

    CA_WriteByte(CMD_MAP);
    CA_WriteString("%s", mapname);
}


/**
 * Write something to a client
 */
void CA_SayClient(void)
{
    uint8_t client_id;
    uint8_t level;
    char *string;
    edict_t *ent;

    if (cloud.state < CA_STATE_TRUSTED) {
        return;
    }

    client_id = CA_ReadByte();
    level = CA_ReadByte();
    string = CA_ReadString();

    ent = proxyinfo[client_id].ent;

    if (!ent) {
        return;
    }

    gi.cprintf(ent, level, string);
}

/**
 * Say something to everyone on the server
 */
void CA_SayAll(void)
{
    uint8_t i, level;
    char *string;

    if (cloud.state < CA_STATE_TRUSTED) {
        return;
    }

    level = CA_ReadByte();
    string = CA_ReadString();

    for (i=0; i<cloud.maxclients; i++) {
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
                level,
                "%s\n",
                string
        );
    }
}

/**
 * Format a string time_spec based on a quantity of frames
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
 * Put the current IP address and ports of the connected cloud admin server into dst
 *
 * dst needs to be at least INET6_ADDRSTRLEN in size
 */
void getCloudIP(char *remoteip, int *remoteport, int *localport)
{
    char addr[INET6_ADDRSTRLEN];

    // IPv6
    if (cloud.addr->ai_family == AF_INET6) {
        q2a_inet_ntop(
            cloud.addr->ai_family,
            &((struct sockaddr_in6 *) cloud.addr->ai_addr)->sin6_addr,
            addr,
            sizeof(addr)
        );

        q2a_strcpy(remoteip, va("[%s]", addr));
    } else {  // IPv4
        q2a_inet_ntop(
            cloud.addr->ai_family,
            &((struct sockaddr_in *) cloud.addr->ai_addr)->sin_addr,
            addr,
            sizeof(addr)
        );

        q2a_strcpy(remoteip, va("%s", addr));
    }

    *localport = (int)((struct sockaddr_in *) cloud.addr->ai_addr)->sin_port;
    remoteport = &cloud_port;
}

/**
 * Main command runner for "sv !cloud <cmd>" server command
 */
void cloudRun(int startarg, edict_t *ent, int client) {
    char *command;
    qboolean connected;
    char connected_time[25];
    char connected_ip[INET6_ADDRSTRLEN];
    int local_port;
    int remote_port;

    if (gi.argc() <= startarg) {
        gi.cprintf(ent, PRINT_HIGH, "Usage: %s\n", CLOUDCMD_LAYOUT);
        return;
    }

    connected = cloud.state == CA_STATE_TRUSTED;
    command = gi.argv(startarg);

    if (Q_stricmp(command, "status") == 0) {
        gi.cprintf(ent, PRINT_HIGH, "[cloud admin status]\n");
        if (connected) {
            getCloudIP(connected_ip, &remote_port, &local_port);
            gi.cprintf(ent, PRINT_HIGH, "%-20s%s\n", "connected to:", va("%s:%d", connected_ip, cloud_port));
        } else {
            gi.cprintf(ent, PRINT_HIGH, "%-20s%s\n", "host:", va("%s:%d", cloud_address, cloud_port));
        }
        gi.cprintf(ent, PRINT_HIGH, "%-20s%s\n", "client uuid:", cloud_uuid);
        if (cloud.state == CA_STATE_DISABLED) {
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

    if (Q_stricmp(command, "reconnect") == 0) {
        CA_Disconnect();
        q2a_memset(&cloud, 0, sizeof(cloud_t));
        CA_printf("disconnected\n");
        CA_Init();
        return;
    }

    if (Q_stricmp(command, "disconnect") == 0) {
        CA_Disconnect();
        q2a_memset(&cloud, 0, sizeof(cloud_t));
        CA_printf("disconnected\n");
        return;
    }

    if (Q_stricmp(command, "connect") == 0) {
        CA_Init();
        return;
    }
}

/**
 * Printf something to the server console prepended with [cloud]
 */
void CA_printf(char *fmt, ...) {
    char cbuffer[8192];
    va_list arglist;

    // convert to string
    va_start(arglist, fmt);
    Q_vsnprintf(cbuffer, sizeof(cbuffer), fmt, arglist);
    va_end(arglist);

    gi.cprintf(NULL, PRINT_HIGH, "[cloud] %s", cbuffer);
}

/**
 * Debug printing to the server console/log. Only outputs
 * if the debug flag is set
 */
void CA_dprintf(char *fmt, ...) {
    char cbuffer[8192];
    va_list arglist;

    if (!RFL(DEBUG)) {
        return;
    }
    // convert to string
    va_start(arglist, fmt);
    Q_vsnprintf(cbuffer, sizeof(cbuffer), fmt, arglist);
    va_end(arglist);

    gi.cprintf(NULL, PRINT_HIGH, "[cloud] %s", cbuffer);
}
