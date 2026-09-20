#ifndef RCN_RCN_DAEMON_H
#define RCN_RCN_DAEMON_H

#include "rcn_peer.h"
#include "rcn_device.h"
#include "rcn_relay.h"
#include <sys/epoll.h>

enum d_msg_source {
    SRC_RELAY,
    SRC_PEER,
};

struct d_context {
    struct epoll_context* ep_ctx;
    struct peer_context* peer_ctx;
    struct device_context* device_ctx;
    struct relay_context* relay_ctx;
    bool exit;
};

typedef typeof(int(struct d_context* d_ctx, struct epoll_entry* entry)) *epoll_handler_t;

struct epoll_handlers {
    epoll_handler_t peer_handler;
    epoll_handler_t relay_handler;
    epoll_handler_t device_handler;
};

struct daemon_arg {
    struct d_context d_ctx;
    struct epoll_handlers handlers;
    enum daemon_type d_type;
};

/* rcn_daemon.c */
int d_init_dir();
int d_init_log(enum daemon_type d_type);
int d_init_usock(char* sock_path, size_t path_len);
int d_init_peer_ctx(struct epoll_context* ep_ctx, struct peer_context* p_ctx, int isock_fd);
int d_init_device_ctx(struct device_context* d_ctx);
int d_init_relay_ctx(struct epoll_context* ep_ctx, struct relay_context* r_ctx, int usock_fd);
int d_init_epoll_ctx(struct epoll_context* ep_ctx);
int d_epoll_add(struct epoll_context* ep_ctx, int fd, enum fd_type);
int d_epoll_close_remove(struct epoll_context* ep_ctx, struct epoll_entry* entry);
int d_epoll_entry_sync_stream(struct epoll_context* ep_ctx, struct epoll_entry* entry, struct stream* stream);
int d_print_log(enum daemon_type d_type);
ssize_t d_stream_or_close(struct epoll_context* ep_ctx, struct epoll_entry* entry);
int d_fork(struct daemon_arg* d_arg, struct relay_arg r_arg);
int d_loop(struct d_context* d_ctx, struct epoll_handlers handlers);

#endif //RCN_RCN_DAEMON_H