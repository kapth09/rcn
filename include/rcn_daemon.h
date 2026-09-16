#ifndef RCN_RCN_DAEMON_H
#define RCN_RCN_DAEMON_H

#include "rcn.h"
#include "rcn_peer.h"
#include "rcn_relay.h"

enum d_msg_source {
    SRC_RELAY,
    SRC_PEER,
};

struct d_handler_context {
    struct epoll_context* ep_ctx;
    device_info_arr* devices;
    int peer_fd;
    bool exit;
};

typedef typeof(int(struct d_handler_context* handler_ctx, struct epoll_entry* entry)) * fd_handler_t;
typedef typeof(int(struct d_handler_context* handler_ctx)) *exit_handler_t;

struct d_loop_handlers {
    fd_handler_t relay;
    fd_handler_t peer;
    fd_handler_t device;
};

struct daemon_arg {
    struct epoll_context* ep_ctx;
    device_info_arr* devices;
    struct d_loop_handlers handlers;
    int peer_fd;
    enum daemon_type d_type;
};

/* rcn_daemon.c */
int d_read_or_close(struct epoll_context* ep_ctx, struct epoll_entry* entry, void* buffer, size_t size);
int d_print_log(enum daemon_type d_type);
int d_init_dir();
int d_init_log(enum daemon_type d_type);
int d_init_usock(char* sock_path, size_t path_len);
int d_init_epoll(struct epoll_context* ep_ctx);
int d_epoll_add(struct epoll_context* ep_ctx, int fd, enum fd_type);
int d_epoll_add_device(struct epoll_context* ep_ctx, struct device_info* dev);
int d_epoll_close_remove(struct epoll_context* ep_ctx, struct epoll_entry* entry);
int d_fork(struct daemon_arg* d_arg, r_handler_t r_handler, struct relay_arg r_arg);
int d_loop(struct epoll_context* ep_ctx, device_info_arr* devices, struct d_loop_handlers handlers, int peer_fd);
int d_write_relay(int relay_fd, enum relay_msg_type msg_type);
int d_write_peer(int peer_fd, enum peer_msg_type type, union peer_msg_data data);
int d_sock_msg(struct d_handler_context* h_ctx, enum d_msg_source source, enum peer_msg_type peer_msg);

#endif //RCN_RCN_DAEMON_H
