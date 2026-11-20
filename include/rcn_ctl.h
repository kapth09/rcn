#ifndef RCN_RCN_CTL_H
#define RCN_RCN_CTL_H

#include "rcn_relay.h"

struct ctl_payload {
    void* data;
    size_t size;
    enum relay_msg_header relay_header;
};

struct ctl_arg {
    struct ctl_payload payload;
    int evtfd;
    enum daemon_type d_type;
    bool check_daemon_status;
};

int rcn_ctl(struct ctl_arg arg);

#endif //RCN_RCN_CTL_H
