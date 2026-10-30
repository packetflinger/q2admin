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
        c->send_key[i] = (byte) (i * 7 + 1);
    }
    q2a_memcpy(c->recv_key, c->send_key, AESKEY_LEN);
    c->encrypted = true;
    c->have_keys = true;
    c->e_ctx = EVP_CIPHER_CTX_new();
    c->d_ctx = EVP_CIPHER_CTX_new();
}

static void stop_encryption(void) {
    EVP_CIPHER_CTX_free(cloud.connection.e_ctx);
    EVP_CIPHER_CTX_free(cloud.connection.d_ctx);
}

/**
 * Seals payload the way the backend does, frame number count under key,
 * and frames it into out. Returns the frame's total size.
 */
static size_t backend_frame(byte *out, const byte *key, uint64_t count, const void *payload, size_t len) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    size_t sealed = 0;

    CHECK(cryptoGCMSeal(ctx, key, CLOUD_DIR_TO_FRONTEND, count, out + 4, &sealed, payload, len));
    EVP_CIPHER_CTX_free(ctx);
    out[0] = sealed & 0xff;
    out[1] = (sealed >> 8) & 0xff;
    out[2] = (sealed >> 16) & 0xff;
    out[3] = (sealed >> 24) & 0xff;
    return sealed + 4;
}

/**
 * Opens a frame q2admin sent, as the backend would, frame number count
 * under key. Returns true if it opened; the plaintext goes in out.
 */
static bool backend_open(byte *out, size_t *out_len, const byte *key, uint64_t count, const byte *frame, size_t len) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    bool ok = cryptoGCMOpen(ctx, key, CLOUD_DIR_TO_BACKEND, count, out, out_len, frame + 4, len - 4);
    EVP_CIPHER_CTX_free(ctx);
    return ok;
}

/**
 * The same key, direction, count and plaintext as the backend's
 * TestStreamVectors (crypto/crypto_test.go), which pins its output to
 * these bytes too. If both pass, the two ends seal frames identically.
 */
static void test_gcm_vectors(void) {
    static const byte to_backend[] = {
        0x0e, 0x50, 0xb9, 0xc6, 0xd4, 0x47, 0x63, 0x97, 0x35, 0xbc, 0xed,
        0xdc, 0xe5, 0x4f, 0x8f, 0x92, 0xdc, 0xb6, 0x78, 0xcf, 0xa4,
    };
    static const byte to_frontend[] = {
        0x16, 0x21, 0x99, 0xa0, 0x16, 0x5b, 0x30, 0xe8, 0x2d, 0x28, 0x7f,
        0x8f, 0x55, 0xbe, 0x06, 0x7d, 0xa6, 0xc6, 0x97, 0xf5, 0x9d,
    };
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    byte key[AESKEY_LEN], out[64];
    size_t len = 0;

    for (int i = 0; i < AESKEY_LEN; i++) {
        key[i] = (byte) (i * 7 + 1);
    }

    SECTION("GCM frames match the backend's, both directions");
    CHECK(cryptoGCMSeal(ctx, key, CLOUD_DIR_TO_BACKEND, 5, out, &len, (const byte *) "q2dm1", 5));
    CHECK_INT(len, sizeof(to_backend));
    CHECK(memcmp(out, to_backend, sizeof(to_backend)) == 0);
    CHECK(cryptoGCMSeal(ctx, key, CLOUD_DIR_TO_FRONTEND, 5, out, &len, (const byte *) "q2dm1", 5));
    CHECK_INT(len, sizeof(to_frontend));
    CHECK(memcmp(out, to_frontend, sizeof(to_frontend)) == 0);

    SECTION("a frame from the backend opens");
    CHECK(cryptoGCMOpen(ctx, key, CLOUD_DIR_TO_FRONTEND, 5, out, &len, to_frontend, sizeof(to_frontend)));
    CHECK_INT(len, 5);
    CHECK(memcmp(out, "q2dm1", 5) == 0);

    SECTION("but not with the wrong count, direction or a changed byte");
    CHECK(!cryptoGCMOpen(ctx, key, CLOUD_DIR_TO_FRONTEND, 4, out, &len, to_frontend, sizeof(to_frontend)));
    CHECK(!cryptoGCMOpen(ctx, key, CLOUD_DIR_TO_BACKEND, 5, out, &len, to_frontend, sizeof(to_frontend)));
    q2a_memcpy(out, to_frontend, sizeof(to_frontend));
    out[0] ^= 1;
    CHECK(!cryptoGCMOpen(ctx, key, CLOUD_DIR_TO_FRONTEND, 5, out + 32, &len, out, sizeof(to_frontend)));
    CHECK(!cryptoGCMOpen(ctx, key, CLOUD_DIR_TO_FRONTEND, 5, out, &len, to_frontend, GCM_TAG_LEN - 1));

    EVP_CIPHER_CTX_free(ctx);
}

