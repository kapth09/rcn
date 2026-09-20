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

ssize_t d_stream_or_close(struct epoll_context* ep_ctx, struct epoll_entry* entry) {
    const int ret = stream(&entry->stream);
    if (entry->stream.state == STREAM_CLOSED)
        CHECK(d_epoll_close_remove(ep_ctx, entry) == -1);
    CHECK(ret == -1);
    return 0;
err:
    ERR_LOG("d_read_or_close");
    return -1;
}

static int accept_isock(struct epoll_context* ep_ctx, const struct stream* stream, int* peer_fd) {
    struct sockaddr_in p_iaddr = {};
    struct sockaddr* addr = (struct sockaddr*)&p_iaddr;
    socklen_t addr_len = sizeof(p_iaddr);
    int sock_fd = TRY(accept(stream->fd, addr, &addr_len), -1);
    CHECK(fcntl(sock_fd, F_SETFL, O_NONBLOCK) == -1);
    CHECK(d_epoll_add(ep_ctx, sock_fd, FD_PEER) == -1);
    *peer_fd = sock_fd;
err:
    ERR_LOG("accept_isock");
    return -1;
}

int d_init_peer_ctx(struct epoll_context* ep_ctx, struct peer_context* p_ctx, int isock_fd) {
    p_ctx->isock_fd = isock_fd;
    p_ctx->peer_state = PERR_CONN_DISCONNECTED;
    p_ctx->expected_msg = PEER_HEADER_IDLE;
    CHECK(stream_set_default(&p_ctx->stream, sizeof(enum peer_msg_header), STREAM_READING) == -1);
    CHECK(d_epoll_add(ep_ctx, isock_fd, FD_ISOCK) == -1);
    return 0;
err:
    ERR_LOG("d_init_peer_ctx");
    return -1;
}

static int accept_usock(struct epoll_context* ep_ctx, const struct stream* stream) {
    struct sockaddr_un r_uaddr = {};
    struct sockaddr* addr = (struct sockaddr*)&r_uaddr;
    socklen_t addr_len = sizeof(r_uaddr);
    int sock_fd = TRY(accept(stream->fd, addr, &addr_len), -1);
    CHECK(fcntl(sock_fd, F_SETFL, O_NONBLOCK) == -1);
    CHECK(d_epoll_add(ep_ctx, sock_fd, FD_RELAY) == -1);
    return 0;
err:
    ERR_LOG("accept_usock");
    return -1;
}

int d_init_relay_ctx(struct epoll_context* ep_ctx, struct relay_context* r_ctx, int usock_fd) {
    r_ctx->usock_fd = usock_fd;
    CHECK(u_array_init(&r_ctx->relays.r, sizeof(struct relay), RCN_STD_CAPACITY) == -1);
    CHECK(d_epoll_add(ep_ctx, usock_fd, FD_USOCK) == -1);
    return 0;
err:
    ERR_LOG("d_init_relay_ctx");
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
    CHECK(freopen(log_path, "w", stderr) == NULL);
    CHECK(setvbuf(stdout, NULL, _IONBF, 0) != 0);
    CHECK(setvbuf(stderr, NULL, _IONBF, 0) != 0);
    return 0;
err:
    ERR_LOG("d_init_log");
    return -1;
}

int d_init_epoll_ctx(struct epoll_context* ep_ctx) {
    ep_ctx->epoll_fd = TRY(epoll_create1(0), -1);
    CHECK(u_array_init(&ep_ctx->entries.r, sizeof(struct epoll_entry*), RCN_STD_CAPACITY) == -1);
    return 0;
err:
    ERR_LOG("d_init_epoll");
    return -1;
}

