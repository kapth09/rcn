#include "include/rcn.h"
#include "include/rcn_peer.h"
#include "include/rcn_relay.h"
#include "include/rcn_stream.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int stream_setup(struct stream* stream, int fd, enum fd_type type) {
    stream->fd = fd;
    stream->header = -1;
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

int stream_set_reading(struct stream* stream, size_t size) {
    if (stream->buffer != NULL && stream->buffer_size != size)
        free(stream->buffer);
    stream->buffer = TRY(calloc(1, size), NULL);
    stream->buffer_size = size;
    stream->buffer_streamed = 0;
    stream->state = STREAM_STREAMING;
    stream->op = STREAM_READING;
    stream->header = -1;
    return 0;
err:
    ERR_LOG("stream_set_reading");
    return -1;
}

int stream_set_writing(struct stream* stream, size_t size, void* data) {
    if (stream->buffer != NULL && stream->buffer_size != size)
        free(stream->buffer);
    stream->buffer = TRY(calloc(1, size), NULL);
    memcpy(stream->buffer, data, size);
    stream->buffer_size = size;
    stream->buffer_streamed = 0;
    stream->state = STREAM_STREAMING;
    stream->op = STREAM_WRITING;
    stream->header = -1;
    return 0;
err:
    ERR_LOG("stream_set_writing");
    return -1;
}

int stream_set_default(struct stream* stream, size_t size, enum stream_operation default_op) {
    if (stream->buffer != NULL && stream->buffer_size != size)
        free(stream->buffer);
    stream->buffer = TRY(calloc(1, size), NULL);
    stream->buffer_size = size;
    stream->buffer_streamed = 0;
    stream->state = STREAM_STREAMING;
    stream->op = default_op;
    stream->default_op = default_op;
    stream->header = -1;
    return 0;
err:
    ERR_LOG("stream_set");
    return -1;
}

int stream_clear(struct stream* stream) {
    CHECK(stream_set_default(stream, stream->buffer_size, stream->default_op) == -1);
    return 0;
err:
    ERR_LOG("stream_clear");
    return -1;
}

int stream_header(struct stream* stream) {
    if (stream->state != STREAM_COMPLETE)
        ERR_GOTO(err, "err: stream is not complete");
    if (stream->header != -1)
        ERR_GOTO(err, "err: stream header already set");
    if (stream->buffer_size != sizeof(stream->header))
        ERR_GOTO(err, "err: streamed header is not size of field 'header'");
    const int header = *(int*)stream->buffer;
    stream->header = header;
    return 0;
err:
    ERR_LOG("stream_header");
    return -1;
}

int stream(struct stream* stream) {
    uintptr_t buffer_offset = (uintptr_t)stream->buffer + stream->buffer_streamed;
    size_t size_offset = stream->buffer_size - stream->buffer_streamed;
    ssize_t streamed;
    if (stream->op == STREAM_READING)
        streamed = TRY(read(stream-> fd, (void*)buffer_offset, size_offset), -1);
    else if (stream->op == STREAM_WRITING)
        streamed = TRY(write(stream-> fd, (void*)buffer_offset, size_offset), -1);
    else
        ERR_GOTO(err, "err: invalid stream state: %d\n", stream->state);
    CHECK(streamed == -1 && (errno != EAGAIN && errno != EWOULDBLOCK));
    stream->buffer_streamed += streamed;
    CHECK(stream->buffer_streamed > stream->buffer_size);
    if (streamed == 0)
        stream->state = STREAM_CLOSED;
    if (stream->buffer_streamed == stream->buffer_size)
        stream->state = STREAM_COMPLETE;
    return 0;
err:
    stream->state = STREAM_CLOSED;
    ERR_LOG("stream");
    return -1;
}
