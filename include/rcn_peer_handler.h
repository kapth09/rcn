#ifndef RCN_RCN_PEER_HANDLER_H
#define RCN_RCN_PEER_HANDLER_H

struct d_context;
struct epoll_stream;
struct stream_item;

int peer_handler(struct d_context* d_ctx, struct epoll_stream* stream, struct stream_item* stream_item);

#endif //RCN_RCN_PEER_HANDLER_H
