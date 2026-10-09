/**
 * The cloud admin client maintains a separate TCP connection to a remote
 * management server. The client (q2 server) and the cloud admin server
 * mutually authenticate via asymmetrically encrypted challenges. Once trusted,
 * the client feeds the server game information (player data, chats, frags,
 * etc) and will do as the cloud admin server instructs (enforcing bans/mutes,
 * telling players about other servers, etc).
 *
 * Server operators can connect to a cloud admin server by registering using
 * it's web interface, exchanging public keys, and obtaining a unique
 * identifier used in the connection handshake.
 *
 * The tcp connection is encrypted using AES-CBC with keys rotating about once
 * an hour. AES-GCM would normally be a better mode as it's more modern and has
 * authentication built-in, but it's too vulnerable in the event of IV reuse.
 */

#include "g_local.h"
#include <pthread.h>

cloud_t cloud;

/**
 * Sets up the cloud admin connection.
 */
void cloudInit() {
    if (cloud.connection.socket) {
        return;
    }

    cloudReadConfigFile();

    q2a_memset(&cloud, 0, sizeof(cloud));
    maxclients = gi.cvar("maxclients", "64", CVAR_LATCH);
    
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

    cloudStartThread(&cloudLookupAddress, NULL);

    // delay connection by a few seconds
    cloud.connect_retry_frame = FUTURE_CA_FRAME(5);
}

/**
 * Load config from disk. First load from q2 folder, then the mod folder.
 */
void cloudReadConfigFile() {
    Q_snprintf(buffer, sizeof(buffer), "%s/%s", moddir, configfile_cloud->string);
    readCfgFile(buffer);
    readCfgFile(configfile_cloud->string);
}

/**
 * getaddrinfo's returns a linked-list of struct addrinfo. Figure out which
 * result is the one we want (IPv6/IPv4)
 */
static struct addrinfo *cloudSelectAddrinfo(struct addrinfo *a) {
    static struct addrinfo *v4, *v6;

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
 * Do a DNS lookup for remote server. This is done in a dedicated thread to
 * prevent blocking. Otherwise the server (and all current players) will
 * freeze in place when new players connect until their PTR record is resolved.
 */
void cloudLookupAddress(void) {
    char str_address[40];
    struct addrinfo hints, *res = 0;

    q2a_memset(&hints, 0, sizeof(hints));
    q2a_memset(&res, 0, sizeof(res));

    hints.ai_family         = AF_UNSPEC;     // either v6 or v4
    hints.ai_socktype       = SOCK_STREAM;   // TCP
    hints.ai_protocol       = 0;
    hints.ai_flags          = AI_ADDRCONFIG; // only for v6 addresses

    errno = 0;

    int err = getaddrinfo(cloud_config.address, va("%d",cloud_config.port), &hints, &res);
    if (err != 0) {
        cloudDPrintf("DNS error\n");
        cloud.state = CLOUD_STATE_DISABLED;
        return;
    } else {
        q2a_memset(&cloud.addr, 0, sizeof(struct addrinfo));

        // getaddrinfo can return multiple mixed v4/v6 results
        cloud.addr = cloudSelectAddrinfo(res);

        if (!cloud.addr) {
            cloud.state = CLOUD_STATE_DISABLED;
            cloudDPrintf("problems resolving server address, disabling\n");
            return;
        }

        if (res->ai_family == AF_INET6) {
            cloud.connection.ipv6 = true;
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

        cloudDPrintf("server resolved to %s\n", str_address);
    }

    cloud.flags = cloud_config.flags;
    cloud.state = CLOUD_STATE_DISCONNECTED;
}

/**
 * Only DNS lookups use this
 */
void cloudStartThread(void *func, void *arg) {
#if _WIN32
    DWORD tid;
    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) func, 0, 0, &tid);
#else
    pthread_t dnsthread;
    pthread_create(&dnsthread, 0, func, 0);
#endif
}

/**
 * Output to the q2 server console (if flags agree)
 */
void cloudDebugPrint(char *str) {
    if (!CFL(DEBUG)) {
        return;
    }
    gi.cprintf(NULL, PRINT_HIGH, "%s\n", str);
}

/**
 * Periodically ping the server to know if the connection is still open. I'm
 * not 100% sure this is necessary. TCP has built-in mechanisms for maintaining
 * a connection if when no data is flowing.
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
 * Run once per server frame. The default tick rate is 10hz or once every 0.1
 * seconds, but servers like q2pro support variable framerates (divisible by 10
 * up to 60), so this could run much more frequently.
 */
void cloudFrame(void) {
    cloud.frame_number++;
    if (cloud.state == CLOUD_STATE_DISABLED) {
        return;
    }

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
 * Local q2 server is shutting down, inform the backend too
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
    freeaddrinfo(cloud.addr);
}