static void test_encrypted_frames(void) {
    byte sent[256], frame[256], plain[256];
    size_t n, len, plain_len;
    const byte *key;

    connect_pair(CLOUD_STATE_TRUSTED);
    use_encryption();
    key = cloud.connection.send_key;

    SECTION("an encrypted frame's length counts the ciphertext and tag");
    cloudWriteByte(CMD_PING);
    cloudSendMessages();
    n = backend_recv(sent, sizeof(sent));
    len = sent[0] | (sent[1] << 8) | (sent[2] << 16) | (sent[3] << 24);
    CHECK_INT(n, 4 + 1 + GCM_TAG_LEN);
    CHECK_INT(len, 1 + GCM_TAG_LEN);

    SECTION("the backend can open each frame, in order");
    CHECK(backend_open(plain, &plain_len, key, 0, sent, n));
    CHECK_INT(plain_len, 1);
    CHECK_INT(plain[0], CMD_PING);
    cloudWriteByte(CMD_PING);
    cloudSendMessages();
    n = backend_recv(sent, sizeof(sent));
    CHECK(!backend_open(plain, &plain_len, key, 0, sent, n));
    CHECK(backend_open(plain, &plain_len, key, 1, sent, n));

    // Split it mid-ciphertext: before framing, each half would have been
    // decrypted on its own.
    SECTION("a frame from the backend split across reads opens once whole");
    n = backend_frame(frame, key, 0, (byte[]) {SCMD_PONG}, 1);
    cloud.ping.waiting = true;
    backend_send(frame, 4 + 5);
    cloudReadMessages();
    CHECK(cloud.ping.waiting);
    backend_send(frame + 9, n - 9);
    cloudReadMessages();
    CHECK(!cloud.ping.waiting);
    CHECK_INT(cloud.state, CLOUD_STATE_TRUSTED);
    CHECK_INT(cloud.connection.recv_count, 1);

    SECTION("a replayed frame drops the connection instead of being parsed");
    cloud.ping.waiting = true;
    backend_send(frame, n);
    cloudReadMessages();
    CHECK(cloud.ping.waiting);
    CHECK_INT(cloud.state, CLOUD_STATE_DISCONNECTED);

    stop_encryption();
    disconnect_pair();
}

/**
 * The race that garbled map names under CBC: the backend sends a new key
 * while our frames under the old one are on their way. Each direction has
 * to switch exactly where the other end does.
 */
static void test_key_rotation(void) {
    byte old_key[AESKEY_LEN], new_key[AESKEY_LEN];
    byte msg[1 + AESKEY_LEN];
    byte frame[256], sent[256], plain[256];
    size_t n, plain_len;

    connect_pair(CLOUD_STATE_TRUSTED);
    use_encryption();
    q2a_memcpy(old_key, cloud.connection.send_key, AESKEY_LEN);
    for (int i = 0; i < AESKEY_LEN; i++) {
        new_key[i] = (byte) (0xa0 + i);
    }

    // q2admin has queued something under the old key when SCMD_KEY arrives
    cloudWriteByte(CMD_PING);
    msg[0] = SCMD_KEY;
    q2a_memcpy(msg + 1, new_key, AESKEY_LEN);
    n = backend_frame(frame, old_key, 0, msg, sizeof(msg));
    backend_send(frame, n);
    cloudReadMessages();
    CHECK_INT(cloud.state, CLOUD_STATE_TRUSTED);

    SECTION("frames from the backend after SCMD_KEY open with the new key");
    cloud.ping.waiting = true;
    n = backend_frame(frame, new_key, 0, (byte[]) {SCMD_PONG}, 1);
    backend_send(frame, n);
    cloudReadMessages();
    CHECK(!cloud.ping.waiting);
    CHECK_INT(cloud.state, CLOUD_STATE_TRUSTED);

    SECTION("the ack goes in the last frame under the old key");
    cloudSendMessages();
    n = backend_recv(sent, sizeof(sent));
    CHECK(backend_open(plain, &plain_len, old_key, 0, sent, n));
    CHECK_INT(plain_len, 2);
    CHECK_INT(plain[0], CMD_PING);
    CHECK_INT(plain[1], CMD_KEYACK);

    SECTION("and the frames after it use the new key, counting from zero");
    cloudWriteByte(CMD_PING);
    cloudSendMessages();
    n = backend_recv(sent, sizeof(sent));
    CHECK(backend_open(plain, &plain_len, new_key, 0, sent, n));
    CHECK(!cloud.connection.send_key_pending);

    SECTION("the ack isn't lost when the queue is full");
    msg[0] = SCMD_KEY;
    for (int i = 0; i < AESKEY_LEN; i++) {
        msg[1 + i] = (byte) (0x50 + i);
    }
    while (cloud.queue_out.length < QUEUE_WRITE_MAX) {
        cloudWriteByte(CMD_PING);
    }
    n = backend_frame(frame, new_key, 1, msg, sizeof(msg));
    backend_send(frame, n);
    cloudReadMessages();
    CHECK_INT(cloud.queue_out.length, QUEUE_WRITE_MAX + 1);
    CHECK_INT(cloud.queue_out.data[QUEUE_WRITE_MAX], CMD_KEYACK);

    SECTION("a second SCMD_KEY before ours is acked drops the connection");
    n = backend_frame(frame, msg + 1, 0, msg, sizeof(msg));
    backend_send(frame, n);
    cloudReadMessages();
    CHECK_INT(cloud.state, CLOUD_STATE_DISCONNECTED);

    stop_encryption();
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
    test_gcm_vectors();
    test_encrypted_frames();
    test_key_rotation();
    test_partial_send();

    return test_summary();
}

#else

int main(void) {
    printf("socket pair tests skipped on Windows\n");
    return 0;
}

#endif
