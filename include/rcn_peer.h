#ifndef RCN_RCN_PEER_H
#define RCN_RCN_PEER_H

#include "rcn_device.h"

/* forward declaration */
struct d_context;

enum peer_msg_header {
    PEER_HEADER_DEV_CRT,        // sends a single struct device_info
    PEER_HEADER_DEV_DEL,        // sends a single random_id of a device
    PEER_HEADER_DEV_UPD_GRAB,   // sends a single struct peer_msg_dev_upd_grab
    PEER_HEADER_EVENT,
    PEER_HEADER_PAUSE,
    PEER_HEADER_RESUME,
    PEER_HEADER_STOP,
};

struct peer_msg_dev_upd_grab {
    uint16_t random_id;
    bool grab;
};

struct peer_msg_event {
    struct input_event evt_data;
    uint16_t random_id;
};

enum peer_state {
    PEER_DISCONNECTED,
    PEER_CONNECTED,
};

struct peer_context {
    struct epoll_stream* isock_stream;
    struct epoll_stream* peer_stream;
    int isock_port;
    enum peer_state isock_state;
    enum peer_state peer_state;
};

int p_init_peer_ctx(struct epoll_context* ep_ctx, struct peer_context* p_ctx, int isock_fd, int port, enum daemon_type d_type);
int p_close_peer_ctx(struct epoll_context* ep_ctx, struct peer_context* p_ctx);
int p_close_peer_naive(struct epoll_context* ep_ctx, struct peer_context* p_ctx);
int p_connect_sock(int port, char* host);
int p_init_sock(int port);

#endif //RCN_RCN_PEER_H
