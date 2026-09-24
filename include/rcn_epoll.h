#ifndef RCN_RCN_EPOLL_H
#define RCN_RCN_EPOLL_H

#include "rcn_types.h"
#include "rcn_daemon.h"

int e_init_epoll_ctx(struct epoll_context* ep_ctx);
int e_close_epoll_ctx(struct epoll_context* ep_ctx);
int e_epoll_add(struct epoll_context* ep_ctx, int fd, enum fd_type type);
int e_epoll_add_getr(struct epoll_context* ep_ctx, int fd, enum fd_type type, struct epoll_stream** out_stream);
int e_epoll_add_device(struct epoll_context* ep_ctx, int fd, struct device* device, enum fd_type type);
int e_epoll_close_remove(struct d_context* d_ctx, struct epoll_stream* stream);
int e_epoll_close_remove_simple(struct epoll_context* ep_ctx, struct epoll_stream* stream);
int e_epoll_sync_stream(struct epoll_context* ep_ctx, struct epoll_stream* stream);
int e_epoll_reset_stream(struct epoll_context* ep_ctx, struct epoll_stream* stream);

#endif //RCN_RCN_EPOLL_H
