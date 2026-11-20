#include "include/rcn.h"
#include "include/rcn_arg.h"
#include "include/rcn_daemon.h"
#include "include/rcn_peer.h"
#include "include/rcn_relay.h"
#include "include/rcn_relay_handler.h"
#include "include/rcn_stream.h"
#include "include/rcn_types.h"
#include <string.h>

static int handler_start(struct d_context* d_ctx, struct epoll_stream* stream, struct stream_item* stream_item) {
    (void)stream_item;
    CHECK(stream_queue_writing_socket(d_ctx->ep_ctx, stream, RELAY_HEADER_START, 0, NULL) == -1);
    return 0;
err:
    ERR_LOG("");
    return -1;
}

static int handler_pause(struct d_context* d_ctx, struct epoll_stream* stream, struct stream_item* stream_item) {
    (void)stream_item;
    if (d_ctx->state == RCN_STATE_PAUSED) {
        CHECK(stream_queue_writing_socket(d_ctx->ep_ctx, stream, RELAY_HEADER_PAUSE_AGAIN, 0, NULL) == -1);
        return 0;
    }
    if (d_ctx->type == DAEMON_CLIENT)
        CHECK(dev_ctrl_devices(d_ctx->ep_ctx, d_ctx->peer_ctx, &d_ctx->device_ctx->devices, DEV_CTRL_RELEASE) == -1);
    else if (d_ctx->type == DAEMON_SERVER)
        CHECK(dev_release_virt_keys_all(d_ctx->ep_ctx, &d_ctx->device_ctx->devices) == -1);
    epoll_stream_arr* relay_streams = &d_ctx->relay_ctx->relay_streams;
    if (d_ctx->peer_ctx->peer_state == PEER_CONNECTED) {
        CHECK(r_broadcast_relay_header(d_ctx->ep_ctx, relay_streams, RELAY_HEADER_PAUSE) == -1);
        CHECK(stream_queue_writing_socket(d_ctx->ep_ctx, d_ctx->peer_ctx->peer_stream, PEER_HEADER_PAUSE, 0, NULL) == -1);
        d_ctx->state = RCN_STATE_PAUSED;
    } else {
        CHECK(r_broadcast_relay_header(d_ctx->ep_ctx, relay_streams, RELAY_HEADER_NO_PEER) == -1);
    }
    return 0;
err:
    ERR_LOG("");
    return -1;
}

static int handler_resume(struct d_context* d_ctx, struct epoll_stream* stream, struct stream_item* stream_item) {
    (void)stream_item;
    if (d_ctx->state == RCN_STATE_RUNNING) {
        CHECK(stream_queue_writing_socket(d_ctx->ep_ctx, stream, RELAY_HEADER_RESUME_AGAIN, 0, NULL) == -1);
        return 0;
    }
    if (d_ctx->type == DAEMON_CLIENT)
        CHECK(dev_ctrl_devices(d_ctx->ep_ctx, d_ctx->peer_ctx, &d_ctx->device_ctx->devices, DEV_CTRL_CAPTURE) == -1);
    else if (d_ctx->type == DAEMON_SERVER)
        CHECK(dev_release_virt_keys_all(d_ctx->ep_ctx, &d_ctx->device_ctx->devices) == -1);
    CHECK(stream_queue_writing_socket(d_ctx->ep_ctx, d_ctx->peer_ctx->peer_stream, PEER_HEADER_RESUME, 0, NULL) == -1);
    d_ctx->state = RCN_STATE_RUNNING;
    return 0;
err:
    ERR_LOG("");
    return -1;
}

static int handler_switch(struct d_context* d_ctx, struct epoll_stream* stream, struct stream_item* stream_item) {
    if (d_ctx->state == RCN_STATE_RUNNING) {
        CHECK(handler_pause(d_ctx, stream, stream_item) == -1);
    } else if (d_ctx->state == RCN_STATE_PAUSED) {
        CHECK(handler_resume(d_ctx, stream, stream_item) == -1);
    }
    return 0;
err:
    ERR_LOG("");
    return -1;
}

static int handler_stop(struct d_context* d_ctx, struct epoll_stream* stream, struct stream_item* stream_item) {
    (void)stream_item;
    (void)stream;
    d_ctx->exit = true;
    epoll_stream_arr* relay_streams = &d_ctx->relay_ctx->relay_streams;
    CHECK(r_broadcast_relay_header(d_ctx->ep_ctx, relay_streams, RELAY_HEADER_STOP) == -1);
    if (d_ctx->peer_ctx->peer_state == PEER_CONNECTED)
        CHECK(stream_queue_writing_socket(d_ctx->ep_ctx, d_ctx->peer_ctx->peer_stream, PEER_HEADER_STOP, 0, NULL) == -1);
    return 0;
err:
    ERR_LOG("");
    return -1;
}

