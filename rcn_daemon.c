#include "include/rcn.h"
#include "include/rcn_daemon.h"
#include "include/rcn_device.h"
#include "include/rcn_stream.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <grp.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#define DEFAULT_USOCK_COUNT 3

ssize_t d_stream_or_close(struct d_context* d_ctx, struct epoll_stream* stream) {
    const int ret = stream_stream(stream);
    if (stream->next->state == STREAM_CLOSED) {
        CHECK(d_epoll_close_remove(d_ctx, stream) == -1);
    }
    CHECK(ret == -1);
    return 0;
err:
    ERR_LOG("d_read_or_close");
    return -1;
}

static int accept_isock(struct epoll_context* ep_ctx, const struct epoll_stream* stream, int* peer_fd) {
    struct sockaddr_in p_iaddr = {};
    struct sockaddr* addr = (struct sockaddr*)&p_iaddr;
    socklen_t addr_len = sizeof(p_iaddr);
    int sock_fd = TRY(accept(stream->fd, addr, &addr_len), -1);
    CHECK(fcntl(sock_fd, F_SETFL, O_NONBLOCK) == -1);
    CHECK(d_epoll_add(ep_ctx, sock_fd, FD_PEER) == -1);
    *peer_fd = sock_fd;
    return 0;
err:
    ERR_LOG("accept_isock");
    return -1;
}

int d_init_peer_ctx(struct epoll_context* ep_ctx, struct peer_context* p_ctx, int isock_fd, enum daemon_type d_type) {
    p_ctx->isock_fd = isock_fd;
    p_ctx->peer_state = PERR_CONN_DISCONNECTED;
    p_ctx->expected_msg = PEER_HEADER_IDLE;
    if (d_type == DAEMON_SERVER) {
        CHECK(d_epoll_add_getr(ep_ctx, isock_fd, FD_ISOCK, &p_ctx->stream) == -1);
        CHECK(stream_set_default(p_ctx->stream, STREAM_READING) == -1);
    } else if (d_type == DAEMON_CLIENT) {
        CHECK(d_epoll_add_getr(ep_ctx, isock_fd, FD_PEER, &p_ctx->stream) == -1);
    }
    return 0;
err:
    ERR_LOG("d_init_peer_ctx");
    return -1;
}

static int accept_usock(struct epoll_context* ep_ctx, epoll_stream_arr* relay_streams, const struct epoll_stream* stream) {
    struct sockaddr_un r_uaddr = {};
    struct sockaddr* addr = (struct sockaddr*)&r_uaddr;
    socklen_t addr_len = sizeof(r_uaddr);
    int sock_fd = TRY(accept(stream->fd, addr, &addr_len), -1);
    CHECK(fcntl(sock_fd, F_SETFL, O_NONBLOCK) == -1);
    struct epoll_stream* relay_stream;
    CHECK(d_epoll_add_getr(ep_ctx, sock_fd, FD_RELAY, &relay_stream) == -1);
    CHECK(u_array_add(&relay_streams->r, &relay_stream) == -1);
    return 0;
err:
    ERR_LOG("accept_usock");
    return -1;
}

int d_init_relay_ctx(struct epoll_context* ep_ctx, struct relay_context* r_ctx, int usock_fd) {
    r_ctx->usock_fd = usock_fd;
    CHECK(u_array_init(&r_ctx->relay_streams.r, sizeof(struct relay*), RCN_STD_CAPACITY) == -1);
    CHECK(d_epoll_add(ep_ctx, usock_fd, FD_USOCK) == -1);
    return 0;
err:
    ERR_LOG("d_init_relay_ctx");
    return -1;
}

int d_broadcast_relay_header(struct epoll_context* ep_ctx, epoll_stream_arr* relay_streams, enum relay_msg_header header) {
    for (size_t i = 0; i < relay_streams->r.length; i++) {
        struct epoll_stream* relay_stream = {};
        CHECK(u_array_getv(&relay_streams->r, &relay_stream, i) == -1);
        CHECK(stream_queue_writing(ep_ctx, relay_stream, header, 0, NULL) == -1);
    }
    return 0;
err:
    ERR_LOG("d_broadcast_relay_header");
    return -1;
}

