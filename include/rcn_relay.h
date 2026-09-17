#ifndef RCN_RCN_RELAY_H
#define RCN_RCN_RELAY_H

#include "rcn_daemon.h"

enum relay_msg_type {
    RELAY_MSG_CONTINUE,
    RELAY_MSG_START,
    RELAY_MSG_PAUSE,
    RELAY_MSG_RESUME,
    RELAY_MSG_STOP,
    RELAY_MSG_ERR
};

enum relay_state {
    RELAY_AWAIT,
    RELAY_SLEEP,
};

struct relay_msg {
    enum relay_msg_type type;
};

struct relay_arg {
    enum relay_msg_type type_sent;
    enum daemon_type d_type;
};

struct relay {
    struct stream stream;
    enum relay_state state;
};

struct relay_context {
    relay_arr relays;
    int usock_fd;
};

typedef typeof(int(struct relay_arg arg)) *r_handler_t;

/* rcn_relay.c */
int r_init_usock(char* sock_path, size_t path_len);
int r_trigger(struct relay_arg arg);

#endif //RCN_RCN_RELAY_H
