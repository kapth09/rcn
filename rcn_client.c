#include "include/rcn.h"
#include "include/rcn_daemon.h"
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

int c_close_connection(int fd) {
    int flags = TRY(fcntl(fd, F_GETFL, 0), -1);
    CHECK(fcntl(fd, F_SETFL, flags & ~O_NONBLOCK) == -1);
    // wait max. 1 second for the server to send a FIN paket, if not, force close the connection
    struct timeval tv = { .tv_sec =  1, .tv_usec =  0};
    CHECK(setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1);
    CHECK(shutdown(fd, SHUT_WR) == -1);
    uint8_t buffer[64] = { 0 };
    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {}
    if (bytes_read == -1 && (errno != EAGAIN && errno != EWOULDBLOCK))
        goto err;
    close(fd);
    return 0;
err:
    close(fd);
    ERR_LOG("c_close_connection");
    return -1;
}

static int resolve_host(const int port, const char *host, struct addrinfo** addr) {
    char port_str[6];
    snprintf(port_str, sizeof(port_str), "%d", port);
    struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    int res = getaddrinfo(host, port_str, &hints, addr);
    if (res != 0)
        goto err;
    return 0;
err:
    ERR_LOG("%s", gai_strerror(res));
    return -1;
}

static int init_psock(const int port, const char *host) {
    const int isock_fd = TRY(socket(AF_INET, SOCK_STREAM, 0), -1);
    const int timeout_ms = 5000;
    CHECK(setsockopt(isock_fd, IPPROTO_TCP, TCP_USER_TIMEOUT, &timeout_ms, sizeof(timeout_ms)) == -1);
    struct addrinfo* p_info = NULL;
    CHECK(resolve_host(port, host, &p_info) == -1);
    bool connected = false;
    for (struct addrinfo* i = p_info; i != NULL; i = i->ai_next) {
        int res = connect(isock_fd, i->ai_addr, i->ai_addrlen);
        if (res == 0) {
            connected = true;
            break;
        }
        CHECK(errno != ETIMEDOUT);
        printf("rcn: timeout\n");
    }
    freeaddrinfo(p_info);
    CHECK(connected == false);
    int fd_flags = TRY(fcntl(isock_fd, F_GETFL, 0), 1);
    CHECK(fcntl(isock_fd, F_SETFL, O_NONBLOCK | fd_flags) == -1);
    return isock_fd;
err:
    ERR_LOG("client init_isock");
    return -1;
}

static int handler_device(struct d_context *h_ctx, struct epoll_entry *entry) {
    return 0;
err:
    ERR_LOG("handler_device");
    return -1;
}

static int handler_peer(struct d_context *h_ctx, struct epoll_entry *entry) {
    return 0;
err:
    ERR_LOG("handler_peer");
    return -1;
}

static int handler_relay(struct d_context *h_ctx, struct epoll_entry *entry) {
    return 0;
err:
    ERR_LOG("handler_relay");
    return -1;
}

int c_start(int port, char *host, char_arr devices_arg) {
    device_arr devices = {};
    CHECK(u_array_init(&devices.r, sizeof(struct device), devices_arg.r.length) == -1);

    struct epoll_context ep_ctx = {};
    CHECK(d_init_epoll_ctx(&ep_ctx) == -1);

    CHECK(d_init_dir() == -1);
    const int usock_fd = TRY(d_init_usock(RCN_CLIENT_SOCKET_PATH, RCN_CLIENT_SOCKET_LEN), -1);
    CHECK(d_epoll_add(&ep_ctx, usock_fd, FD_USOCK) == -1);

    const int psock_fd = TRY(init_psock(port, host), -1);
    CHECK(d_epoll_add(&ep_ctx, psock_fd, FD_PEER) == -1);

    for (size_t i = 0; i < devices_arg.r.length; i++) {
        char *dev_path = NULL;
        CHECK(u_array_getv(&devices_arg.r, &dev_path, i) == -1);
        struct device dev = { 0 };
        CHECK(e_init_device(&ep_ctx, &devices, dev_path, &dev) == -1);
        union peer_msg_data data =  { .dev_info = dev };
        CHECK(d_write_peer(psock_fd, PEER_MSG_DEV_CRT, data) == -1);
        CHECK(e_grab_device_by_ptr(&dev, true) == -1);
    }

    struct daemon_arg d_arg = {
        .d_type = DAEMON_CLIENT,
        .peer_fd = psock_fd,
        .ep_ctx = &ep_ctx,
        .devices = &devices,
        .handlers = {
            .relay = handler_relay,
            .peer = handler_peer,
            .device = handler_device,
        }
    };
    struct relay_arg r_arg = {
        .type_sent = RELAY_MSG_CONTINUE,
        .d_type =  DAEMON_CLIENT,
    };
    CHECK(u_array_free(&devices_arg.r) == -1);
    return d_fork(&d_arg, r_trigger, r_arg);
err:
    if (ep_ctx.entries.r.data != NULL)
        u_array_free(&ep_ctx.entries.r);
    if (devices.r.data != NULL)
        u_array_free(&devices.r);
    ERR_LOG("c_start");
    return -1;
}