int d_init_device_ctx(struct device_context* d_ctx) {
    CHECK(u_array_init(&d_ctx->devices.r, sizeof(struct device), RCN_STD_CAPACITY) == -1);
    return 0;
err:
    ERR_LOG("d_init_device_ctx");
    return -1;
}

int d_print_log(enum daemon_type d_type) {
    char* log = NULL;
    if (d_type == DAEMON_SERVER)
        log = RCN_SERVER_LOG_PATH;
    else if (d_type == DAEMON_CLIENT)
        log = RCN_CLIENT_LOG_PATH;
    FILE* log_file = TRY(fopen(log, "r"), NULL);
    char buffer[256] = { 0 };
    while (fread(buffer, sizeof(char), sizeof(buffer), log_file) > 0)
        printf("%s", buffer);
    CHECK(ferror(log_file) > 0);
    fclose(log_file);
    return 0;
err:
    ERR_LOG("d_print_log");
    return -1;
}

int d_init_dir() {
    const mode_t mask = umask(0);
    const int rcn_dir = mkdir(RCN_DAEMON_DIR_PATH, 0);
    CHECK(rcn_dir == -1 && errno != EEXIST);
    const struct group* grp = TRY(getgrnam(RCN_GROUP), NULL);
    CHECK(chown(RCN_DAEMON_DIR_PATH, -1, grp->gr_gid) == -1);
    CHECK(chmod(RCN_DAEMON_DIR_PATH, 0770) == -1);
    umask(mask);
    return 0;
err:
    ERR_LOG("d_init_dir");
    return -1;
}

int d_init_log(enum daemon_type d_type) {
    char* log_path = NULL;
    if (d_type == DAEMON_SERVER)
        log_path = RCN_SERVER_LOG_PATH;
    else if (d_type == DAEMON_CLIENT)
        log_path = RCN_CLIENT_LOG_PATH;
    CHECK(log_path == NULL);
    CHECK(freopen(log_path, "w", stdout) == NULL);
    CHECK(freopen(log_path, "a", stderr) == NULL);
    CHECK(setvbuf(stdout, NULL, _IONBF, 0) != 0);
    CHECK(setvbuf(stderr, NULL, _IONBF, 0) != 0);
    return 0;
err:
    ERR_LOG("d_init_log");
    return -1;
}

int d_init_epoll_ctx(struct epoll_context* ep_ctx) {
    ep_ctx->epoll_fd = TRY(epoll_create1(0), -1);
    CHECK(u_array_init(&ep_ctx->stream_ptrs.r, sizeof(struct epoll_stream*), RCN_STD_CAPACITY) == -1);
    return 0;
err:
    ERR_LOG("d_init_epoll");
    return -1;
}

int d_epoll_sync_stream(struct epoll_context* ep_ctx, struct epoll_stream* stream) {
    enum EPOLL_EVENTS events = stream->next->op == STREAM_WRITING ? EPOLLOUT : EPOLLIN;
    struct epoll_event evt = {
        .data.ptr = stream,
        .events = events,
    };
    CHECK(epoll_ctl(ep_ctx->epoll_fd, EPOLL_CTL_MOD, stream->fd, &evt) == -1);
    return 0;
err:
    ERR_LOG("d_epoll_entry_update");
    return -1;
}

static int epoll_counter_update(struct epoll_context* ep_ctx, enum fd_type type, int op) {
    CHECK(op == 0);
    if (op < 0)
        op = -1;
    else if (op > 0)
        op = 1;
    int* counter = NULL;
    if (type == FD_RELAY)
        counter = &ep_ctx->relay_count;
    else if (type == FD_PEER)
        counter = &ep_ctx->peer_count;
    else
        return 0;
    *counter += op;
    if (*counter < 0) {
        fprintf(stderr, "err: epoll counter is negative\n");
        goto err;
    }
    return 0;
err:
    ERR_LOG("update_epoll_counter");
    return -1;
}

