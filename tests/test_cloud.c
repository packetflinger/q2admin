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

/**
 * Tests the Cloud Admin wire framing (see CLOUD_FRAME_HEADER in
 * g_cloud.h) with the real send and receive code. The connection's socket
 * is one end of a socket pair and the test plays the backend on the other
 * end, so no network or backend is needed.
 */

#include "g_local.h"
#include "q2a_test.h"
#include "stub_engine.h"

#ifndef _WIN32
#include <sys/socket.h>
#include <fcntl.h>
#include <signal.h>

// Defined in g_init.c and g_cmd.c, which the tests don't link.
cloud_config_t cloud_config;
cvar_t *configfile_cloud;
float frametime = 0.1;
int hz = 10;

bool readCfgFile(char *cfgfilename) {
    (void) cfgfilename;
    return false;
}

static int backend;     // the test's end of the socket pair

/**
 * Starts each test with a fresh connection in the given state, unencrypted.
 */
static void connect_pair(cloud_state_t state) {
    int fds[2];

    q2a_memset(&cloud, 0, sizeof(cloud));
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
        CHECK(!"socketpair failed");
        exit(1);
    }
    // non-blocking, as cloudConnect() makes the real socket
    fcntl(fds[0], F_SETFL, O_NONBLOCK);
    cloud.connection.socket = fds[0];
    backend = fds[1];
    cloud.state = state;
    cloud.maxclients = 8;
}

static void disconnect_pair(void) {
    if (cloud.state >= CLOUD_STATE_CONNECTING) {
        close(cloud.connection.socket);
    }
    close(backend);
}

/**
 * Writes len bytes to the connection as the backend would.
 */
static void backend_send(const void *data, size_t len) {
    if (write(backend, data, len) != (ssize_t) len) {
        CHECK(!"write to the socket pair failed");
    }
}

/**
 * Reads whatever the connection has sent so far, as the backend would.
 * Returns how many bytes.
 */
static size_t backend_recv(byte *out, size_t size) {
    ssize_t n = recv(backend, out, size, MSG_DONTWAIT);
    return n < 0 ? 0 : (size_t) n;
}

/**
 * Builds a frame: the 4-byte little-endian length, then the payload.
 * Returns the frame's total size.
 */
static size_t make_frame(byte *out, const void *payload, size_t len) {
    out[0] = len & 0xff;
    out[1] = (len >> 8) & 0xff;
    out[2] = (len >> 16) & 0xff;
    out[3] = (len >> 24) & 0xff;
    q2a_memcpy(out + 4, payload, len);
    return len + 4;
}

static void test_send_frames(void) {
    byte got[256];
    size_t n;

    connect_pair(CLOUD_STATE_CONNECTED);

    SECTION("a send is one frame, its length in front");
    cloudWriteByte(CMD_PING);
    cloudWriteString("hi");
    cloudSendMessages();
    n = backend_recv(got, sizeof(got));
    CHECK_INT(n, 8);
    CHECK_INT(got[0] | (got[1] << 8) | (got[2] << 16) | (got[3] << 24), 4);
    CHECK_INT(got[4], CMD_PING);
    CHECK(memcmp(got + 5, "hi", 3) == 0);
    CHECK_INT(cloud.queue_out.length, 0);
    CHECK_INT(cloud.queue_out.framed, 0);

    SECTION("nothing queued sends nothing");
    cloudSendMessages();
    CHECK_INT(backend_recv(got, sizeof(got)), 0);

    disconnect_pair();
}

static void test_receive_frames(void) {
    byte pong[1] = { SCMD_PONG };
    byte frame[64];
    byte two[64];
    size_t len, len2;

    connect_pair(CLOUD_STATE_CONNECTED);
    len = make_frame(frame, pong, sizeof(pong));

    SECTION("a frame split across reads waits for the rest");
    cloud.ping.waiting = true;
    cloud.ping.miss_count = 2;
    backend_send(frame, 3);
    cloudReadMessages();
    CHECK(cloud.ping.waiting);
    CHECK_INT(cloud.queue_recv.length, 3);
    backend_send(frame + 3, len - 3);
    cloudReadMessages();
    CHECK(!cloud.ping.waiting);
    CHECK_INT(cloud.ping.miss_count, 0);
    CHECK_INT(cloud.queue_recv.length, 0);

    SECTION("two frames in one read are both handled");
    len2 = make_frame(two, pong, sizeof(pong));
    len2 += make_frame(two + len2, pong, sizeof(pong));
    cloud.ping.waiting = true;
    backend_send(two, len2);
    cloudReadMessages();
    CHECK(!cloud.ping.waiting);
    CHECK_INT(cloud.queue_recv.length, 0);

    SECTION("an empty frame is ignored");
    len = make_frame(frame, NULL, 0);
    backend_send(frame, len);
    cloudReadMessages();
    CHECK_INT(cloud.state, CLOUD_STATE_CONNECTED);
    CHECK_INT(cloud.queue_recv.length, 0);

    SECTION("a frame too long to be real drops the connection");
    frame[0] = 0xff;
    frame[1] = 0xff;
    frame[2] = 0xff;
    frame[3] = 0x7f;
    backend_send(frame, 4);
    cloudReadMessages();
    CHECK_INT(cloud.state, CLOUD_STATE_DISCONNECTED);

    disconnect_pair();
}

