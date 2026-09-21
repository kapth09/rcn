#ifndef RCN_RCN_TYPES_H
#define RCN_RCN_TYPES_H

#include "rcn_util.h"

U_DEFINE_ARR(epoll_stream_arr, struct epoll_stream*);
U_DEFINE_ARR(device_arr, struct device);
U_DEFINE_ARR(relay_arr, struct relay);
U_DEFINE_ARR(char_arr, char);

U_DEFINE_QUEUE(stream_queue, struct stream_data);

typedef int stream_header_t;

enum rcn_state {
    RCN_RUNNING,
    RCN_PAUSED,
};

enum stream_state {
    STREAM_STREAMING,
    STREAM_COMPLETE,
    STREAM_CLOSED,
};

enum stream_operation {
    STREAM_WRITING,
    STREAM_READING,
};

struct stream_data {
    void* buffer;
    size_t buffer_size;
    size_t buffer_streamed;
    enum stream_state state;
    enum stream_operation op;
};

enum fd_type {
    FD_USOCK,
    FD_RELAY,
    FD_ISOCK,
    FD_PEER,
    FD_UDEV,
    FD_DEV,
};

struct epoll_stream {
    struct stream_data fallback;
    struct stream_data* next;
    stream_queue queue;
    stream_header_t header;
    enum stream_operation default_op;
    enum fd_type fd_type;
    int fd;
};

struct epoll_context {
    epoll_stream_arr stream_ptrs;    // stores pointers to struct epoll_stream
    int epoll_fd;
    int relay_count;
    int peer_count;
};

enum daemon_type {
    DAEMON_SERVER,
    DAEMON_CLIENT,
};

#endif //RCN_RCN_TYPES_H
