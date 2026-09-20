#ifndef RCN_RCN_STREAM_H
#define RCN_RCN_STREAM_H

#include "rcn_types.h"

int stream(struct stream* stream);
int stream_init(struct stream* stream, int fd, enum fd_type type);
int stream_queue_reading(struct stream* stream, size_t size);
int stream_queue_writing(struct stream* stream, size_t size, void* data);
int stream_set_default(struct stream* stream, size_t size, enum stream_operation default_op);
int stream_header(struct stream* stream);
int stream_clear_fallback(struct stream* stream);
int stream_collect(struct stream* stream, struct stream_data** stream_data);
int stream_close(struct stream* stream);

#endif //RCN_RCN_STREAM_H