int d_epoll_entry_sync_stream(struct epoll_context* ep_ctx, struct epoll_entry* entry, struct stream* stream) {
    enum EPOLL_EVENTS events = stream->op == STREAM_WRITING ? EPOLLOUT : EPOLLIN;
    struct epoll_event evt = {
        .data.ptr = entry,
        .events = events,
    };
    CHECK(epoll_ctl(ep_ctx->epoll_fd, EPOLL_CTL_MOD, entry->stream.fd, &evt) == -1);
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

int d_epoll_add(struct epoll_context* ep_ctx, int fd, enum fd_type type) {
    struct epoll_entry* entry = TRY(calloc(1, sizeof(struct epoll_entry)), NULL);
    entry->type = type;
    CHECK(stream_setup(&entry->stream, fd, type) == -1);
    CHECK(u_array_add(&ep_ctx->entries.r, &entry) == -1);
    struct epoll_event u_evt = { .events = EPOLLIN, .data.ptr = entry, };
    CHECK(epoll_ctl(ep_ctx->epoll_fd, EPOLL_CTL_ADD, fd, &u_evt) == -1);
    CHECK(epoll_counter_update(ep_ctx, type, 1) == -1);
    return 0;
err:
    ERR_LOG("d_epoll_add");
    return -1;
}

int d_epoll_close_remove(struct epoll_context* ep_ctx, struct epoll_entry* entry) {
    CHECK(epoll_ctl(ep_ctx->epoll_fd, EPOLL_CTL_DEL, entry->stream.fd, NULL) == -1);
    size_t index = TRY(u_array_find_index(&ep_ctx->entries.r, &entry), -1);
    CHECK(u_array_remove(&ep_ctx->entries.r, index) == -1);
    CHECK(epoll_counter_update(ep_ctx, entry->type, -1) == -1);
    close(entry->stream.fd);
    free(entry);
    return 0;
err:
    close(entry->stream.fd);
    ERR_LOG("d_epoll_remove");
    return -1;
}

int epoll_reset_stream(struct epoll_context* ep_ctx, struct epoll_entry* entry) {
    CHECK(d_epoll_entry_sync_stream(ep_ctx, entry, &entry->stream) == -1);
    CHECK(stream_clear(&entry->stream) == -1);
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

static int dispatch_epoll(struct epoll_context* ep_ctx, struct epoll_event* epoll_buff, size_t fd_count) {
    int nfds = TRY(epoll_wait(ep_ctx->epoll_fd, epoll_buff, fd_count, -1), -1);
    for (int i = 0; i < nfds; i++) {
        struct epoll_event evt = epoll_buff[i];
        struct epoll_entry* entry = evt.data.ptr;
        switch (entry->type) {
            case FD_USOCK: {
                CHECK(accept_usock(ep_ctx, &entry->stream) == -1);
                break;
            }
            case FD_ISOCK: {
                struct peer_context* p_ctx = (struct peer_context*)&entry->stream;
                CHECK(accept_isock(ep_ctx, &entry->stream, &p_ctx->isock_fd) == -1);
                break;
            }
            case FD_PEER: {
                CHECK(d_stream_or_close(ep_ctx, entry) == -1);
                break;
            }
            case FD_RELAY: {
                CHECK(d_stream_or_close(ep_ctx, entry) == -1);
                break;
            }
            case FD_DEV: {
                CHECK(d_stream_or_close(ep_ctx, entry) == -1);
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

static int resolve_fd_context(struct d_context* d_ctx, struct epoll_handlers handlers, struct epoll_event* epoll_buff, int fd_count) {
    for (int i = 0; i < fd_count; i++) {
        struct epoll_event evt = epoll_buff[i];
        struct epoll_entry* entry = evt.data.ptr;
        struct stream* stream = &entry->stream;
        if (stream->state != STREAM_COMPLETE) {
            continue;
        }
        if (stream->header == -1) {
            CHECK(stream_header(stream) == -1);
        }
        if (stream->op != stream->default_op) {
            CHECK(epoll_reset_stream(d_ctx->ep_ctx, entry) == -1);
            continue;
        }
        switch (entry->type) {
            case FD_PEER: {
                CHECK(handlers.peer_handler(d_ctx, entry) == -1);
                break;
            }
            case FD_RELAY: {
                CHECK(handlers.relay_handler(d_ctx, entry) == -1);
                break;
            }
            case FD_DEV: {
                CHECK(handlers.device_handler(d_ctx, entry) == -1);
                break;
            }
            case FD_ISOCK: return 0;
            case FD_USOCK: return 0;
            default: ERR_GOTO(err, "err: invalid entry-type\n");
        }
        CHECK(d_epoll_entry_sync_stream(d_ctx->ep_ctx, entry, stream) == -1);
    }
    return 0;
err:
    ERR_LOG("resolve_fd_context");
    return -1;
}

int d_loop(struct d_context* d_ctx, struct epoll_handlers handlers) {
    struct epoll_context* ep_ctx = d_ctx->ep_ctx;
    while (can_exit(d_ctx) == false) {
        const size_t fd_count = ep_ctx->entries.r.length;
        struct epoll_event epoll_buff[fd_count];
        const int nfds = TRY(dispatch_epoll(ep_ctx, epoll_buff, fd_count), -1);
        CHECK(resolve_fd_context(d_ctx, handlers, epoll_buff, nfds) == -1);
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