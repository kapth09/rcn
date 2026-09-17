#include "include/rcn.h"
#include "include/rcn_evdev.h"
#include "include/rcn_daemon.h"
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>

static int init_psock(const int port) {
    const int isock_fd = TRY(socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0), -1);
    const int reuse = 1;
    CHECK(setsockopt(isock_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1);
    struct sockaddr_in s_iaddr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = INADDR_ANY
    };
    CHECK(bind(isock_fd, (struct sockaddr*)&s_iaddr, sizeof(s_iaddr)) == -1);
    CHECK(listen(isock_fd, 0) == -1);
    return isock_fd;
err:
    ERR_LOG("server init_isock");
    return -1;
}

static int handler_device(struct d_handler_context *h_ctx, struct epoll_entry *entry) {
    // data is never read from uinput devices, so this function is empty
    (void) entry;
    (void) h_ctx;
    return 0;
}
static int emit_event(device_info_arr *devices, struct peer_msg_event event) {
    struct device_info *dev = NULL;
    for (size_t i = 0; i < devices->r.length; i++) {
        CHECK(u_array_getr(&devices->r, (void**)&dev, i) == -1);
        if (dev->random_id == event.random_id)
            break;
    }
    CHECK(dev == NULL);
    if (event.evt_data.type == EV_SYN)
        printf("syn\n");
    CHECK(d_write_all(dev->fd, &event.evt_data, sizeof(event.evt_data)) == -1);
    return 0;
err:
    ERR_LOG("emit_event");
    return -1;
}

static int handler_peer(struct d_handler_context *h_ctx, struct epoll_entry *entry) {
    struct peer_msg msg = {};
    ssize_t read_bytes = TRY(d_read_or_close(h_ctx->ep_ctx, entry, &msg, sizeof(msg)), -1);
    if (read_bytes == 0)
        return 0;
    switch (msg.type) {
        case PEER_MSG_PAUSE: {
            printf("server: peer pause\n");
            CHECK(d_sock_msg(h_ctx, SRC_PEER, PEER_MSG_PAUSE) == -1);
            break;
        }
        case PEER_MSG_RESUME: {
            printf("server: peer resume\n");
            CHECK(d_sock_msg(h_ctx, SRC_PEER, PEER_MSG_RESUME) == -1);
            break;
        }
        case PEER_MSG_STOP: {
            printf("server: peer stop\n");
            CHECK(d_sock_msg(h_ctx, SRC_PEER, PEER_MSG_STOP) == -1);
            break;
        }
        case PEER_MSG_EVT: {
            printf("server: peer evt\n");
            CHECK(emit_event(h_ctx->devices, msg.data.event) == -1);
            break;
        }
        case PEER_MSG_DEV_CRT: {
            printf("server: peer dev crt\n");
            CHECK(e_create_udev(h_ctx->ep_ctx, h_ctx->devices, &msg.data.dev_info) == -1);
            break;
        }
        case PEER_MSG_DEV_DEL: {
            printf("server: peer dev del\n");
            break;
        }
        default: break;
    }
    return 0;
err:
    ERR_LOG("handler_peer");
    return -1;
}

static int handler_relay(struct d_handler_context *h_ctx, struct epoll_entry *entry) {
    struct relay_msg msg = {};
    ssize_t read_bytes = TRY(d_read_or_close(h_ctx->ep_ctx, entry, &msg, sizeof(msg)), -1);
    if (read_bytes == 0)
        return 0;
    switch (msg.type) {
        case RELAY_MSG_START: {
            printf("server: relay start\n");
            CHECK(d_write_relay(entry->fd, RELAY_MSG_STOP) == -1);
            break;
        }
        case RELAY_MSG_PAUSE: {
            printf("server: relay pause\n");
            CHECK(d_sock_msg(h_ctx, SRC_RELAY, PEER_MSG_PAUSE) == -1);
            break;
        }
        case RELAY_MSG_RESUME: {
            printf("server: relay resume\n");
            CHECK(d_sock_msg(h_ctx, SRC_RELAY, PEER_MSG_RESUME) == -1);
            break;
        }
        case RELAY_MSG_STOP: {
            printf("server: relay stop\n");
            h_ctx->exit = true;
            CHECK(d_sock_msg(h_ctx, SRC_RELAY, PEER_MSG_STOP) == -1);
            break;
        }
        case RELAY_MSG_CONTINUE: // do nothing, fall through
        default: break;
    }
    return 0;
err:
    ERR_LOG("handler_relay");
    return -1;
}

int s_start(const int port) {
    device_info_arr devices = {};
    CHECK(u_array_init(&devices.r, sizeof(struct device_info), RCN_STD_CAPACITY) == -1);

    struct epoll_context ep_ctx = {};
    CHECK(d_init_epoll(&ep_ctx) == -1);

    CHECK(d_init_dir() == -1);
    const int usock_fd = TRY(d_init_usock(RCN_SERVER_SOCKET_PATH, RCN_SERVER_SOCKET_LEN), -1);
    CHECK(d_epoll_add(&ep_ctx, usock_fd, FD_USOCK) == -1);

    const int isock_fd = TRY(init_psock(port), -1);
    CHECK(d_epoll_add(&ep_ctx, isock_fd, FD_ISOCK) == -1);

    struct daemon_arg d_arg = {
        .d_type = DAEMON_SERVER,
        .peer_fd = -1,
        .ep_ctx = &ep_ctx,
        .devices = &devices,
        .handlers = {
            .relay = handler_relay,
            .peer = handler_peer,
            .device = handler_device,
        }
    };
    struct relay_arg r_arg = {
        .type_sent = RELAY_MSG_START,
        .d_type = DAEMON_SERVER,
    };
    return d_fork(&d_arg, r_trigger, r_arg);
err:
    if (ep_ctx.entries.r.data != NULL)
        u_array_free(&ep_ctx.entries.r);
    if (devices.r.data != NULL)
        u_array_free(&devices.r);
    ERR_LOG("s_start");
    return -1;
}
