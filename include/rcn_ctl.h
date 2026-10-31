#ifndef RCN_RCN_CTL_H
#define RCN_RCN_CTL_H

#include "rcn_relay.h"

struct ctl_arg {
    struct arg devices;
    int evtfd;
    enum relay_msg_header header_sent;
    enum daemon_type d_type;
    bool check_daemon_status;
};

int rcn_ctl(struct ctl_arg arg);

#endif //RCN_RCN_CTL_H