static int handler_list(struct d_context* d_ctx, struct epoll_stream* stream, struct stream_item* stream_item) {
    (void)stream_item;
    relay_devices_list list = {};
    device_arr* devices = &d_ctx->device_ctx->devices;
    if (devices->r.length == 0)
        goto send;
    CHECK(u_array_init(&list.r, sizeof(struct relay_data_list), devices->r.length) == -1);
    for (size_t i = 0; i < devices->r.length; i++) {
        struct device* device = {};
        CHECK(u_array_getr(&devices->r, (void**)&device, i) == -1);
        struct relay_data_list list_entry = {};
        list_entry.grabbed = device->grabbed;
        list_entry.random_id = device->info.random_id;
        memcpy(list_entry.dev_name, device->info.name, UINPUT_MAX_NAME_SIZE);
        CHECK(u_array_add(&list.r, &list_entry) == -1);
    }
send:
    const size_t size = list.r.length * list.r.size;
    CHECK(stream_queue_writing_socket(d_ctx->ep_ctx, stream, RELAY_HEADER_LIST, size, list.r.data) == -1);
    CHECK(u_array_free(&list.r) == -1);
    return 0;
err:
    ERR_LOG("");
    return -1;
}

static int dyn_add_dev(struct d_context* d_ctx, char* dev_path) {
    struct device* device = {};
    CHECK_LBL(dev_init_device(d_ctx->ep_ctx, &d_ctx->device_ctx->devices, dev_path, &device) == -1, invalid);
    CHECK(stream_queue_writing_socket(d_ctx->ep_ctx, d_ctx->peer_ctx->peer_stream, PEER_HEADER_DEV_CRT, sizeof(struct device_info), &device->info) == -1);
    if (d_ctx->state == RCN_STATE_PAUSED)
        CHECK(dev_ctrl_device(d_ctx->ep_ctx, d_ctx->peer_ctx, device, DEV_CTRL_RELEASE) == -1);
    else if (d_ctx->state == RCN_STATE_RUNNING)
        CHECK(dev_ctrl_device(d_ctx->ep_ctx, d_ctx->peer_ctx, device, DEV_CTRL_CAPTURE) == -1);
    return 0;
err:
    ERR_LOG("");
    return -1;
invalid:
    ERR_LOG("");
    return -2;
}

static int handler_add_dev(struct d_context* d_ctx, struct epoll_stream* stream, struct stream_item* stream_item) {
    dev_path_arr devices = {};
    u_array_from_stream_shallow(&devices.r, stream_item, sizeof(struct arg_device));
    struct relay_dev_config config_result = {};
    config_result.count_success = 0;
    config_result.count_total = devices.r.length;
    if (d_ctx->type == DAEMON_SERVER) {
        printf("err: server cannot add devices\n");
        CHECK(stream_queue_writing_socket(d_ctx->ep_ctx, stream, RELAY_HEADER_DEV_CONFIG, sizeof(config_result), &config_result) == -1);
        return 0;
    }
    for (size_t i = 0; i < devices.r.length; i++) {
        struct arg_device dev_path = {};
        CHECK(u_array_getv(&devices.r, &dev_path, i) == -1);
        int ret = dyn_add_dev(d_ctx, dev_path.path);
        CHECK(ret == -1);
        if (ret == 0)
            config_result.count_success++;
    }
    CHECK(stream_queue_writing_socket(d_ctx->ep_ctx, stream, RELAY_HEADER_DEV_CONFIG, sizeof(config_result), &config_result) == -1);
    return 0;
err:
    stream_queue_writing_socket(d_ctx->ep_ctx, stream, RELAY_HEADER_DEV_CONFIG, sizeof(config_result), &config_result);
    ERR_LOG("");
    return -1;
}

static int dyn_rm_dev(struct d_context* d_ctx, char* dev_path) {
    uint16_t random_id = 0;
    CHECK_LBL(u_str_to_ushort(dev_path, &random_id) == -1, invalid);
    struct device* device = {};
    CHECK_LBL(dev_find_by_id(&d_ctx->device_ctx->devices, random_id, &device) == -1, invalid);
    CHECK(dev_close_device(d_ctx->ep_ctx, d_ctx->device_ctx, device) == -1);
    CHECK(stream_queue_writing_socket(d_ctx->ep_ctx, d_ctx->peer_ctx->peer_stream, PEER_HEADER_DEV_DEL, sizeof(random_id), &random_id) == -1);
    return 0;
err:
    ERR_LOG("");
    return -1;
invalid:
    ERR_LOG("");
    return -2;
}

