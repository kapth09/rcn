#include "include/rcn.h"
#include "include/rcn_daemon.h"

#include <fcntl.h>

#include "include/rcn_evdev.h"
#include <stdlib.h>
#include <unistd.h>
#include <grp.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define DEFAULT_USOCK_COUNT 3

int d_write_all(int fd, void* data, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t chunk = write(fd, ((uint8_t*)(data)) + total, len - total);
        if (chunk == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                usleep(1000000);    // sleep 1ms to avoid busy waiting
            else
                goto err;
        } else {
            total += (size_t)chunk;
        }
    }
    return 0;
err:
    ERR_LOG("d_write_all");
    return -1;
}

ssize_t d_read_all(int fd, void* data, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t chunk = read(fd, ((uint8_t*)(data)) + total, len - total);
        if (chunk == 0)
            break;
        if (chunk == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                usleep(1000000);    // sleep 1ms to avoid busy waiting
            else
                goto err;
        } else {
            total += (size_t)chunk;
        }
    }
    return total;
err:
    if (errno != 0)
        ERR_LOG("d_read_all");
    return -1;
}

ssize_t d_read_or_close(struct epoll_context* ep_ctx, struct epoll_entry* entry, void* buffer, size_t size) {
    ssize_t read = d_read_all(entry->fd, buffer, size);
    if (read == -1 || read == 0)
        CHECK(d_epoll_close_remove(ep_ctx, entry) == -1);
    return read;
err:
    ERR_LOG("d_read_or_close");
    return -1;
}

