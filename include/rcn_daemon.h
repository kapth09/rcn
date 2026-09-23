#ifndef RCN_RCN_DAEMON_H
#define RCN_RCN_DAEMON_H

#include "rcn_peer.h"
#include "rcn_device.h"
#include "rcn_relay.h"

struct d_context {
    struct epoll_context* ep_ctx;
    struct peer_context* peer_ctx;
    struct device_context* device_ctx;
    struct relay_context* relay_ctx;
    enum rcn_state state;
    enum daemon_type type;
    bool exit;
};

typedef typeof(int(struct d_context* d_ctx, struct epoll_stream* stream, struct stream_item* stream_item)) *epoll_handler_t;

struct epoll_handlers {
    epoll_handler_t peer_handler;
    epoll_handler_t relay_handler;
    epoll_handler_t device_handler;
};

struct daemon_arg {
    struct d_context d_ctx;
    struct epoll_handlers handlers;
};

/* rcn_daemon.c */
int d_init_dir();
int d_init_log(enum daemon_type d_type);
int d_init_epoll_ctx(struct epoll_context* ep_ctx);
int d_epoll_add(struct epoll_context* ep_ctx, int fd, enum fd_type type);
int d_epoll_add_getr(struct epoll_context* ep_ctx, int fd, enum fd_type type, struct epoll_stream** out_stream);
int d_epoll_add_device(struct epoll_context* ep_ctx, int fd, struct device* device, enum fd_type type);
int d_epoll_close_remove(struct d_context* d_ctx, struct epoll_stream* stream);
int d_epoll_sync_stream(struct epoll_context* ep_ctx, struct epoll_stream* stream);
int d_print_log(enum daemon_type d_type);
ssize_t d_stream_or_close(struct d_context* d_ctx, struct epoll_stream* stream);
int d_fork(struct daemon_arg* d_arg, struct relay_arg r_arg);
int d_loop(struct d_context* d_ctx, struct epoll_handlers handlers);

#endif //RCN_RCN_DAEMON_H