static int handler_rm_dev(struct d_context* d_ctx, struct epoll_stream* stream, struct stream_item* stream_item) {
    dev_path_arr devices = {};
    u_array_from_stream_shallow(&devices.r, stream_item, sizeof(struct arg_device));
    struct relay_dev_config config_result = {};
    config_result.count_success = 0;
    config_result.count_total = devices.r.length;
    if (d_ctx->type == DAEMON_SERVER) {
        printf("err: server cannot remove devices\n");
        CHECK(stream_queue_writing_socket(d_ctx->ep_ctx, stream, RELAY_HEADER_DEV_CONFIG, sizeof(config_result), &config_result) == -1);
        return 0;
    }
    for (size_t i = 0; i < devices.r.length; i++) {
        struct arg_device device = {};
        CHECK(u_array_getv(&devices.r, &device, i) == -1);
        int ret = dyn_rm_dev(d_ctx, device.path);
        CHECK(ret == -1);
        if (ret == 0)
            config_result.count_success++;
    }
    CHECK(stream_queue_writing_socket(d_ctx->ep_ctx, stream, RELAY_HEADER_DEV_CONFIG, sizeof(config_result), &config_result) == -1);
    return 0;
err:
    stream_queue_writing_socket(d_ctx->ep_ctx, stream, RELAY_HEADER_DEV_CONFIG, sizeof(config_result), &config_result);
    ERR_LOG("");
    return -1;
}

static int dyn_mod_dev(struct d_context* d_ctx, char* dev_path) {
    uint16_t random_id = 0;
    CHECK_LBL(u_str_to_ushort(dev_path, &random_id) == -1, invalid);
    struct device* device = {};
    CHECK_LBL(dev_find_by_id(&d_ctx->device_ctx->devices, random_id, &device) == -1, invalid);
    enum device_ctrl ctrl = {};
    if (device->grabbed == true)
        ctrl = DEV_CTRL_RELEASE;
    else if (device->grabbed == false)
        ctrl = DEV_CTRL_CAPTURE;
    CHECK(dev_ctrl_device(d_ctx->ep_ctx, d_ctx->peer_ctx, device, ctrl) == -1);
    return 0;
err:
    ERR_LOG("");
    return -1;
invalid:
    ERR_LOG("");
    return -2;
}

static int handler_mod_dev(struct d_context* d_ctx, struct epoll_stream* stream, struct stream_item* stream_item) {
    dev_path_arr devices = {};
    u_array_from_stream_shallow(&devices.r, stream_item, sizeof(struct arg_device));
    struct relay_dev_config config_result = {};
    config_result.count_success = 0;
    config_result.count_total = devices.r.length;
    if (d_ctx->type == DAEMON_SERVER) {
        printf("err: server cannot modify devices\n");
        CHECK(stream_queue_writing_socket(d_ctx->ep_ctx, stream, RELAY_HEADER_DEV_CONFIG, sizeof(config_result), &config_result) == -1); return 0;
    }
    for (size_t i = 0; i < devices.r.length; i++) {
        struct arg_device dev_path = {};
        CHECK(u_array_getv(&devices.r, &dev_path, i) == -1);
        int ret = dyn_mod_dev(d_ctx, dev_path.path);
        CHECK(ret == -1);
        if (ret == 0)
            config_result.count_success++;
    }
    CHECK(stream_queue_writing_socket(d_ctx->ep_ctx, stream, RELAY_HEADER_DEV_CONFIG, sizeof(config_result), &config_result) == -1);
    return 0;
err:
    ERR_LOG("");
    stream_queue_writing_socket(d_ctx->ep_ctx, stream, RELAY_HEADER_DEV_CONFIG, sizeof(config_result), &config_result);
    return -1;
}

int relay_handler(struct d_context* d_ctx, struct epoll_stream* stream, struct stream_item* stream_item) {
    switch (stream_item->payload.msg.header.value) {
        case RELAY_HEADER_IDLE:
            // do nothing so the relay hangs on read()
            break;
        case RELAY_HEADER_START:
            CHECK(handler_start(d_ctx, stream, stream_item) == -1);
            break;
        case RELAY_HEADER_PAUSE:
            CHECK(handler_pause(d_ctx, stream, stream_item) == -1);
            break;
        case RELAY_HEADER_RESUME:
            CHECK(handler_resume(d_ctx, stream, stream_item) == -1);
            break;
        case RELAY_HEADER_STOP:
            CHECK(handler_stop(d_ctx, stream, stream_item) == -1);
            break;
        case RELAY_HEADER_SWITCH:
            CHECK(handler_switch(d_ctx, stream, stream_item) == -1);
            break;
        case RELAY_HEADER_LIST:
            CHECK(handler_list(d_ctx, stream, stream_item) == -1);
            break;
        case RELAY_HEADER_ADD_DEV:
            CHECK(handler_add_dev(d_ctx, stream, stream_item) == -1);
            break;
        case RELAY_HEADER_RM_DEV:
            CHECK(handler_rm_dev(d_ctx, stream, stream_item) == -1);
            break;
        case RELAY_HEADER_MOD_DEV:
            CHECK(handler_mod_dev(d_ctx, stream, stream_item) == -1);
            break;
        default: ERR_GOTO(err, "err: unknown relay header '%d'\n", stream_item->payload.msg.header.value);
    }
    return 0;
err:
    ERR_LOG("");
    return -1;
}
