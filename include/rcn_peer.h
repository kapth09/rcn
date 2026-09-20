#ifndef RCN_RCN_PEER_H
#define RCN_RCN_PEER_H

#include "rcn_device.h"

enum peer_msg_header {
    PEER_HEADER_IDLE,
    PEER_HEADER_DEV_CRT,
    PEER_HEADER_DEV_DEL,
    PEER_HEADER_EVENT,
    PEER_HEADER_PAUSE,
    PEER_HEADER_RESUME,
    PEER_HEADER_STOP,
};

struct peer_msg_dev_crt {
    struct device dev;
};

struct peer_msg_dev_del {
    size_t random_id;
};

struct peer_msg_event {
    struct input_event evt_data;
    size_t random_id;
};

enum peer_conn_state {
    PERR_CONN_DISCONNECTED,
    PEER_CONN_PAUSED,
    PEER_CONN_RESUMED,
};

struct peer_context {
    struct stream stream;
    enum peer_conn_state peer_state;
    enum peer_msg_header expected_msg;
    int isock_fd;
};

size_t p_msg_size(enum peer_msg_header msg_type);

#endif //RCN_RCN_PEER_H
