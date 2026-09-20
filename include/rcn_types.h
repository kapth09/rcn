#ifndef RCN_RCN_TYPES_H
#define RCN_RCN_TYPES_H

#include "rcn_util.h"

U_DEFINE_ARR(epoll_entry_arr, struct epoll_entry);
U_DEFINE_ARR(device_arr, struct device);
U_DEFINE_ARR(relay_arr, struct relay);
U_DEFINE_ARR(char_arr, char);

typedef int stream_header_t;

enum stream_state {
    STREAM_STREAMING,
    STREAM_COMPLETE,
    STREAM_CLOSED,
};

enum stream_operation {
    STREAM_WRITING,
    STREAM_READING,
};

struct stream {
    void* buffer;
    size_t buffer_size;
    size_t buffer_streamed;
    enum stream_state state;
    enum stream_operation op;
    enum stream_operation default_op;
    int fd;
    stream_header_t header;
};

enum fd_type {
    FD_USOCK,
    FD_RELAY,
    FD_ISOCK,
    FD_PEER,
    FD_UDEV,
    FD_DEV,
};

struct epoll_entry {
    enum fd_type type;
    struct stream stream;
};

struct epoll_context {
    epoll_entry_arr entries;    // stores pointers to struct epoll_entry
    int epoll_fd;
    int relay_count;
    int peer_count;
};

enum daemon_type {
    DAEMON_SERVER,
    DAEMON_CLIENT,
};

#endif //RCN_RCN_TYPES_H
