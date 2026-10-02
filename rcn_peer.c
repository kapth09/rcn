#include "include/rcn.h"
#include "include/rcn_daemon.h"
#include "include/rcn_epoll.h"
#include "include/rcn_peer.h"
#include "include/rcn_stream.h"
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

static int handler_dev_crt(struct d_context* d_ctx, struct epoll_stream* stream, struct stream_item* stream_item) {
    (void)stream;
    struct device_info* new_dev = stream_item->payload.msg.buffer;
    CHECK(dev_init_udev(d_ctx->ep_ctx, &d_ctx->device_ctx->devices, new_dev) == -1);
    return 0;
err:
    ERR_LOG("");
    return -1;
}

static int handler_dev_upd_grab(struct d_context* d_ctx, struct epoll_stream* stream, struct stream_item* stream_item) {
    (void)stream;
    struct peer_msg_dev_upd_grab* msg = stream_item->payload.msg.buffer;
    struct device* dev = NULL;
    CHECK(dev_find_by_id(&d_ctx->device_ctx->devices, msg->random_id, &dev) == -1);
    dev->grabbed = msg->grab;
    return 0;
err:
    ERR_LOG("");
    return -1;
}

static int handler_event(struct d_context* d_ctx, struct epoll_stream* stream, struct stream_item* stream_item) {
    (void)stream;
    struct peer_msg_event msg = *(struct peer_msg_event*)stream_item->payload.msg.buffer;
    CHECK(dev_emit_event_msg(d_ctx, msg) == -1);
    return 0;
err:
    ERR_LOG("");
    return -1;
}

static int handler_pause(struct d_context* d_ctx, struct epoll_stream* stream, struct stream_item* stream_item) {
    (void)d_ctx;
    (void)stream;
    (void)stream_item;
    if (d_ctx->type == DAEMON_CLIENT)
        CHECK(dev_ctrl_devices(d_ctx->ep_ctx, d_ctx->peer_ctx, &d_ctx->device_ctx->devices, DEV_CTRL_RELEASE) == -1);
    else if (d_ctx->type == DAEMON_SERVER)
        CHECK(dev_release_virt_keys_all(d_ctx->ep_ctx, &d_ctx->device_ctx->devices) == -1);
    epoll_stream_arr* relay_streams = &d_ctx->relay_ctx->relay_streams;
    CHECK(r_broadcast_relay_header(d_ctx->ep_ctx, relay_streams, RELAY_HEADER_PAUSE) == -1);
    d_ctx->state = RCN_PAUSED;
    return 0;
err:
    ERR_LOG("");
    return -1;
}
static int handler_resume(struct d_context* d_ctx, struct epoll_stream* stream, struct stream_item* stream_item) {
    (void)d_ctx;
    (void)stream;
    (void)stream_item;
    if (d_ctx->type == DAEMON_CLIENT)
        CHECK(dev_ctrl_devices(d_ctx->ep_ctx, d_ctx->peer_ctx, &d_ctx->device_ctx->devices, DEV_CTRL_CAPTURE) == -1);
    else if (d_ctx->type == DAEMON_SERVER)
        CHECK(dev_release_virt_keys_all(d_ctx->ep_ctx, &d_ctx->device_ctx->devices) == -1);
    epoll_stream_arr* relay_streams = &d_ctx->relay_ctx->relay_streams;
    CHECK(r_broadcast_relay_header(d_ctx->ep_ctx, relay_streams, RELAY_HEADER_RESUME) == -1);
    d_ctx->state = RCN_RUNNING;
    return 0;
err:
    ERR_LOG("");
    return -1;
}
static int handler_stop(struct d_context* d_ctx, struct epoll_stream* stream, struct stream_item* stream_item) {
    (void)d_ctx;
    (void)stream;
    (void)stream_item;
    if (d_ctx->type == DAEMON_SERVER) {
        CHECK(dev_close_device_ctx(d_ctx->ep_ctx, d_ctx->device_ctx) == -1);
        CHECK(dev_init_device_ctx(d_ctx, NULL, DAEMON_SERVER) == -1);
        d_ctx->peer_ctx->isock_stream->fd = TRY(d_init_inet_sock(d_ctx->peer_ctx->isock_port), -1);
        CHECK(p_init_peer_ctx(d_ctx->ep_ctx, d_ctx->peer_ctx, d_ctx->peer_ctx->isock_stream->fd, d_ctx->peer_ctx->isock_port, DAEMON_SERVER) == -1);
        d_ctx->state = RCN_RUNNING;
    } else if (d_ctx->type == DAEMON_CLIENT) {
        d_ctx->exit = true;
        epoll_stream_arr* relay_streams = &d_ctx->relay_ctx->relay_streams;
        CHECK(r_broadcast_relay_header(d_ctx->ep_ctx, relay_streams, RELAY_HEADER_STOP) == -1);
    }
    return 0;
err:
    ERR_LOG("");
    return -1;
}

