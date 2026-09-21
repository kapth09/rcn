#ifndef RCN_RCN_STREAM_H
#define RCN_RCN_STREAM_H

#include "rcn_types.h"

int stream_stream(struct epoll_stream* stream);
int stream_init(struct epoll_stream* stream, int fd, enum fd_type type);
int stream_queue_reading(struct epoll_stream* stream, size_t size);
int stream_queue_writing(struct epoll_context* ep_ctx, struct epoll_stream* stream, size_t size, void* data);
int stream_set_default(struct epoll_stream* stream, size_t size, enum stream_operation default_op);
int stream_header(struct epoll_stream* stream);
int stream_clear_fallback(struct epoll_stream* stream);
int stream_collect(struct epoll_stream* stream, struct stream_data** stream_data);
int stream_close(struct epoll_stream* stream);

#endif //RCN_RCN_STREAM_H
