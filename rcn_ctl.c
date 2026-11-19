#include "include/rcn.h"
#include "include/rcn_ctl.h"
#include "include/rcn_relay.h"
#include "include/rcn_stream.h"
#include "include/rcn_epoll.h"
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/un.h>
#include <sys/epoll.h>

static const char *relay_text[] = {
    [RELAY_HEADER_IDLE] = "",
    [RELAY_HEADER_START] = "started",
    [RELAY_HEADER_PAUSE] = "paused",
    [RELAY_HEADER_PAUSE_AGAIN] = "already paused",
    [RELAY_HEADER_RESUME] = "resumed",
    [RELAY_HEADER_RESUME_AGAIN] = "already resumed",
    [RELAY_HEADER_STOP] = "stopped",
    [RELAY_HEADER_NO_PEER] = "no peer connected",
};

static int request_default(struct stream_item *item, bool* stream_closed) {
    *stream_closed = true;
    LOG("%s", relay_text[item->payload.msg.header.value]);
    return 0;
}

static int request_list(struct stream_item *item, bool* stream_closed) {
    *stream_closed = true;
    if (item->payload.msg.header.size == 0) {
        LOG("no devices captured");
        return 0;
    }
    relay_devices_list list = {};
    u_array_from_stream_shallow(&list.r, item, sizeof(struct relay_data_list));
    for (size_t i = 0; i < list.r.length; i++) {
        struct relay_data_list *list_entry = {};
        CHECK(u_array_getr(&list.r, (void**)&list_entry, i) == -1);
        printf("\rrcn:\t%d\t", list_entry->random_id);
        if (list_entry->grabbed)
            printf("[captured]\t");
        else
            printf("[released]\t");
        printf("'%s'\n", list_entry->dev_name);
    }
    return 0;
err:
    ERR_LOG("");
    return -1;
}

static int request_dev_config(struct stream_item* item, bool* stream_closed) {
    *stream_closed = true;
    CHECK(item->buffer_streamed != sizeof(struct relay_dev_config));
    struct relay_dev_config* dev_config = item->payload.msg.buffer;
    LOG("configured %d/%d devices", dev_config->count_success, dev_config->count_total);
    return 0;
err:
    ERR_LOG("");
    return -1;
}

static void sig_sigint(int sig) {
    (void) sig;
    LOG("got interrupted");
    exit(0);
}

static int handle_request(struct stream_item *item, bool* stream_closed) {
    enum relay_msg_header header = item->payload.msg.header.value;
    switch (header) {
        case RELAY_HEADER_START:
        case RELAY_HEADER_PAUSE:
        case RELAY_HEADER_PAUSE_AGAIN:
        case RELAY_HEADER_RESUME:
        case RELAY_HEADER_RESUME_AGAIN:
        case RELAY_HEADER_NO_PEER:
        case RELAY_HEADER_STOP:
            CHECK(request_default(item, stream_closed) == -1);
            break;
        case RELAY_HEADER_LIST:
            CHECK(request_list(item, stream_closed) == -1);
            break;
        case RELAY_HEADER_DEV_CONFIG:
            CHECK(request_dev_config(item, stream_closed) == -1);
            break;
        default: goto err;
    }
    return 0;
err:
    ERR_LOG("unknown relay header: %d", header);
    return -1;
}

static int init_ctl(struct ctl_arg c_arg, struct epoll_context* ep_ctx) {
    struct relay_arg r_arg = {};
    r_arg.evt_fd = c_arg.evtfd;
    r_arg.sighandler = sig_sigint;
    r_arg.check_daemon = c_arg.check_daemon_status;
    r_arg.d_type = c_arg.d_type;
    struct epoll_stream* stream = TRY(calloc(1, sizeof(struct epoll_stream)), NULL);
    CHECK(relay_start(stream, r_arg) == -1);
    CHECK(e_init_epoll_ctx(ep_ctx) == -1);
    CHECK(e_epoll_add_stream(ep_ctx, stream) == -1);
    struct ctl_payload payload = c_arg.payload;
    CHECK(stream_queue_writing_socket(ep_ctx, stream, payload.relay_header, payload.size, payload.data) == -1);
    printf("\rrcn>");
    fflush(stdout);
    return 0;
err:
    ERR_LOG("");
    return -1;
}

// configure payload in ctl_arg and define when to shutdown stream
int rcn_ctl(struct ctl_arg c_arg) {
    struct epoll_context ep_ctx = {};
    CHECK(init_ctl(c_arg, &ep_ctx) == -1);
    bool stream_closed = false;
    while (stream_closed == false) {
        const size_t epoll_count = ep_ctx.stream_ptrs.r.length;
        struct epoll_event epoll_buff[epoll_count] = {};
        int nfds = TRY(epoll_wait(ep_ctx.epoll_fd, epoll_buff, epoll_count, -1), -1);

        for (int i = 0; i < nfds; i++) {
            struct epoll_event evt = epoll_buff[i];
            struct epoll_stream* stream = evt.data.ptr;
            CHECK(stream_stream(stream) == -1);
            struct stream_item *item = stream->next;
            if (item->state == STREAM_STREAMING_CLOSED)
                stream_closed = true;
            if (item->state != STREAM_STREAMING_COMPLETE)
                continue;
            CHECK(stream_collect(stream, &item) == -1);
            if (item->op == STREAM_OP_READING) {
                CHECK(handle_request(item, &stream_closed) == -1);
            }
            CHECK(e_epoll_reset_stream(&ep_ctx, stream) == -1);
        }
    }
    CHECK(e_close_epoll_ctx(&ep_ctx) == -1);
    return 0;
err:
    ERR_LOG("");
    return -1;
}