int p_handler(struct d_context* d_ctx, struct epoll_stream* stream, struct stream_item* stream_item) {
    switch (stream_item->payload.msg.header.value) {
        case PEER_HEADER_DEV_CRT: {
            CHECK(handler_dev_crt(d_ctx, stream, stream_item) == -1);
            break;
        }
        case PEER_HEADER_DEV_UPD_GRAB: {
            CHECK(handler_dev_upd_grab(d_ctx, stream, stream_item) == -1);
            break;
        }
        case PEER_HEADER_EVENT: {
            CHECK(handler_event(d_ctx, stream, stream_item) == -1);
            break;
        }
        case PEER_HEADER_PAUSE: {
            CHECK(handler_pause(d_ctx, stream, stream_item) == -1);
            break;
        }
        case PEER_HEADER_RESUME: {
            CHECK(handler_resume(d_ctx, stream, stream_item) == -1);
            break;
        }
        case PEER_HEADER_STOP: {
            CHECK(handler_stop(d_ctx, stream, stream_item) == -1);
            break;
        }
        default: ERR_GOTO(err, "err: unknown peer header '%d'\n", stream_item->payload.msg.header.value);
    }
    return 0;
err:
    ERR_LOG("");
    return -1;
}

int p_init_peer_ctx(struct epoll_context* ep_ctx, struct peer_context* p_ctx, int isock_fd, int port, enum daemon_type d_type) {
    p_ctx->isock_port = port;
    if (d_type == DAEMON_SERVER) {
        p_ctx->peer_stream = NULL;
        CHECK(e_epoll_add_getr(ep_ctx, isock_fd, FD_ISOCK, &p_ctx->isock_stream) == -1);
        CHECK(stream_set_default(p_ctx->isock_stream, STREAM_OP_READING, STREAM_TYPE_SOCKET) == -1);
        p_ctx->peer_state = PEER_DISCONNECTED;
    } else if (d_type == DAEMON_CLIENT) {
        p_ctx->isock_stream = NULL;
        CHECK(e_epoll_add_getr(ep_ctx, isock_fd, FD_PEER, &p_ctx->peer_stream) == -1);
        CHECK(stream_set_default(p_ctx->peer_stream, STREAM_OP_READING, STREAM_TYPE_SOCKET) == -1);
        p_ctx->peer_state = PEER_CONNECTED;
    }
    return 0;
err:
    ERR_LOG("");
    return -1;
}

int p_close_peer_ctx(struct epoll_context* ep_ctx, struct peer_context* p_ctx) {
    if (p_ctx->peer_stream != NULL && p_ctx->peer_state != PEER_DISCONNECTED) {
        p_ctx->peer_state = PEER_DISCONNECTED;
        CHECK(e_epoll_close_remove_simple(ep_ctx, p_ctx->peer_stream) == -1);
    }
    return 0;
err:
    ERR_LOG("");
    return -1;
}

int p_close_peer(struct epoll_context* ep_ctx, struct peer_context* p_ctx) {
    (void)ep_ctx;
    p_ctx->peer_state = PEER_DISCONNECTED;
    LOG("peer closed connection");
    return 0;
}