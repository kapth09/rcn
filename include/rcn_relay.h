#ifndef RCN_RCN_RELAY_H
#define RCN_RCN_RELAY_H

#include "rcn_arg.h"
#include <linux/uinput.h>
#include <signal.h>

#define DEFAULT_USOCK_COUNT 3
#define RELAY_SLEEP_TIMEOUT 5

/* forward declarations */
struct d_context;

enum relay_msg_header {
    RELAY_HEADER_IDLE,
    RELAY_HEADER_START,
    RELAY_HEADER_PAUSE,
    RELAY_HEADER_PAUSE_AGAIN,
    RELAY_HEADER_RESUME,
    RELAY_HEADER_RESUME_AGAIN,
    RELAY_HEADER_NO_PEER,
    RELAY_HEADER_STOP,
    RELAY_HEADER_SWITCH,
    RELAY_HEADER_LIST,
    RELAY_HEADER_ADD_DEV,
    RELAY_HEADER_RM_DEV,
    RELAY_HEADER_MOD_DEV,
};

struct relay_msg {
    enum relay_msg_header header;
};

struct relay_data_list {
    char dev_name[UINPUT_MAX_NAME_SIZE];
    uint16_t random_id;
    bool grabbed;
};

struct relay_context {
    epoll_stream_arr relay_streams;
    int usock_fd;
};

struct relay_arg {
    __sighandler_t sighandler;
    int evt_fd;
    bool check_daemon;
    enum daemon_type d_type;
};

/* rcn_relay.c */
int r_init_usock(char* sock_path, size_t path_len);
int r_connect_usock(char *sock_path, size_t path_len);
int r_close_relay(struct relay_context* r_ctx, struct epoll_stream* stream);
int r_broadcast_relay_header(struct epoll_context* ep_ctx, epoll_stream_arr* relay_streams, enum relay_msg_header);
int r_init_relay_ctx(struct epoll_context* ep_ctx, struct relay_context* r_ctx, int usock_fd);
int r_close_relay_ctx(struct epoll_context* ep_ctx, struct relay_context* r_ctx);
int relay_start(struct epoll_stream* relay_stream, struct relay_arg r_arg);

#endif //RCN_RCN_RELAY_H