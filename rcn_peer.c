#include "include/rcn.h"
#include "include/rcn_peer.h"

#include <string.h>
#include <unistd.h>

// return the expected size of the next message, SIZE_MAX on error
size_t p_msg_size(enum peer_msg_type msg_type) {
    switch (msg_type) {
        case PEER_MSG_DEV_CRT: return sizeof(struct peer_msg_dev_crt);
        case PEER_MSG_DEV_DEL: return sizeof(struct peer_msg_dev_del);
        case PEER_MSG_EVENT: return sizeof(struct peer_msg_event);
        // reset for a new message
        case PEER_MSG_IDLE:
        case PEER_MSG_PAUSE:
        case PEER_MSG_RESUME:
        case PEER_MSG_STOP: return sizeof(enum peer_msg_type);
        default: goto err;
    }
err:
    ERR_LOG("p_msg_size");
    return SIZE_MAX;
}