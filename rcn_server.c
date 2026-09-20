#include "include/rcn.h"
#include "include/rcn_peer.h"
#include "include/rcn_device.h"
#include "include/rcn_daemon.h"
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>

#include "include/rcn_stream.h"

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
    ERR_LOG("server init_psock");
    return -1;
}

static int emit_event(device_arr *devices, struct peer_msg_event event) {
    struct device *dev = NULL;
    for (size_t i = 0; i < devices->r.length; i++) {
        CHECK(u_array_getr(&devices->r, (void**)&dev, i) == -1);
        if (dev->random_id == event.random_id)
            break;
    }
    CHECK(dev == NULL);
    if (event.evt_data.type == EV_SYN)
        printf("syn\n");
    // CHECK(d_write_all(dev->fd, &event.evt_data, sizeof(event.evt_data)) == -1);
    return 0;
err:
    ERR_LOG("emit_event");
    return -1;
}

static int handler_device(struct d_context* d_ctx, struct epoll_entry* entry) {
    return 0;
err:
    ERR_LOG("handler_device");
    return -1;
}

static int handler_peer(struct d_context *d_ctx, struct epoll_entry* entry) {
    return 0;
err:
    ERR_LOG("handler_peer");
    return -1;
}

static int handler_relay(struct d_context* d_ctx, struct epoll_entry* entry) {
    struct stream* stream = &entry->stream;
    switch (stream->header) {
        case RELAY_HEADER_AWAIT: {
            struct relay_msg msg = { .header = RELAY_HEADER_STOP };
            CHECK(stream_set_writing(stream, sizeof(msg), &msg) == -1);
            break;
        }
        default: ERR_GOTO(err, "err: unknown relay header '%d'", stream->header);
    }
    return 0;
err:
    ERR_LOG("handler_relay");
    return -1;
}

int s_start(const int port) {
    struct peer_context peer_ctx = {};
    struct device_context device_ctx = {};
    struct relay_context relay_ctx = {};
    struct epoll_context ep_ctx = {};

    CHECK(d_init_dir() == -1);

    CHECK(d_init_epoll_ctx(&ep_ctx) == -1);
    CHECK(d_init_device_ctx(&device_ctx) == -1);

    const int usock_fd = TRY(d_init_usock(RCN_SERVER_SOCKET_PATH, RCN_SERVER_SOCKET_LEN), -1);
    CHECK(d_init_relay_ctx(&ep_ctx, &relay_ctx, usock_fd) == -1);

    const int isock_fd = TRY(init_psock(port), -1);
    CHECK(d_init_peer_ctx(&ep_ctx, &peer_ctx, isock_fd) == -1);

    struct daemon_arg d_arg = {
        .d_type = DAEMON_SERVER,
        .handlers = {
            .peer_handler = handler_peer,
            .relay_handler = handler_relay,
            .device_handler = handler_device,
        },
        .d_ctx = {
            .ep_ctx = &ep_ctx,
            .peer_ctx = &peer_ctx,
            .relay_ctx = &relay_ctx,
            .device_ctx = &device_ctx,
            .exit = false,
        }
    };
    struct relay_arg r_arg = {
        .header_sent = RELAY_HEADER_AWAIT,
        .d_type = DAEMON_SERVER,
    };
    return d_fork(&d_arg, r_arg);
err:
    if (ep_ctx.entries.r.data != NULL)
        u_array_free(&ep_ctx.entries.r);
    if (device_ctx.devices.r.data != NULL)
        u_array_free(&device_ctx.devices.r);
    ERR_LOG("s_start");
    return -1;
}