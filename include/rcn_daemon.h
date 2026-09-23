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
int d_print_log(enum daemon_type d_type);
ssize_t d_stream_or_close(struct d_context* d_ctx, struct epoll_stream* stream);
int d_fork(struct daemon_arg* d_arg, struct relay_arg r_arg);
int d_loop(struct d_context* d_ctx, struct epoll_handlers handlers);

#endif //RCN_RCN_DAEMON_H