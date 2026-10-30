#include "include/rcn.h"
#include "include/rcn_epoll.h"
#include "include/rcn_peer.h"
#include "include/rcn_stream.h"
#include <string.h>

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