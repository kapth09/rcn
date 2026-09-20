#include "include/rcn.h"
#include "include/rcn_relay.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/un.h>

struct sock_info {
    char* sock_path;
    size_t path_len;
};

static const char* relay_text[] = {
    [RELAY_HEADER_IDLE] = "",
    [RELAY_HEADER_AWAIT] = "started",
    [RELAY_HEADER_PAUSE] = "paused",
    [RELAY_HEADER_RESUME] = "resumed",
    [RELAY_HEADER_STOP] = "stopped",
    [RELAY_HEADER_ERR] = "error",
};

static int init_sockinfo(enum daemon_type d_type, struct sock_info* info) {
    if (d_type == DAEMON_SERVER) {
        info->sock_path = RCN_SERVER_SOCKET_PATH;
        info->path_len = RCN_SERVER_SOCKET_LEN;
    } else if (d_type == DAEMON_CLIENT) {
        info->sock_path = RCN_CLIENT_SOCKET_PATH;
        info->path_len = RCN_CLIENT_SOCKET_LEN;
    } else {
        goto err;
    }
    return 0;
err:
    ERR_LOG("init_sockinfo");
    return -1;
}

int r_init_usock(char* sock_path, size_t path_len) {
    int usock_fd = TRY(socket(AF_UNIX, SOCK_STREAM, 0), -1);
    struct sockaddr_un r_uaddr = {
        .sun_family = AF_UNIX,
    };
    memcpy(r_uaddr.sun_path, sock_path, path_len);
    CHECK(connect(usock_fd, (struct sockaddr*)&r_uaddr, sizeof(r_uaddr)) == -1);
    return usock_fd;
err:
    ERR_LOG("r_init_usock");
    return -1;
}

int r_trigger(struct relay_arg arg) {
    struct sock_info info = { 0 };
    CHECK(init_sockinfo(arg.d_type, &info) == -1);
    int usock_fd = TRY(r_init_usock(info.sock_path, info.path_len), -1);
    printf("rcn>");
    fflush(stdout);
    struct relay_msg msg = { .header = arg.header_sent };
    CHECK(write(usock_fd, &msg, sizeof(msg)) == -1);
    // blocking read on .sock to wait for daemon
    CHECK(read(usock_fd, &msg, sizeof(msg)) == -1);
    CHECK(c_close_connection(usock_fd) == -1);
    printf("\rrcn: %s\n", relay_text[arg.header_sent]);
    return 0;
err:
    ERR_LOG("r_await");
    return -1;
}