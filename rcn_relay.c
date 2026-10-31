#include "include/rcn.h"
#include "include/rcn_daemon.h"
#include "include/rcn_epoll.h"
#include "include/rcn_peer.h"
#include "include/rcn_relay.h"
#include "include/rcn_stream.h"
#include "include/rcn_types.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/un.h>
#include <signal.h>
#include <unistd.h>
#include <sys/poll.h>
#include <sys/prctl.h>

struct sock_info {
    char *sock_path;
    size_t path_len;
};

static int init_sockinfo(enum daemon_type d_type, struct sock_info *info) {
    if (d_type == DAEMON_SERVER) {
        info->sock_path = RCN_SERVER_USOCKET_PATH;
        info->path_len = RCN_SERVER_USOCKET_LEN;
    } else if (d_type == DAEMON_CLIENT) {
        info->sock_path = RCN_CLIENT_USOCKET_PATH;
        info->path_len = RCN_CLIENT_USOCKET_LEN;
    } else {
        goto err;
    }
    return 0;
    err:
        ERR_LOG("");
    return -1;
}

int r_init_usock(char* sock_path, size_t path_len) {
    unlink(sock_path);
    int d_usock_fd = TRY(socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0), -1);
    struct sockaddr_un d_uaddr = {
        .sun_family = AF_UNIX,
    };
    memcpy(d_uaddr.sun_path, sock_path, path_len);
    CHECK(bind(d_usock_fd, (struct sockaddr*)&d_uaddr, sizeof(d_uaddr)) == -1);
    CHECK(listen(d_usock_fd, DEFAULT_USOCK_COUNT) == -1);
    LOG("created unix socket");
    return d_usock_fd;
err:
    ERR_LOG("");
    return -1;
}

int r_connect_usock(char *sock_path, size_t path_len) {
    int usock_fd = TRY(socket(AF_UNIX, SOCK_STREAM, 0), -1);
    struct sockaddr_un r_uaddr = {};
    r_uaddr.sun_family = AF_UNIX;
    memcpy(r_uaddr.sun_path, sock_path, path_len);
    CHECK(connect(usock_fd, (struct sockaddr*)&r_uaddr, sizeof(r_uaddr)) == -1);
    return usock_fd;
err:
    ERR_LOG("");
    return -1;
}

int r_broadcast_relay_header(struct epoll_context* ep_ctx, epoll_stream_arr* relay_streams, enum relay_msg_header header) {
    for (size_t i = 0; i < relay_streams->r.length; i++) {
        struct epoll_stream* relay_stream = {};
        CHECK(u_array_getv(&relay_streams->r, &relay_stream, i) == -1);
        CHECK(stream_queue_writing_socket(ep_ctx, relay_stream, header, 0, NULL) == -1);
    }
    return 0;
err:
    ERR_LOG("");
    return -1;
}

int r_close_relay(struct relay_context* r_ctx, struct epoll_stream* stream) {
    size_t index = TRY(u_array_find_index(&r_ctx->relay_streams.r, &stream), -1);
    CHECK(u_array_remove(&r_ctx->relay_streams.r, index) == -1);
    return 0;
err:
    ERR_LOG("");
    return -1;
}

int r_init_relay_ctx(struct epoll_context* ep_ctx, struct relay_context* r_ctx, int usock_fd) {
    r_ctx->usock_fd = usock_fd;
    CHECK(u_array_init(&r_ctx->relay_streams.r, sizeof(struct relay*), RCN_STD_CAPACITY) == -1);
    CHECK(e_epoll_add(ep_ctx, usock_fd, FD_USOCK) == -1);
    return 0;
err:
    ERR_LOG("");
    return -1;
}

int r_close_relay_ctx(struct epoll_context* ep_ctx, struct relay_context* r_ctx) {
    // do not close usock_fd, its closed in close_epoll_ctx
    for (size_t i = 0; i < r_ctx->relay_streams.r.length; i++) {
        struct epoll_stream* r_stream = {};
        CHECK(u_array_getr(&r_ctx->relay_streams.r, (void**)&r_stream, i) == -1);
        CHECK(e_epoll_close_remove_simple(ep_ctx, r_stream) == -1);
    }
    CHECK(u_array_free(&r_ctx->relay_streams.r) == -1);
    return 0;
err:
    ERR_LOG("");
    return -1;
}

static int check_daemon_status(int eventfd) {
    printf("...");
    fflush(stdout);
    int64_t status = 0;
    const int flags = TRY(fcntl(eventfd, F_GETFL), -1);
    CHECK(fcntl(eventfd, F_SETFL, flags | O_NONBLOCK) == -1);
    struct pollfd fds = {};
    fds.fd = eventfd;
    fds.events = POLLIN;
    fflush(stdout);
    CHECK(poll(&fds, 1, 5000) == -1);
    read(eventfd, &status, sizeof(status));
    if (status != DAEMON_STATUS_OK)
        goto daemon_err;
    return 0;
    daemon_err:
        if (status == 0)
            status = ETIMEDOUT; // a poll-timeout results in status=SUCCESS, so convert to a proper ETIMEDOUT error
    fprintf(stderr, "\rerr: cannot start daemon because [%s]\n", strerror(status));
    fflush(stderr);
    return -1;
    err:
        ERR_LOG("");
    return -1;
}

int relay_start(struct epoll_stream* relay_stream, struct relay_arg r_arg) {
    signal(SIGINT, r_arg.sighandler);
    if (r_arg.check_daemon == true)
        CHECK(check_daemon_status(r_arg.evt_fd) == -1);
    CHECK(prctl(PR_SET_NAME, RCN_PROC_NAME_RELAY, 0UL, 0UL, 0UL) == -1);
    struct sock_info info = {};
    CHECK(init_sockinfo(r_arg.d_type, &info) == -1);
    int usock_fd = TRY(r_connect_usock(info.sock_path, info.path_len), -1);
    CHECK(stream_init(relay_stream, usock_fd, FD_RELAY) == -1);
    CHECK(fcntl(usock_fd, F_SETFL, O_NONBLOCK) == -1);
    return 0;
err:
    ERR_LOG("");
    return -1;
}