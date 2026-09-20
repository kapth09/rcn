#include "include/rcn.h"
#include "include/rcn_peer.h"

#include <string.h>
#include <unistd.h>

// return the expected size of the next message, SIZE_MAX on error
size_t p_msg_size(enum peer_msg_header msg_type) {
    switch (msg_type) {
        case PEER_HEADER_DEV_CRT: return sizeof(struct peer_msg_dev_crt);
        case PEER_HEADER_DEV_DEL: return sizeof(struct peer_msg_dev_del);
        case PEER_HEADER_EVENT: return sizeof(struct peer_msg_event);
        // reset for a new message
        case PEER_HEADER_IDLE:
        case PEER_HEADER_PAUSE:
        case PEER_HEADER_RESUME:
        case PEER_HEADER_STOP: return sizeof(enum peer_msg_header);
        default: goto err;
    }
err:
    ERR_LOG("p_msg_size");
    return SIZE_MAX;
}