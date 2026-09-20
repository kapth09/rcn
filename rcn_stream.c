#include "include/rcn.h"
#include "include/rcn_peer.h"
#include "include/rcn_relay.h"
#include "include/rcn_stream.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int stream_init(struct stream* stream, int fd, enum fd_type type) {
    stream->fd = fd;
    stream->header = -1;
    stream->next = &stream->fallback;
    CHECK(u_queue_init(&stream->queue.r, sizeof(struct stream_data), RCN_STD_CAPACITY) == -1);
    switch (type) {
        case FD_PEER: CHECK(stream_set_default(stream, sizeof(enum peer_msg_header), STREAM_READING) == -1); break;
        case FD_RELAY: CHECK(stream_set_default(stream, sizeof(enum relay_msg_header), STREAM_READING) == -1); break;
        case FD_DEV: CHECK(stream_set_default(stream, sizeof(struct input_event), STREAM_READING) == -1); break;
        case FD_UDEV: CHECK(stream_set_default(stream, sizeof(struct input_event), STREAM_WRITING) == -1); break;
        case FD_USOCK: break;
        case FD_ISOCK: break;
    }
    return 0;
err:
    ERR_LOG("stream_setup");
    return -1;
}

int stream_queue_reading(struct stream* stream, size_t size) {
    struct stream_data stream_data = {
        .state = STREAM_STREAMING,
        .op = STREAM_READING,
        .buffer = TRY(calloc(1, size), NULL),
        .buffer_size = size,
        .buffer_streamed = 0,
    };
    bool was_empty = stream->queue.r.is_empty;
    CHECK(u_queue_push(&stream->queue.r,  &stream_data) == -1);
    if (was_empty)
        CHECK(u_queue_peek(&stream->queue.r, (void**)&stream->next) == -1);
    return 0;
err:
    ERR_LOG("stream_set_reading");
    return -1;
}

int stream_queue_writing(struct stream* stream, size_t size, void* data) {
    struct stream_data stream_data = {
        .state = STREAM_STREAMING,
        .op = STREAM_WRITING,
        .buffer = TRY(calloc(1, size), NULL),
        .buffer_size = size,
        .buffer_streamed = 0,
    };
    memcpy(stream_data.buffer, data, size);
    bool was_empty = stream->queue.r.is_empty;
    CHECK(u_queue_push(&stream->queue.r, &stream_data) == -1);
    if (was_empty)
        CHECK(u_queue_peek(&stream->queue.r, (void**)&stream->next) == -1);
    return 0;
err:
    ERR_LOG("stream_set_writing");
    return -1;
}

int stream_set_default(struct stream* stream, size_t size, enum stream_operation default_op) {
    struct stream_data* stream_data = &stream->fallback;
    if (stream_data->buffer != NULL && stream_data->buffer_size != size)
        free(stream_data->buffer);
    stream_data->buffer = TRY(calloc(1, size), NULL);
    stream_data->buffer_size = size;
    stream_data->buffer_streamed = 0;
    stream_data->state = STREAM_STREAMING;
    stream_data->op = default_op;
    stream->default_op = default_op;
    stream->header = -1;
    return 0;
err:
    ERR_LOG("stream_set");
    return -1;
}

int stream_clear_fallback(struct stream* stream) {
    CHECK(stream_set_default(stream, stream->fallback.buffer_size, stream->default_op) == -1);
    return 0;
err:
    ERR_LOG("stream_clear");
    return -1;
}

int stream_header(struct stream* stream) {
    struct stream_data* stream_data = stream->next;
    if (stream_data->state != STREAM_COMPLETE)
        ERR_GOTO(err, "err: stream is not complete");
    if (stream->header != -1)
        ERR_GOTO(err, "err: stream header already set");
    if (stream_data->buffer_size != sizeof(stream->header))
        ERR_GOTO(err, "err: streamed header is not size of field 'header'");
    const int header = *(int*)stream_data->buffer;
    stream->header = header;
    return 0;
err:
    ERR_LOG("stream_header");
    return -1;
}

int stream(struct stream* stream) {
    struct stream_data* stream_data = stream->next;
    uintptr_t buffer_offset = (uintptr_t)stream_data->buffer + stream_data->buffer_streamed;
    size_t size_offset = stream_data->buffer_size - stream_data->buffer_streamed;
    ssize_t streamed;
    if (stream_data->op == STREAM_READING)
        streamed = read(stream-> fd, (void*)buffer_offset, size_offset);
    else if (stream_data->op == STREAM_WRITING)
        streamed = write(stream-> fd, (void*)buffer_offset, size_offset);
    else
        ERR_GOTO(err, "err: invalid stream state: %d\n", stream_data->state);
    if (streamed == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        return -1;
    }
    if (streamed == 0 || streamed == -1) {
        stream_data->state = STREAM_CLOSED;
        return 0;
    }
    stream_data->buffer_streamed += streamed;
    CHECK(stream_data->buffer_streamed > stream_data->buffer_size);
    if (stream_data->buffer_streamed == stream_data->buffer_size)
        stream_data->state = STREAM_COMPLETE;
    return 0;
err:
    stream_data->state = STREAM_CLOSED;
    ERR_LOG("stream");
    return -1;
}

int stream_collect(struct stream* stream, struct stream_data** stream_data) {
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

int stream_close(struct stream* stream) {
    while (stream->queue.r.is_empty == false) {
        struct stream_data* stream_data = NULL;
        CHECK(u_queue_pop(&stream->queue.r, stream_data) == -1);
        free(stream_data->buffer);
    }
    CHECK(u_queue_free(&stream->queue.r) == -1);
    free(stream->fallback.buffer);
    close(stream->fd);
    return 0;
err:
    ERR_LOG("stream_close");
    return -1;
}