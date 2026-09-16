#ifndef RCN_RCN_PEER_H
#define RCN_RCN_PEER_H

#include "rcn.h"
#include "rcn_evdev.h"

enum peer_msg_type {
    PEER_MSG_DEV_CRT,
    PEER_MSG_DEV_DEL,
    PEER_MSG_EVT,
    PEER_MSG_PAUSE,
    PEER_MSG_RESUME,
    PEER_MSG_STOP,
};

struct peer_msg_event {
    struct input_event evt_data;
    size_t random_id;
};

union peer_msg_data {
    struct device_info dev_info;
    struct peer_msg_event event;
};

struct peer_msg {
    enum peer_msg_type type;
    union peer_msg_data data;
};

#endif //RCN_RCN_PEER_H