static int accept_sock(struct d_handler_context* h_ctx, struct epoll_entry* entry, enum fd_type type) {
    struct sockaddr_un r_uaddr = {};
    struct sockaddr_in p_iaddr = {};
    struct sockaddr* addr;
    socklen_t addr_len;
    enum fd_type conn_type;
    if (type == FD_USOCK) {
        addr = (struct sockaddr*)&r_uaddr;
        addr_len = sizeof(r_uaddr);
        conn_type = FD_RELAY;
    } else if (type == FD_ISOCK) {
        addr = (struct sockaddr*)&p_iaddr;
        addr_len = sizeof(p_iaddr);
        conn_type = FD_PEER;
    } else {
        goto err;
    }
    int sock_fd = TRY(accept(entry->fd, addr, &addr_len), -1);
    CHECK(fcntl(sock_fd, F_SETFL, O_NONBLOCK) == -1);
    if (conn_type == FD_PEER)
        h_ctx->peer_fd = sock_fd;
    CHECK(d_epoll_add(h_ctx->ep_ctx, sock_fd, conn_type) == -1);
    return 0;
err:
    ERR_LOG("accept_sock");
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

int d_write_peer(int peer_fd, enum peer_msg_type type, union peer_msg_data data) {
    struct peer_msg msg = {
        .type = type,
        .data = data
    };
    CHECK(d_write_all(peer_fd, &msg, sizeof(msg)) == -1);
    return 0;
err:
    ERR_LOG("d_write_peer");
    return -1;
}

int d_write_relay(int relay_fd, enum relay_msg_type msg_type) {
    struct relay_msg msg = {
        .type = msg_type
    };
    CHECK(d_write_all(relay_fd, &msg, sizeof(msg)) == -1);
    return 0;
err:
    ERR_LOG("d_write_relay");
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

int d_init_epoll(struct epoll_context* ep_ctx) {
    ep_ctx->epoll_fd = TRY(epoll_create1(0), -1);
    CHECK(u_array_init(&ep_ctx->entries.r, sizeof(struct epoll_entry*), RCN_STD_CAPACITY) == -1);
    return 0;
err:
    ERR_LOG("d_init_epoll");
    return -1;
}

static int update_epoll_counter(struct epoll_context* ep_ctx, enum fd_type type, int op) {
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
    if (type == FD_DEV)
        DO_GOTO(fprintf(stderr, "type FD_DEV invalid for normal d_epoll_add\n"), err);
    struct epoll_entry* entry = TRY(calloc(1, sizeof(struct epoll_entry)), NULL);
    entry->type = type;
    entry->fd = fd;
    entry->device = NULL;
    CHECK(u_array_add(&ep_ctx->entries.r, &entry) == -1);
    struct epoll_event u_evt = { .events = EPOLLIN, .data.ptr = entry, };
    CHECK(epoll_ctl(ep_ctx->epoll_fd, EPOLL_CTL_ADD, fd, &u_evt) == -1);
    CHECK(update_epoll_counter(ep_ctx, type, 1) == -1);
    return 0;
err:
    ERR_LOG("d_epoll_add");
    return -1;
}

int d_epoll_add_device(struct epoll_context* ep_ctx, struct device_info* dev) {
    struct epoll_entry* entry = TRY(calloc(1, sizeof(struct epoll_entry)), NULL);
    entry->type = FD_DEV;
    entry->fd = dev->fd;
    entry->device = dev;
    dev->entry = entry;
    CHECK(u_array_add(&ep_ctx->entries.r, &entry) == -1);
    struct epoll_event u_evt = { .events = EPOLLIN, .data.ptr = entry };
    CHECK(epoll_ctl(ep_ctx->epoll_fd, EPOLL_CTL_ADD, entry->fd, &u_evt) == -1);
    CHECK(update_epoll_counter(ep_ctx, FD_DEV, 1) == -1);
    return 0;
err:
    ERR_LOG("d_epoll_add_device");
    return -1;
}

int d_epoll_close_remove(struct epoll_context* ep_ctx, struct epoll_entry* entry) {
    CHECK(epoll_ctl(ep_ctx->epoll_fd, EPOLL_CTL_DEL, entry->fd, NULL) == -1);
    size_t index = TRY(u_array_find_index(&ep_ctx->entries.r, &entry), -1);
    CHECK(u_array_remove(&ep_ctx->entries.r, index) == -1);
    CHECK(update_epoll_counter(ep_ctx, entry->type, -1) == -1);
    close(entry->fd);
    free(entry);
    return 0;
err:
    close(entry->fd);
    ERR_LOG("d_epoll_remove");
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

int d_sock_msg(struct d_handler_context* h_ctx, enum d_msg_source source, enum peer_msg_type peer_msg) {
    for (size_t i = 0; i < h_ctx->ep_ctx->entries.r.length; i++) {
        struct epoll_entry* entry = {};
        CHECK(u_array_getv(&h_ctx->ep_ctx->entries.r, (void**)&entry, i) == -1);
        if (entry->type == FD_RELAY) {
            CHECK(d_write_relay(entry->fd, RELAY_MSG_STOP) == -1);
        } else if (entry->type == FD_PEER && source == SRC_RELAY) {
            const union peer_msg_data data = {0};
            CHECK(d_write_peer(entry->fd, peer_msg, data) == -1);
        }
    }
    return 0;
err:
    ERR_LOG("d_msg_resume");
    return -1;
}

static int can_exit(struct d_handler_context* h_ctx) {
    if (h_ctx->exit == false)
        return 0;
    if (h_ctx->ep_ctx->peer_count > 0)
        return 0;
    if (h_ctx->ep_ctx->relay_count > 0)
        return 0;
    return 1;
}

int d_loop(struct epoll_context* ep_ctx, device_info_arr* devices, struct d_loop_handlers handlers, int peer_fd) {
    struct d_handler_context h_ctx = { .ep_ctx = ep_ctx, .devices = devices, .exit = false, .peer_fd = peer_fd};
    while (can_exit(&h_ctx) == false) {
        size_t fd_count = ep_ctx->entries.r.length;
        struct epoll_event epoll_buff[fd_count];
        int nfds = TRY(epoll_wait(ep_ctx->epoll_fd, epoll_buff, fd_count, -1), -1);
        for (int i = 0; i < nfds; i++) {
            struct epoll_event evt = epoll_buff[i];
            struct epoll_entry* entry = evt.data.ptr;
            switch (entry->type) {
                case FD_USOCK: {
                    CHECK(accept_sock(&h_ctx, entry, FD_USOCK));
                    break;
                }
                case FD_RELAY: {
                    CHECK(handlers.relay(&h_ctx, entry));
                    break;
                }
                case FD_ISOCK: {
                    CHECK(accept_sock(&h_ctx, entry, FD_ISOCK));
                    break;
                }
                case FD_PEER: {
                    CHECK(handlers.peer(&h_ctx, entry));
                    break;
                }
                case FD_DEV: {
                    CHECK(handlers.device(&h_ctx, entry));
                    break;
                }
                default: goto err;
            }
        }
    }
    return 0;
err:
    ERR_LOG("d_loop");
    return -1;
}

int cleanup(struct daemon_arg* d_arg) {
    device_info_arr* devices = d_arg->devices;
    for (size_t i = 0; i < devices->r.length; i++) {
        struct device_info dev = { 0 };
        CHECK(u_array_getv(&devices->r, &dev, i) == -1);
        CHECK(close(dev.entry->fd) == -1);
    }
    CHECK(u_array_free(&d_arg->devices->r) == -1);
    CHECK(u_array_free(&d_arg->ep_ctx->entries.r) == -1);
    return 0;
err:
    ERR_LOG("cleanup");
    return -1;
}

int run(struct daemon_arg* d_arg) {
    CHECK(setsid() == -1);
    CHECK(d_init_log(d_arg->d_type) == -1);
    CHECK(d_loop(d_arg->ep_ctx, d_arg->devices, d_arg->handlers, d_arg->peer_fd) == -1);
    CHECK(cleanup(d_arg) == -1);
    return 0;
err:
    ERR_LOG("d_run");
    return -1;
}

int d_fork(struct daemon_arg* d_arg, r_handler_t r_handler, struct relay_arg r_arg) {
    const pid_t pid = TRY(fork(), -1);
    if (pid == 0) {
        CHECK(run(d_arg) == -1);
    } else {
        CHECK(r_handler(r_arg) == -1);
    }
    return 0;
err:
    ERR_LOG("d_fork");
    return -1;
}