#ifndef RCN_RCN_RELAY_H
#define RCN_RCN_RELAY_H

enum relay_msg_header {
    RELAY_HEADER_IDLE,
    RELAY_HEADER_AWAIT,
    RELAY_HEADER_PAUSE,
    RELAY_HEADER_RESUME,
    RELAY_HEADER_STOP,
    RELAY_HEADER_ERR
};

struct relay_msg {
    enum relay_msg_header header;
};

struct relay_arg {
    enum relay_msg_header header_sent;
    enum daemon_type d_type;
};

struct relay_context {
    epoll_stream_arr relay_streams;
    int usock_fd;
};

typedef typeof(int(struct relay_arg arg)) *r_handler_t;

/* rcn_relay.c */
int r_init_usock(char* sock_path, size_t path_len);
int r_trigger(struct relay_arg arg);
int r_close_relay(struct relay_context* r_ctx, struct epoll_stream* stream);

#endif //RCN_RCN_RELAY_H
