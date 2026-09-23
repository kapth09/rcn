#include "include/rcn.h"
#include "include/rcn_daemon.h"
#include "include/rcn_peer.h"
#include <string.h>
#include <unistd.h>

#include "include/rcn_epoll.h"
#include "include/rcn_stream.h"

static int handler_dev_crt(struct d_context* d_ctx, struct epoll_stream* stream, struct stream_item* stream_item) {
    (void)d_ctx;
    (void)stream;
    (void)stream_item;
    return 0;
//err:
    ERR_LOG("handler_dev_crt");
    return -1;
}

static int handler_dev_del(struct d_context* d_ctx, struct epoll_stream* stream, struct stream_item* stream_item) {
    (void)d_ctx;
    (void)stream;
    (void)stream_item;
    return 0;
//err:
    ERR_LOG("handler_dev_del");
    return -1;
}

static int handler_event(struct d_context* d_ctx, struct epoll_stream* stream, struct stream_item* stream_item) {
    (void)d_ctx;
    (void)stream;
    (void)stream_item;
    return 0;
//err:
    ERR_LOG("handler_event");
    return -1;
}

static int handler_pause(struct d_context* d_ctx, struct epoll_stream* stream, struct stream_item* stream_item) {
    (void)d_ctx;
    (void)stream;
    (void)stream_item;
    printf("peer pause");
    if (d_ctx->type == DAEMON_CLIENT) {}
    // TODO: ungrab devices
    epoll_stream_arr* relay_streams = &d_ctx->relay_ctx->relay_streams;
    CHECK(r_broadcast_relay_header(d_ctx->ep_ctx, relay_streams, RELAY_HEADER_PAUSE) == -1);
    d_ctx->state = RCN_PAUSED;
    return 0;
err:
    ERR_LOG("handler_pause");
    return -1;
}
static int handler_resume(struct d_context* d_ctx, struct epoll_stream* stream, struct stream_item* stream_item) {
    (void)d_ctx;
    (void)stream;
    (void)stream_item;
    printf("peer pause");
    if (d_ctx->type == DAEMON_CLIENT) {}
    // TODO: regrab devices
    epoll_stream_arr* relay_streams = &d_ctx->relay_ctx->relay_streams;
    CHECK(r_broadcast_relay_header(d_ctx->ep_ctx, relay_streams, RELAY_HEADER_RESUME) == -1);
    d_ctx->state = RCN_RUNNING;
    return 0;
err:
    ERR_LOG("handler_resume");
    return -1;
}
static int handler_stop(struct d_context* d_ctx, struct epoll_stream* stream, struct stream_item* stream_item) {
    (void)d_ctx;
    (void)stream;
    (void)stream_item;
    return 0;
//err:
    ERR_LOG("handler_stop");
    return -1;
}

int p_handler(struct d_context* d_ctx, struct epoll_stream* stream, struct stream_item* stream_item) {
    switch (stream_item->msg.header.value) {
        case PEER_HEADER_DEV_CRT: {
            CHECK(handler_dev_crt(d_ctx, stream, stream_item) == -1);
            break;
        }
        case PEER_HEADER_DEV_DEL: {
            CHECK(handler_dev_del(d_ctx, stream, stream_item) == -1);
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
        default: ERR_GOTO(err, "err: unknown peer header '%d'\n", stream_item->msg.header.value);
    }
    return 0;
err:
    ERR_LOG("p_handler");
    return -1;
}

int p_init_peer_ctx(struct epoll_context* ep_ctx, struct peer_context* p_ctx, int isock_fd, enum daemon_type d_type) {
    if (d_type == DAEMON_SERVER) {
        p_ctx->peer_stream = NULL;
        CHECK(e_epoll_add_getr(ep_ctx, isock_fd, FD_ISOCK, &p_ctx->isock_stream) == -1);
        CHECK(stream_set_default(p_ctx->isock_stream, STREAM_READING) == -1);
    } else if (d_type == DAEMON_CLIENT) {
        p_ctx->isock_stream = NULL;
        CHECK(e_epoll_add_getr(ep_ctx, isock_fd, FD_PEER, &p_ctx->peer_stream) == -1);
        CHECK(stream_set_default(p_ctx->peer_stream, STREAM_READING) == -1);
    }
    return 0;
    err:
        ERR_LOG("d_init_peer_ctx");
    return -1;
}