int d_epoll_add_getr(struct epoll_context* ep_ctx, int fd, enum fd_type type, struct epoll_stream** out_stream) {
    struct epoll_stream* stream= TRY(calloc(1, sizeof(struct epoll_stream)), NULL);
    stream->fd_type= type;
    CHECK(stream_init(stream, fd, type) == -1);
    CHECK(u_array_add(&ep_ctx->stream_ptrs.r, &stream) == -1);
    struct epoll_event u_evt = { .events = EPOLLIN, .data.ptr = stream, };
    CHECK(epoll_ctl(ep_ctx->epoll_fd, EPOLL_CTL_ADD, fd, &u_evt) == -1);
    CHECK(epoll_counter_update(ep_ctx, type, 1) == -1);
    *out_stream = stream;
    return 0;
err:
    ERR_LOG("d_epoll_add");
    return -1;
}

int d_epoll_add(struct epoll_context* ep_ctx, int fd, enum fd_type type) {
    struct epoll_stream* tmp_stream;
    CHECK(d_epoll_add_getr(ep_ctx, fd, type, &tmp_stream) == -1);
    return 0;
err:
    ERR_LOG("d_epoll_add");
    return -1;
}

int d_epoll_add_device(struct epoll_context* ep_ctx, int fd, struct device* device, enum fd_type type) {
    struct epoll_stream* stream = TRY(calloc(1, sizeof(struct epoll_stream)), NULL);
    CHECK(stream_init(stream, fd, type) == -1);
    CHECK(u_array_add(&ep_ctx->stream_ptrs.r, &stream) == -1);
    struct epoll_event u_evt = { .events = EPOLLIN, .data.ptr = stream, };
    CHECK(epoll_ctl(ep_ctx->epoll_fd, EPOLL_CTL_ADD, fd, &u_evt) == -1);
    CHECK(epoll_counter_update(ep_ctx, type, 1) == -1);
    stream->fd_type= type;
    device->stream = *stream;
    return 0;
err:
    ERR_LOG("d_epoll_add");
    return -1;
}

int d_epoll_close_remove(struct d_context* d_ctx, struct epoll_stream* stream) {
    switch (stream->fd_type) {
        case FD_RELAY: r_close_relay(d_ctx->relay_ctx, stream); break;
        case FD_DEV: e_close_dev(d_ctx->device_ctx, stream); break;
        case FD_PEER: p_close_peer_ctx(d_ctx->peer_ctx); break;
        default: ERR_GOTO(err, "err: unknown fd_type\n");
    }
    struct epoll_context* ep_ctx = d_ctx->ep_ctx;
    CHECK(epoll_ctl(ep_ctx->epoll_fd, EPOLL_CTL_DEL, stream->fd, NULL) == -1);
    size_t index = TRY(u_array_find_index(&ep_ctx->stream_ptrs.r, &stream), -1);
    CHECK(u_array_remove(&ep_ctx->stream_ptrs.r, index) == -1);
    CHECK(epoll_counter_update(ep_ctx, stream->fd_type, -1) == -1);
    CHECK(stream_close(stream) == -1);
    u_safe_free((void**)&stream);
    return 0;
err:
    close(stream->fd);
    ERR_LOG("d_epoll_close_remove");
    return -1;
}

static int epoll_reset_stream(struct epoll_context* ep_ctx, struct epoll_stream* stream) {
    CHECK(stream_clear_fallback(stream) == -1);
    CHECK(d_epoll_sync_stream(ep_ctx, stream) == -1);
    return 0;
err:
    ERR_LOG("epoll_reset_stream");
    return -1;
}

int d_init_usock(char* sock_path, size_t path_len) {
    unlink(sock_path);
    int d_usock_fd = TRY(socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0), -1);
    struct sockaddr_un d_uaddr = {
        .sun_family = AF_UNIX,
    };
    memcpy(d_uaddr.sun_path, sock_path, path_len);
    CHECK(bind(d_usock_fd, (struct sockaddr*)&d_uaddr, sizeof(d_uaddr)) == -1);
    CHECK(listen(d_usock_fd, DEFAULT_USOCK_COUNT) == -1);
    return d_usock_fd;
err:
    ERR_LOG("d_init_usock");
    return -1;
}

static int can_exit(struct d_context* h_ctx) {
    if (h_ctx->exit == false)
        return 0;
    if (h_ctx->ep_ctx->peer_count > 0)
        return 0;
    if (h_ctx->ep_ctx->relay_count > 0)
        return 0;
    return 1;
}

