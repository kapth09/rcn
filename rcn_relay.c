#include "include/rcn.h"
#include "include/rcn_daemon.h"
#include "include/rcn_epoll.h"
#include "include/rcn_peer.h"
#include "include/rcn_relay.h"
#include "include/rcn_stream.h"
#include "include/rcn_types.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/un.h>
#include <unistd.h>

struct sock_info {
    char* sock_path;
    size_t path_len;
};

static const char* relay_text[] = {
    [RELAY_HEADER_IDLE] = "",
    [RELAY_HEADER_START] = "started",
    [RELAY_HEADER_PAUSE] = "paused",
    [RELAY_HEADER_PAUSE_AGAIN] = "already paused",
    [RELAY_HEADER_RESUME] = "resumed",
    [RELAY_HEADER_RESUME_AGAIN] = "already resumed",
    [RELAY_HEADER_STOP] = "stopped",
    [RELAY_HEADER_NO_PEER] = "no peer connected",
};

static int init_sockinfo(enum daemon_type d_type, struct sock_info* info) {
    if (d_type == DAEMON_SERVER) {
        info->sock_path = RCN_SERVER_USOCKET_PATH;
        info->path_len = RCN_SERVER_USOCKET_LEN;
    } else if (d_type == DAEMON_CLIENT) {
        info->sock_path = RCN_CLIENT_USOCKET_PATH;
        info->path_len = RCN_CLIENT_USOCKET_LEN;
    } else {
        goto err;
    }
    return 0;
err:
    DEBUG_LOG("");
    return -1;
}

int r_init_usock(char* sock_path, size_t path_len) {
    unlink(sock_path);
    int d_usock_fd = TRY(socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0), -1);
    struct sockaddr_un d_uaddr = {
        .sun_family = AF_UNIX,
    };
    memcpy(d_uaddr.sun_path, sock_path, path_len);
    CHECK(bind(d_usock_fd, (struct sockaddr*)&d_uaddr, sizeof(d_uaddr)) == -1);
    CHECK(listen(d_usock_fd, DEFAULT_USOCK_COUNT) == -1);
    LOG("created unix socket");
    return d_usock_fd;
err:
    DEBUG_LOG("");
    return -1;
}

static int connect_usock(char* sock_path, size_t path_len) {
    int usock_fd = TRY(socket(AF_UNIX, SOCK_STREAM, 0), -1);
    struct sockaddr_un r_uaddr = {};
    r_uaddr.sun_family = AF_UNIX;
    memcpy(r_uaddr.sun_path, sock_path, path_len);
    CHECK(connect(usock_fd, (struct sockaddr*)&r_uaddr, sizeof(r_uaddr)) == -1);
    return usock_fd;
err:
    ERR_LOG("");
    return -1;
}

int r_broadcast_relay_header(struct epoll_context* ep_ctx, epoll_stream_arr* relay_streams, enum relay_msg_header header) {
    for (size_t i = 0; i < relay_streams->r.length; i++) {
        struct epoll_stream* relay_stream = {};
        CHECK(u_array_getv(&relay_streams->r, &relay_stream, i) == -1);
        CHECK(stream_queue_writing_socket(ep_ctx, relay_stream, header, 0, NULL) == -1);
    }
    return 0;
err:
    DEBUG_LOG("");
    return -1;
}

static int check_daemon_status(int eventfd) {
    printf("...");
    fflush(stdout);
    int64_t status = 0;
    CHECK(read(eventfd, &status, sizeof(status)) == -1);
    CHECK(status != DAEMON_STATUS_OK);
    return 0;
err:
    fprintf(stderr, "\rerr: %s\n", strerror(status));
    fflush(stderr);
    DEBUG_LOG("");
    return -1;
}

static int handle_request(struct stream_item* item) {
    enum relay_msg_header header = item->payload.msg.header.value;
    switch (header) {
        case RELAY_HEADER_START:
        case RELAY_HEADER_PAUSE:
        case RELAY_HEADER_PAUSE_AGAIN:
        case RELAY_HEADER_RESUME:
        case RELAY_HEADER_RESUME_AGAIN:
        case RELAY_HEADER_NO_PEER:
        case RELAY_HEADER_STOP: printf("\rrcn: %s\n", relay_text[header]); break;
        case RELAY_HEADER_LIST: {
            if (item->payload.msg.header.size == 0) {
                printf("rcn: no devices captured\n");
                break;
            }
            relay_devices_list list = {};
            list.r.size = sizeof(struct relay_data_list);
            list.r.length = item->payload.msg.header.size / list.r.size;
            list.r.data = item->payload.msg.buffer;
            list.r.capacity = list.r.size * list.r.length;
            for (size_t i = 0; i < list.r.length; i++) {
                struct relay_data_list* list_entry = {};
                CHECK(u_array_getr(&list.r, (void**)&list_entry, i++) == -1);
                printf("\rrcn> %s ", list_entry->dev_name);
                if (list_entry->grabbed)
                    printf("[grabbed]\n");
                else
                    printf("[ungrabbed]\n");
            }
            break;
        }
        default: goto err;
    }
    return 0;
err:
    DEBUG_LOG("");
    return -1;
}