/**
 * TCP connection was broken, reset local state in preparation for reconnecting
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
 * Get the frame number at which we should try another connection. Increase the
 * interval as attempts increase without a connection. The longer the
 * connection is inactive, the more infrequently it tries to reconnect.
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
 * Make the connection to the backend
 */
void cloudConnect(void) {
    int flags, ret;

    if (cloud.frame_number < cloud.connect_retry_frame) {
        return;
    }

    q2a_memset(&cloud.queue_out, 0, sizeof(message_queue_t));
    q2a_memset(&cloud.queue_in, 0, sizeof(message_queue_t));

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
        cloud.state = CLOUD_STATE_DISCONNECTED;
        cloud.connect_retry_frame = FUTURE_CA_FRAME(30);
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
 * Check to see if the connection initiated by cloudConnect() has finished
 */
void cloudCheckConnection(void) {
    cloud_connection_t *c;
    uint32_t ret;
    bool connected = false;
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
            connected = true;
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
        getpeername(c->socket, (struct sockaddr *)&addr, &len);

        if (errno) {
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
 * Send the contents of our outgoing buffer to the server
 */
void cloudSendMessages(void) {
    if (cloud.state < CLOUD_STATE_CONNECTING) {
        return;
    }

    if (!cloud.queue_out.length) {
        return;
    }

    uint32_t ret;
    struct timeval tv;
    tv.tv_sec = tv.tv_usec = 0;
    cloud_connection_t *c;
    message_queue_t *q, e;

    c = &cloud.connection;
    q = &cloud.queue_out;

    while (true) {
        FD_ZERO(&c->set_w);
        FD_SET(c->socket, &c->set_w);

        // see if the socket is ready to send data
        ret = select((int) c->socket + 1, NULL, &c->set_w, NULL, &tv);
        if (ret == -1) {
            perror("send select");
            errno = 0;
        }

        // socket write buffer is ready, send
        if (ret) {
            if (c->encrypted && c->have_keys && cloud.state == CLOUD_STATE_TRUSTED) {
                q2a_memset(&e, 0, sizeof(message_queue_t));
                e.length = cryptoSymmetricEncrypt(e.data, q->data, q->length);
                q2a_memset(q, 0, sizeof(message_queue_t));
                q2a_memcpy(q->data, e.data, e.length);
                q->length = e.length;
            }
            ret = send(c->socket, (const char *) q->data, q->length, 0);
            if (ret == -1) {
                if (errno == EPIPE) {
                    gi.cprintf(NULL, PRINT_HIGH, "Remote side disconnected\n");
                    cloudPeerDisconnected();
                    errno = 0;
                    break;
                }
                perror("send error");
                errno = 0;
            } else {
                // shift off the data we just sent
                q2a_memmove(q->data, q->data + ret, q->length - ret);
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
void cloudReadMessages(void) {
    uint32_t ret;
    byte temp_iv[DIGEST_LEN];
    struct timeval tv;
    message_queue_t *in, dec;

    if (cloud.state < CLOUD_STATE_CONNECTING) {
        return;
    }

    tv.tv_sec = tv.tv_usec = 0;
    in = &cloud.queue_in;

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
        }

        // socket read buffer has data waiting in it
        if (ret) {
            if (in->length >= QUEUE_SIZE - 1) {
                // already full - stop reading until cloudParseMessage() has
                // drained some of it, instead of overflowing in->data[]
                break;
            }
            ret = recv(cloud.connection.socket, (char *) in->data + in->length,
                    (QUEUE_SIZE - 1) - in->length, 0);

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
                errno = 0;
            }
            in->length += ret;

            // decrypt if necessary
            if (cloud.connection.encrypted && cloud.connection.have_keys && cloud.state == CLOUD_STATE_TRUSTED) {
                q2a_memcpy(temp_iv, in->data, AES_IV_LEN);
                q2a_memset(&dec, 0, sizeof(message_queue_t));
                dec.length = cryptoSymmetricDecrypt(dec.data, in->data, in->length);
                q2a_memset(in->data, 0, in->length);
                q2a_memcpy(in->data, dec.data, dec.length);
                in->length = dec.length;
                q2a_memcpy(cloud.connection.initial_value, temp_iv, AES_IV_LEN);
            }
        } else {
            // no data has been sent to read
            break;
        }
    }
    cloudParseMessage();
}

/**
 * The server has let us know we are trusted.
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
 * Parse a newly received message and act accordingly
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

    while (msg->index < msg->length) {
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

    // reset queue back to zero
    q2a_memset(&cloud.queue_in, 0, sizeof(message_queue_t));
}

/**
 * Is a a digital signature valid?
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
 * Authenticate the backend
 *
 * 1. Decrypt the nonce sent back from the server and check if it matches
 * 2. If the encryption is requested, parse the AES 128 key and IV
 * 3. Read the plaintext nonce from the server, encrypt and send back
 *    to auth the client
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
    q2a_memset(response_plain, 0, sizeof(response_plain));
    dec_len = cryptoPrivateDecrypt(response_plain, response, sizeof(response));
    if (dec_len == 0) {
        cloudDPrintf("zero bytes decrypted for server authentication\n");
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

        // reuse encrypt/decrypt contexts
        c->e_ctx = EVP_CIPHER_CTX_new();
        c->d_ctx = EVP_CIPHER_CTX_new();
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
 * Server sent over a command. It unconditionally executes any command the
 * backend sends over. Generally this is a bad, but since we can't get here
 * unless the connection is authenticated, it's probably safe enough. Maybe
 * add some sanity checks later.
 */
void cloudParseCommand(void) {
    char *cmd;

    if (cloud.state < CLOUD_STATE_TRUSTED) {
        return;
    }
    cmd = cloudReadString();
    gi.AddCommandString(cmd);
}

/**
 * Backend responded to our PING
 */
void cloudParsePong(void) {
    cloud.ping.waiting = false;
    cloud.ping.miss_count = 0;
}

/**
 * The server sent us new symmetric encryption keys, parse them and
 * start using them
 */
void cloudRotateKeys(void) {
    cloud_connection_t *c;
    c = &cloud.connection;

    cloudReadData(c->session_key, AESKEY_LEN);
    cloudReadData(c->initial_value, AESBLOCK_LEN);
}

/**
 * There was a sudden disconnection mid-stream. Reconnect after an appropriate
 * delay.
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
 * Send info about all the players connected. First send the number of players
 * to follow, then for each: the player id followed by the userinfo and finally
 * the version string for the client the player is using.
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
 * Immediately after connecting, we have to say hi, giving the server our
 * information. Once the server acknowledges we can start sending game data.
 *
 * The last section of data is a random nonce. The server will encrypt this
 * and send it back to us. We then decrypt and check if it matches, if so,
 * the server is who we think it is and is considered trusted.
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
    cloudWriteString(cloud_config.uuid);
    cloudWriteLong(Q2A_REVISION);
    cloudWriteShort(cloud.port);
    cloudWriteByte(cloud.maxclients);
    cloudWriteByte(cloud_config.encryption ? 1 : 0);
    cloudWriteData(challenge, RSA_LEN);
}

/**
 * The server replied negatively to something
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

    logEvent(LT_CLOUDERROR, 0, NULL, reason ? reason : "", reason_id, 0.0, false);

    // Where to output the error msg
    if (client_id == -1) {
        gi.cprintf(NULL, PRINT_HIGH, "%s\n", reason);
    } else {
        gi.cprintf(NULL, PRINT_HIGH, "error msg here\n");
    }

    // serious enough to disconnect
    if (reason_id >= 200) {
        closesocket(cloud.connection.socket);
        cloud.state = CLOUD_STATE_DISABLED;
        freeaddrinfo(cloud.addr);
        logEvent(LT_CLOUDDISCONNECT, 0, NULL,
            va("disabled by server: %s", reason ? reason : ""),
            cloud.disconnect_count, 0.0, false);
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
 * Read a single byte from the message buffer
 */
uint8_t cloudReadByte(void) {
    unsigned char b = cloud.queue_in.data[cloud.queue_in.index++];
    return b & 0xff;
}

/**
 * Write a single byte to the message buffer
 */
void cloudWriteByte(uint8_t b) {
    if (cloud.queue_out.length >= QUEUE_SIZE) {
        cloudDPrintf("outgoing queue full, dropping byte\n");
        return;
    }
    cloud.queue_out.data[cloud.queue_out.length++] = b & 0xff;
}

/**
 * Read a short (2 bytes) from the message buffer
 */
uint16_t cloudReadShort(void) {
    message_queue_t *q = &cloud.queue_in;
    int s = q->data[q->index] + (q->data[q->index + 1] << 8);
    q->index += 2;
    return s & 0xffff;
}

/**
 * Write 2 bytes to the message buffer
 */
void cloudWriteShort(uint16_t s) {
    if (cloud.queue_out.length + 2 > QUEUE_SIZE) {
        cloudDPrintf("outgoing queue full, dropping short\n");
        return;
    }
    cloud.queue_out.data[cloud.queue_out.length++] = s & 0xff;
    cloud.queue_out.data[cloud.queue_out.length++] = (s >> 8) & 0xff;
}

/**
 * Read 4 bytes from the message buffer
 */
int32_t cloudReadLong(void) {
    message_queue_t *q = &cloud.queue_in;
    int num = q->data[q->index] + (q->data[q->index + 1] << 8) +
            (q->data[q->index + 2] << 16) + (q->data[q->index + 3] << 24);
    q->index += 4;
    return num;
}

/**
 * Write 4 bytes (long) to the message buffer
 */
void cloudWriteLong(uint32_t i) {
    if (cloud.queue_out.length + 4 > QUEUE_SIZE) {
        cloudDPrintf("outgoing queue full, dropping long\n");
        return;
    }
    cloud.queue_out.data[cloud.queue_out.length++] = i & 0xff;
    cloud.queue_out.data[cloud.queue_out.length++] = (i >> 8) & 0xff;
    cloud.queue_out.data[cloud.queue_out.length++] = (i >> 16) & 0xff;
    cloud.queue_out.data[cloud.queue_out.length++] = (i >> 24) & 0xff;
}

/**
 * Write an arbitrary amount of data from the message buffer
 */
void cloudWriteData(const void *data, size_t length) {
    uint32_t i;
    for (i=0; i<length; i++) {
        cloudWriteByte(((byte *) data)[i]);
    }
}

/**
 * Read a null terminated string from the buffer
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
    }
    return str;
}


// printf-ish
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
    if (cloud.queue_out.length + len + 1 > QUEUE_SIZE) {
        cloudWriteByte(0);
        return;
    }

    for (i=0; i<len; i++) {
        cloud.queue_out.data[cloud.queue_out.length++] = str[i];
    }

    cloudWriteByte(0);
}

/**
 * Read an arbitrary amount of data from the message buffer
 */
void cloudReadData(void *out, size_t len) {
    q2a_memcpy(out, &(cloud.queue_in.data[cloud.queue_in.index]), len);
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
 * Called when a player disconnects
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
 * Called for every broadcast print (bprintf), but only
 * on dedicated servers
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
 * Called when a player issues the teleport command
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
 * Called when a player changes part of their userinfo.
 * ex: name, skin, gender, rate, etc
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
 * Called when a player issues the invite command
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
    cloudWriteString(text);
}

/**
 * Called when a player issues the whois command
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
    cloudWriteString(name);
}

/**
 * Called when a player dies
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
 * Called when the map changes
 */
void cloudMap(const char *mapname) {
    if (cloud.state < CLOUD_STATE_TRUSTED) {
        return;
    }
    cloudWriteByte(CMD_MAP);
    cloudWriteString("%s", mapname);
}


/**
 * Write something to a client
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
    if (client_id >= cloud.maxclients) {
        return;
    }
    ent = proxyinfo[client_id].ent;
    if (!ent) {
        return;
    }
    gi.cprintf(ent, level, string);
}

/**
 * Say something to everyone on the server
 */
void cloudSayAll(void) {
    uint8_t i, level;
    char *string;

    if (cloud.state < CLOUD_STATE_TRUSTED) {
        return;
    }
    level = cloudReadByte();
    string = cloudReadString();
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
void cloudGetIP(char *remoteip, int *remoteport, int *localport) {
    char addr[INET6_ADDRSTRLEN];

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

    Q_snprintf(remoteip, sizeof(remoteip), "[%s]", addr);
    *localport = (int)((struct sockaddr_in *) cloud.addr->ai_addr)->sin_port;
    remoteport = &cloud_config.port;
}

/**
 * Main command runner for "sv !cloud <cmd>" server command
 */
void cloudRun(int startarg, edict_t *ent, int client) {
    char *command;
    bool connected;
    char connected_time[25];
    char connected_ip[INET6_ADDRSTRLEN];
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
            cloudGetIP(connected_ip, &remote_port, &local_port);
            gi.cprintf(ent, PRINT_HIGH, "%-20s%s\n", "connected to:", va("%s:%d", connected_ip, cloud_config.port));
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

    if (Q_stricmp(command, "reconnect") == 0) {
        cloudDisconnect();
        q2a_memset(&cloud, 0, sizeof(cloud_t));
        cloudPrintf("disconnected\n");
        cloudInit();
        return;
    }

    if (Q_stricmp(command, "disconnect") == 0) {
        cloudDisconnect();
        q2a_memset(&cloud, 0, sizeof(cloud_t));
        cloudPrintf("disconnected\n");
        return;
    }

    if (Q_stricmp(command, "connect") == 0) {
        cloudInit();
        return;
    }
}

/**
 * Printf something to the server console prepended with [cloud]
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
 * Debug printing to the server console/log. Only outputs
 * if the debug flag is set
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
 * A player issued the teleport command. The gameserver will send the request
 * to the connected CloudAdmin server to find either the current list of
 * available servers or the address of the server requested.
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
 *
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
