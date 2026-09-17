#ifndef RCN_RCN_TYPES_H
#define RCN_RCN_TYPES_H

#include "rcn_util.h"

U_DEFINE_ARR(epoll_entry_arr, struct epoll_entry);
U_DEFINE_ARR(device_arr, struct device);
U_DEFINE_ARR(relay_arr, struct relay);
U_DEFINE_ARR(char_arr, char);

enum stream_state {
    D_STREAM_WRITING,
    D_STREAM_READING,
    D_STREAM_COMPLETE,
    D_STREAM_CLOSED,
};

struct stream {
    void* buffer;
    size_t buffer_size;
    size_t buffer_streamed;
    int fd;
    enum stream_state state;
};

enum fd_type {
    FD_USOCK,
    FD_RELAY,
    FD_ISOCK,
    FD_PEER,
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

#endif //RCN_RCN_TYPES_H
