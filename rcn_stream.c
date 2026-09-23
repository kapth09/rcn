#include "include/rcn.h"
#include "include/rcn_peer.h"
#include "include/rcn_stream.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "include/rcn_daemon.h"
#include "include/rcn_epoll.h"

int stream_init(struct epoll_stream* stream, int fd, enum fd_type type) {
    stream->fd = fd;
    stream->next = &stream->fallback;
    CHECK(u_queue_init(&stream->queue.r, sizeof(struct stream_item), RCN_STD_CAPACITY) == -1);
    switch (type) {
        case FD_PEER: CHECK(stream_set_default(stream, STREAM_READING) == -1); break;
        case FD_RELAY: CHECK(stream_set_default(stream, STREAM_READING) == -1); break;
        case FD_DEV: CHECK(stream_set_default(stream, STREAM_READING) == -1); break;
        case FD_UDEV: CHECK(stream_set_default(stream, STREAM_WRITING) == -1); break;
        case FD_USOCK: break;
        case FD_ISOCK: break;
    }
    return 0;
err:
    ERR_LOG("stream_setup");
    return -1;
}

int stream_queue_reading(struct epoll_stream* stream) {
    struct stream_item stream_data = {};
    stream_data.state = STREAM_STREAMING_HEADER;
    stream_data.op = STREAM_READING;
    stream_data.msg.header = (struct stream_header){};
    stream_data.buffer_streamed = 0;
    bool was_empty = stream->queue.r.is_empty;
    CHECK(u_queue_push(&stream->queue.r,  &stream_data) == -1);
    if (was_empty)
        CHECK(u_queue_peek(&stream->queue.r, (void**)&stream->next) == -1);
    return 0;
err:
    ERR_LOG("stream_set_reading");
    return -1;
}

int stream_queue_writing(struct epoll_context* ep_ctx, struct epoll_stream* stream, int header, size_t size, void* data) {
    struct stream_item stream_data = {};
    stream_data.state = STREAM_STREAMING_HEADER;
    stream_data.op = STREAM_WRITING;
    stream_data.msg.header.value = header;
    stream_data.msg.header.size = size;
    stream_data.buffer_streamed = 0;
    memcpy(stream_data.msg.buffer, data, size);
    bool was_empty = stream->queue.r.is_empty;
    CHECK(u_queue_push(&stream->queue.r, &stream_data) == -1);
    if (was_empty)
        CHECK(u_queue_peek(&stream->queue.r, (void**)&stream->next) == -1);
    CHECK(e_epoll_sync_stream(ep_ctx, stream) == -1);
    return 0;
err:
    ERR_LOG("stream_set_writing");
    return -1;
}

int stream_set_default(struct epoll_stream* stream, enum stream_operation default_op) {
    CHECK(stream == NULL);
    struct stream_item* stream_item = &stream->fallback;
    if (stream_item->msg.buffer != NULL)
        u_safe_free(&stream_item->msg.buffer);
    stream_item->buffer_streamed = 0;
    stream_item->state = STREAM_STREAMING_HEADER;
    stream_item->op = default_op;
    stream->default_op = default_op;
    return 0;
err:
    ERR_LOG("stream_set");
    return -1;
}

int stream_clear_fallback(struct epoll_stream* stream) {
    CHECK(stream_set_default(stream, stream->default_op) == -1);
    return 0;
err:
    ERR_LOG("stream_clear");
    return -1;
}

int stream_stream(struct epoll_stream* stream) {
    struct stream_item* stream_item = stream->next;
    CHECK(stream_item->state == STREAM_COMPLETE);
    // set data info baseline depending on state (header or body)
    size_t data_size = sizeof(struct stream_header);
    void* data_ptr = &stream_item->msg.header;
    if (stream_item->state == STREAM_STREAMING_BODY) {
        if (stream_item->op == STREAM_READING && stream_item->msg.buffer == NULL)
            stream_item->msg.buffer = TRY(calloc(1, stream_item->msg.header.size), NULL);
        data_size = stream_item->msg.header.size;
        data_ptr = stream_item->msg.buffer;
    }
    // calculate buffer offset to send and remaining size independent of state
    void* buffer_offset = data_ptr + stream_item->buffer_streamed;
    size_t size_remaining = data_size - stream_item->buffer_streamed;
    ssize_t streamed = 0;
    // stream actual data
    if (stream_item->op == STREAM_READING)
        streamed = read(stream->fd, buffer_offset, size_remaining);
    else if (stream_item->op == STREAM_WRITING)
        streamed = write(stream->fd, buffer_offset, size_remaining);
    else
        ERR_GOTO(err, "err: invalid stream state: %d\n", stream_item->state);
    // check for errors, ignore EAGAIN/EWOUDLBLOCK and ENODEV if a device is unplugged
    if (streamed == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ENODEV) {
            return 0;
        }
        return -1;
    }
    if (streamed == 0 || streamed == -1) {
        stream_item->state = STREAM_CLOSED;
        return 0;
    }
    // update state after streaming
    stream_item->buffer_streamed += streamed;
    CHECK(stream_item->buffer_streamed > data_size);
    if (stream_item->buffer_streamed == data_size) {
        stream_item->buffer_streamed = 0;
        if (stream_item->state == STREAM_STREAMING_BODY || stream_item->msg.header.size == 0)
            stream_item->state = STREAM_COMPLETE;
        else if (stream_item->state == STREAM_STREAMING_HEADER)
            stream_item->state = STREAM_STREAMING_BODY;
    }
    return 0;
err:
    stream_item->state = STREAM_CLOSED;
    ERR_LOG("stream");
    return -1;
}

int stream_collect(struct epoll_stream* stream, struct stream_item** stream_data) {
    *stream_data = &stream->fallback;
    if (stream->queue.r.is_empty == false) {
        CHECK(u_queue_pop(&stream->queue.r, *stream_data) == -1);
    }
    if (stream->queue.r.is_empty == true)
        stream->next = &stream->fallback;
    else
        CHECK(u_queue_peek(&stream->queue.r, (void**)&stream->next) == -1);
    return 0;
err:
    ERR_LOG("stream_collect");
    return -1;
}

int stream_close(struct epoll_stream* stream) {
    while (stream->queue.r.is_empty == false) {
        struct stream_item* stream_item = NULL;
        CHECK(u_queue_pop(&stream->queue.r, stream_item) == -1);
        if (stream_item->msg.buffer != NULL)
            u_safe_free(&stream_item->msg.buffer);
    }
    CHECK(u_queue_free(&stream->queue.r) == -1);
    if (stream->fallback.msg.buffer != NULL)
        u_safe_free(&stream->fallback.msg.buffer);
    close(stream->fd);
    return 0;
err:
    ERR_LOG("stream_close");
    return -1;
}