static int dispatch_epoll(struct d_context* d_ctx, struct epoll_event* epoll_buff, size_t fd_count) {
    struct epoll_context* ep_ctx = d_ctx->ep_ctx;
    int nfds = TRY(epoll_wait(ep_ctx->epoll_fd, epoll_buff, fd_count, -1), -1);
    for (int i = 0; i < nfds; i++) {
        struct epoll_event evt = epoll_buff[i];
        struct epoll_stream* stream = evt.data.ptr;
        switch (stream->fd_type) {
            case FD_USOCK: {
                CHECK(accept_usock(ep_ctx, &d_ctx->relay_ctx->relay_streams, stream) == -1);
                break;
            }
            case FD_ISOCK: {
                CHECK(accept_isock(ep_ctx, stream, &d_ctx->peer_ctx->isock_fd) == -1);
                break;
            }
            case FD_PEER: {
                CHECK(d_stream_or_close(d_ctx, stream) == -1);
                break;
            }
            case FD_RELAY: {
                CHECK(d_stream_or_close(d_ctx, stream) == -1);
                break;
            }
            case FD_DEV: {
                CHECK(d_stream_or_close(d_ctx, stream) == -1);
                break;
            }
            case FD_UDEV: {
                break;
            }
        }
    }
    return nfds;
err:
    ERR_LOG("dispatch_epoll");
    return -1;
}

static int resolve_fd_streams(struct d_context* d_ctx, struct epoll_handlers handlers, struct epoll_event* epoll_buff, int fd_count) {
    for (int i = 0; i < fd_count; i++) {
        struct epoll_event evt = epoll_buff[i];
        struct epoll_stream* stream = evt.data.ptr;
        struct stream_item* stream_item = stream->next;
        if (stream_item->state != STREAM_COMPLETE || stream_item->state == STREAM_CLOSED) {
            continue;
        }
        CHECK(stream_collect(stream, &stream_item) == -1);
        if (stream_item->op != stream->default_op) {
            CHECK(epoll_reset_stream(d_ctx->ep_ctx, stream) == -1);
            continue;
        }
        switch (stream->fd_type) {
            case FD_PEER: {
                CHECK(handlers.peer_handler(d_ctx, stream, stream_item) == -1);
                break;
            }
            case FD_RELAY: {
                CHECK(handlers.relay_handler(d_ctx, stream, stream_item) == -1);
                break;
            }
            case FD_DEV: {
                CHECK(handlers.device_handler(d_ctx, stream, stream_item) == -1);
                break;
            }
            case FD_ISOCK: return 0;
            case FD_USOCK: return 0;
            default: ERR_GOTO(err, "err: invalid entry-type\n");
        }
        CHECK(epoll_reset_stream(d_ctx->ep_ctx, stream) == -1);
    }
    return 0;
err:
    ERR_LOG("resolve_fd_context");
    return -1;
}

int d_loop(struct d_context* d_ctx, struct epoll_handlers handlers) {
    struct epoll_context* ep_ctx = d_ctx->ep_ctx;
    while (can_exit(d_ctx) == false) {
        const size_t fd_count = ep_ctx->stream_ptrs.r.length;
        struct epoll_event epoll_buff[fd_count];
        const int nfds = TRY(dispatch_epoll(d_ctx, epoll_buff, fd_count), -1);
        CHECK(resolve_fd_streams(d_ctx, handlers, epoll_buff, nfds) == -1);
    }
    return 0;
err:
    ERR_LOG("d_loop");
    return -1;
}
int run(struct daemon_arg* d_arg) {
    CHECK(setsid() == -1);
    CHECK(d_init_log(d_arg->d_type) == -1);
    CHECK(d_loop(&d_arg->d_ctx, d_arg->handlers) == -1);
    // TODO: REIMPLEMENT
    // CHECK(cleanup(d_arg) == -1);
    return 0;
err:
    ERR_LOG("d_run");
    return -1;
}

int d_fork(struct daemon_arg* d_arg, struct relay_arg r_arg) {
    const pid_t pid = TRY(fork(), -1);
    if (pid == 0) {
        CHECK(run(d_arg) == -1);
    } else {
        CHECK(r_trigger(r_arg) == -1);
    }
    return 0;
err:
    ERR_LOG("d_fork");
    return -1;
}