/**
 * Sets up the session key the handshake would have given, so frames are
 * encrypted.
 */
static void use_encryption(void) {
    cloud_connection_t *c = &cloud.connection;

    for (int i = 0; i < AESKEY_LEN; i++) {
        c->session_key[i] = (byte) (i * 7 + 1);
    }
    for (int i = 0; i < AES_IV_LEN; i++) {
        c->initial_value[i] = (byte) (i * 13 + 5);
    }
    c->encrypted = true;
    c->have_keys = true;
    c->e_ctx = EVP_CIPHER_CTX_new();
    c->d_ctx = EVP_CIPHER_CTX_new();
}

static void test_encrypted_frames(void) {
    byte sent[256];
    size_t n, len;

    connect_pair(CLOUD_STATE_TRUSTED);
    use_encryption();

    SECTION("an encrypted frame's length counts the ciphertext");
    cloudWriteByte(SCMD_PONG);
    cloudSendMessages();
    n = backend_recv(sent, sizeof(sent));
    len = sent[0] | (sent[1] << 8) | (sent[2] << 16) | (sent[3] << 24);
    CHECK_INT(n, 4 + AESBLOCK_LEN);
    CHECK_INT(len, AESBLOCK_LEN);
    CHECK(sent[4] != SCMD_PONG || sent[5] != 0);

    // The payload is a valid encrypted SCMD_PONG, so sending the frame back
    // shows the receiving side decrypts it. Split it mid-ciphertext: before
    // framing, each half would have been decrypted on its own.
    SECTION("an encrypted frame split across reads decrypts once whole");
    cloud.ping.waiting = true;
    backend_send(sent, 4 + 5);
    cloudReadMessages();
    CHECK(cloud.ping.waiting);
    backend_send(sent + 9, n - 9);
    cloudReadMessages();
    CHECK(!cloud.ping.waiting);
    CHECK_INT(cloud.state, CLOUD_STATE_TRUSTED);

    EVP_CIPHER_CTX_free(cloud.connection.e_ctx);
    EVP_CIPHER_CTX_free(cloud.connection.d_ctx);
    disconnect_pair();
}

static void test_partial_send(void) {
    static byte got[QUEUE_SIZE * 2];
    size_t total = 0, n;
    int size = 4096;
    int rounds = 0;
    size_t want;

    connect_pair(CLOUD_STATE_CONNECTED);

    // A small send buffer makes send() take only part of a big frame.
    setsockopt(cloud.connection.socket, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));

    SECTION("a frame sent in parts arrives whole, framed once");
    for (int i = 0; i < 1500; i++) {
        cloudWriteString("message %04d", i);
    }
    want = cloud.queue_out.length;
    while ((cloud.queue_out.length || cloud.queue_out.framed) && rounds < 1000) {
        cloudSendMessages();
        while ((n = backend_recv(got + total, sizeof(got) - total)) > 0) {
            total += n;
        }
        rounds++;
    }
    CHECK(rounds > 1);
    CHECK_INT(total, 4 + want);
    CHECK_INT(got[0] | (got[1] << 8) | (got[2] << 16) | (got[3] << 24), want);
    CHECK(memcmp(got + 4, "message 0000", 13) == 0);
    CHECK(memcmp(got + 4 + want - 13, "message 1499", 13) == 0);

    disconnect_pair();
}

int main(void) {
    stub_engine_init();

    // fail rather than hang if a send or read ever blocks
    alarm(30);

    test_send_frames();
    test_receive_frames();
    test_encrypted_frames();
    test_partial_send();

    return test_summary();
}

#else

int main(void) {
    printf("socket pair tests skipped on Windows\n");
    return 0;
}

#endif