int relay_start(struct relay_arg arg) {
    if (arg.check_daemon_status == true)
        CHECK(check_daemon_status(arg.evtfd) == -1);
    CHECK(prctl(PR_SET_NAME, RCN_PROC_NAME_RELAY, 0UL, 0UL, 0UL) == -1);
    struct sock_info info = {};
    CHECK(init_sockinfo(arg.d_type, &info) == -1);
    int usock_fd = TRY(connect_usock(info.sock_path, info.path_len), -1);
    struct stream_header request = {};
    request.size = 0;
    request.value = arg.header_sent;
    CHECK(write(usock_fd, &request, sizeof(request)) == -1);
    CHECK(fcntl(usock_fd, F_SETFL, O_NONBLOCK) == -1);
    struct epoll_stream stream = {};
    CHECK(stream_init(&stream, usock_fd, FD_RELAY) == -1);
    struct pollfd fds = {};
    fds.fd = usock_fd;
    fds.events = POLLIN;
    printf("\rrcn>");
    fflush(stdout);
    for (;;) {
        CHECK(poll(&fds, 1, -1) == -1);
        CHECK(stream_stream(&stream) == -1);
        struct stream_item* item = stream.next;
        CHECK(item->state == STREAM_STREAMING_CLOSED);
        if (item->state != STREAM_STREAMING_COMPLETE)
            continue;
        CHECK(stream_collect(&stream, &item) == -1);
        CHECK(handle_request(item) == -1);
        break;
    }
    CHECK(stream_shutdown(&stream) == -1);
    return 0;
err:
    DEBUG_LOG("");
    return -1;
}

int r_close_relay(struct relay_context* r_ctx, struct epoll_stream* stream) {
    size_t index = TRY(u_array_find_index(&r_ctx->relay_streams.r, &stream), -1);
    CHECK(u_array_remove(&r_ctx->relay_streams.r, index) == -1);
    return 0;
err:
    DEBUG_LOG("");
    return -1;
}

static int handler_start(struct d_context* d_ctx, struct epoll_stream* stream, struct stream_item* stream_item) {
    (void)stream_item;
    CHECK(stream_queue_writing_socket(d_ctx->ep_ctx, stream, RELAY_HEADER_START, 0, NULL) == -1);
    return 0;
err:
    DEBUG_LOG("");
    return -1;
}

static int handler_pause(struct d_context* d_ctx, struct epoll_stream* stream, struct stream_item* stream_item) {
    (void)stream_item;
    if (d_ctx->state == RCN_PAUSED) {
        CHECK(stream_queue_writing_socket(d_ctx->ep_ctx, stream, RELAY_HEADER_PAUSE_AGAIN, 0, NULL) == -1);
        return 0;
    }
    if (d_ctx->type == DAEMON_CLIENT)
        CHECK(dev_ctrl_devices(d_ctx->ep_ctx, d_ctx->peer_ctx, &d_ctx->device_ctx->devices, DEV_CTRL_RELEASE) == -1);
    else if (d_ctx->type == DAEMON_SERVER)
        CHECK(dev_release_virt_keys_all(d_ctx->ep_ctx, &d_ctx->device_ctx->devices) == -1);
    epoll_stream_arr* relay_streams = &d_ctx->relay_ctx->relay_streams;
    if (d_ctx->peer_ctx->peer_state == PEER_CONNECTED) {
        CHECK(r_broadcast_relay_header(d_ctx->ep_ctx, relay_streams, RELAY_HEADER_PAUSE) == -1);
        CHECK(stream_queue_writing_socket(d_ctx->ep_ctx, d_ctx->peer_ctx->peer_stream, PEER_HEADER_PAUSE, 0, NULL) == -1);
        d_ctx->state = RCN_PAUSED;
    } else {
        CHECK(r_broadcast_relay_header(d_ctx->ep_ctx, relay_streams, RELAY_HEADER_NO_PEER) == -1);
    }
    return 0;
err:
    DEBUG_LOG("");
    return -1;
}

