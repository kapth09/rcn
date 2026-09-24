#include "include/rcn.h"
#include "include/rcn_epoll.h"
#include "include/rcn_stream.h"
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

int e_init_epoll_ctx(struct epoll_context* ep_ctx) {
    ep_ctx->epoll_fd = TRY(epoll_create1(0), -1);
    CHECK(u_array_init(&ep_ctx->stream_ptrs.r, sizeof(struct epoll_stream*), RCN_STD_CAPACITY) == -1);
    return 0;
err:
    ERR_LOG("d_init_epoll");
    return -1;
}

int e_close_epoll_ctx(struct epoll_context* ep_ctx) {
    while (ep_ctx->stream_ptrs.r.length > 0) {
        struct epoll_stream* e_stream = {};
        CHECK(u_array_getv(&ep_ctx->stream_ptrs.r, (void**)&e_stream, 0) == -1);
        CHECK(e_epoll_close_remove_simple(ep_ctx, e_stream) == -1);
    }
    CHECK(u_array_free(&ep_ctx->stream_ptrs.r) == -1);
    close(ep_ctx->epoll_fd);
    return 0;
err:
    ERR_LOG("e_close_epoll_ctx");
    return -1;
}

int e_epoll_add_getr(struct epoll_context* ep_ctx, int fd, enum fd_type type, struct epoll_stream** out_stream) {
    struct epoll_stream* stream= TRY(calloc(1, sizeof(struct epoll_stream)), NULL);
    stream->fd_type= type;
    CHECK(stream_init(stream, fd, type) == -1);
    CHECK(u_array_add(&ep_ctx->stream_ptrs.r, &stream) == -1);
    struct epoll_event u_evt = {};
    u_evt.events = EPOLLIN;
    u_evt.data.ptr = stream;
    CHECK(epoll_ctl(ep_ctx->epoll_fd, EPOLL_CTL_ADD, fd, &u_evt) == -1);
    *out_stream = stream;
    return 0;
err:
    ERR_LOG("d_epoll_add");
    return -1;
}

int e_epoll_add(struct epoll_context* ep_ctx, int fd, enum fd_type type) {
    struct epoll_stream* tmp_stream;
    CHECK(e_epoll_add_getr(ep_ctx, fd, type, &tmp_stream) == -1);
    return 0;
err:
    ERR_LOG("d_epoll_add");
    return -1;
}

int e_epoll_add_device(struct epoll_context* ep_ctx, int fd, struct device* device, enum fd_type type) {
    struct epoll_stream* stream = TRY(calloc(1, sizeof(struct epoll_stream)), NULL);
    CHECK(stream_init(stream, fd, type) == -1);
    CHECK(u_array_add(&ep_ctx->stream_ptrs.r, &stream) == -1);
    struct epoll_event u_evt = {};
    u_evt.events = EPOLLIN;
    u_evt.data.ptr = stream;
    CHECK(epoll_ctl(ep_ctx->epoll_fd, EPOLL_CTL_ADD, fd, &u_evt) == -1);
    stream->fd_type= type;
    device->stream = *stream;
    return 0;
err:
    ERR_LOG("d_epoll_add");
    return -1;
}

int e_epoll_reset_stream(struct epoll_context* ep_ctx, struct epoll_stream* stream) {
    CHECK(stream_clear_fallback(stream) == -1);
    CHECK(e_epoll_sync_stream(ep_ctx, stream) == -1);
    return 0;
err:
    ERR_LOG("epoll_reset_stream");
    return -1;
}

int e_epoll_sync_stream(struct epoll_context* ep_ctx, struct epoll_stream* stream) {
    enum EPOLL_EVENTS events = stream->next->op == STREAM_WRITING ? EPOLLOUT : EPOLLIN;
    struct epoll_event evt = {};
    evt.events = events;
    evt.data.ptr = stream;
    CHECK(epoll_ctl(ep_ctx->epoll_fd, EPOLL_CTL_MOD, stream->fd, &evt) == -1);
    return 0;
err:
    ERR_LOG("d_epoll_entry_update");
    return -1;
}

int e_epoll_close_remove(struct d_context* d_ctx, struct epoll_stream* stream) {
    switch (stream->fd_type) {
        case FD_RELAY: r_close_relay(d_ctx->relay_ctx, stream); break;
        case FD_DEV: dev_close_dev(d_ctx->device_ctx, stream); break;
        case FD_PEER: p_close_peer(d_ctx->ep_ctx, d_ctx->peer_ctx); break;
        default: ERR_GOTO(err, "err: unknown fd_type\n");
    }
    CHECK(e_epoll_close_remove_simple(d_ctx->ep_ctx, stream) == -1);
    return 0;
err:
    ERR_LOG("d_epoll_close_remove");
    return -1;
}

int e_epoll_close_remove_simple(struct epoll_context* ep_ctx, struct epoll_stream* stream) {
    CHECK(epoll_ctl(ep_ctx->epoll_fd, EPOLL_CTL_DEL, stream->fd, NULL) == -1);
    size_t index = TRY(u_array_find_index(&ep_ctx->stream_ptrs.r, &stream), -1);
    CHECK(u_array_remove(&ep_ctx->stream_ptrs.r, index) == -1);
    CHECK(stream_close(stream) == -1);
    u_safe_free((void**)&stream);
    return 0;
err:
    close(stream->fd);
    ERR_LOG("e_epoll_close_remove_simple");
    return -1;
}