static int handler_resume(struct d_context* d_ctx, struct epoll_stream* stream, struct stream_item* stream_item) {
    (void)stream_item;
    if (d_ctx->state == RCN_RUNNING) {
        CHECK(stream_queue_writing_socket(d_ctx->ep_ctx, stream, RELAY_HEADER_RESUME_AGAIN, 0, NULL) == -1);
        return 0;
    }
    if (d_ctx->type == DAEMON_CLIENT)
        CHECK(dev_ctrl_devices(d_ctx->ep_ctx, d_ctx->peer_ctx, &d_ctx->device_ctx->devices, DEV_CTRL_CAPTURE) == -1);
    else if (d_ctx->type == DAEMON_SERVER)
        CHECK(dev_release_virt_keys_all(d_ctx->ep_ctx, &d_ctx->device_ctx->devices) == -1);
    CHECK(stream_queue_writing_socket(d_ctx->ep_ctx, d_ctx->peer_ctx->peer_stream, PEER_HEADER_RESUME, 0, NULL) == -1);
    d_ctx->state = RCN_RUNNING;
    return 0;
err:
    DEBUG_LOG("");
    return -1;
}

static int handler_stop(struct d_context* d_ctx, struct epoll_stream* stream, struct stream_item* stream_item) {
    (void)stream_item;
    (void)stream;
    d_ctx->exit = true;
    epoll_stream_arr* relay_streams = &d_ctx->relay_ctx->relay_streams;
    CHECK(r_broadcast_relay_header(d_ctx->ep_ctx, relay_streams, RELAY_HEADER_STOP) == -1);
    CHECK(stream_queue_writing_socket(d_ctx->ep_ctx, d_ctx->peer_ctx->peer_stream, PEER_HEADER_STOP, 0, NULL) == -1);
    return 0;
err:
    DEBUG_LOG("");
    return -1;
}

static int handler_list(struct d_context* d_ctx, struct epoll_stream* stream, struct stream_item* stream_item) {
    (void)stream_item;
    relay_devices_list list = {};
    device_arr* devices = &d_ctx->device_ctx->devices;
    if (devices->r.length == 0)
        goto send;
    CHECK(u_array_init(&list.r, sizeof(struct relay_data_list), devices->r.length) == -1);
    for (size_t i = 0; i < devices->r.length; i++) {
        struct device* device = {};
        CHECK(u_array_getr(&devices->r, (void**)&device, i) == -1);
        struct relay_data_list list_entry = {};
        list_entry.grabbed = device->grabbed;
        memcpy(list_entry.dev_name, device->info.name, UINPUT_MAX_NAME_SIZE);
        CHECK(u_array_add(&list.r, &list_entry) == -1);
    }
send:
    const size_t size = list.r.length * list.r.size;
    CHECK(stream_queue_writing_socket(d_ctx->ep_ctx, stream, RELAY_HEADER_LIST, size, list.r.data) == -1);
    CHECK(u_array_free(&list.r) == -1);
    return 0;
err:
    DEBUG_LOG("");
    return -1;
}

int r_handler(struct d_context* d_ctx, struct epoll_stream* stream, struct stream_item* stream_item) {
    switch (stream_item->payload.msg.header.value) {
        case RELAY_HEADER_IDLE: {
            // CHECK(handler_idle(d_ctx, stream, stream_item) == -1);
            break;
        }
        case RELAY_HEADER_START: {
            CHECK(handler_start(d_ctx, stream, stream_item) == -1);
            break;
        }
        case RELAY_HEADER_PAUSE: {
            CHECK(handler_pause(d_ctx, stream, stream_item) == -1);
            break;
        }
        case RELAY_HEADER_RESUME: {
            CHECK(handler_resume(d_ctx, stream, stream_item) == -1);
            break;
        }
        case RELAY_HEADER_STOP: {
            CHECK(handler_stop(d_ctx, stream, stream_item) == -1);
            break;
        }
        case RELAY_HEADER_LIST: {
            CHECK(handler_list(d_ctx, stream, stream_item) == -1);
            break;
        }
        default: ERR_GOTO(err, "err: unknown relay header '%d'\n", stream_item->payload.msg.header.value);
    }
    return 0;
err:
    DEBUG_LOG("");
    return -1;
}

int r_init_relay_ctx(struct epoll_context* ep_ctx, struct relay_context* r_ctx, int usock_fd) {
    r_ctx->usock_fd = usock_fd;
    CHECK(u_array_init(&r_ctx->relay_streams.r, sizeof(struct relay*), RCN_STD_CAPACITY) == -1);
    CHECK(e_epoll_add(ep_ctx, usock_fd, FD_USOCK) == -1);
    return 0;
err:
    DEBUG_LOG("");
    return -1;
}

int r_close_relay_ctx(struct epoll_context* ep_ctx, struct relay_context* r_ctx) {
    // do not close usock_fd, its closed in close_epoll_ctx
    for (size_t i = 0; i < r_ctx->relay_streams.r.length; i++) {
        struct epoll_stream* r_stream = {};
        CHECK(u_array_getr(&r_ctx->relay_streams.r, (void**)&r_stream, i) == -1);
        CHECK(e_epoll_close_remove_simple(ep_ctx, r_stream) == -1);
    }
    CHECK(u_array_free(&r_ctx->relay_streams.r) == -1);
    return 0;
err:
    DEBUG_LOG("");
    return -1;
}