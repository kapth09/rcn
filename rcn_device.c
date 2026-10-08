#include "include/rcn.h"
#include "include/rcn_daemon.h"
#include "include/rcn_device.h"
#include "include/rcn_epoll.h"
#include "include/rcn_peer.h"
#include "include/rcn_stream.h"
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/random.h>
#include <unistd.h>

const int device_evt_val_arr[] = {
    [DEVICE_EVT_KEY_INDEX] = DEVICE_EVT_KEY,
    [DEVICE_EVT_REL_INDEX] = DEVICE_EVT_REL,
    [DEVICE_EVT_ABS_INDEX] = DEVICE_EVT_ABS,
    [DEVICE_EVT_SWT_INDEX] = DEVICE_EVT_SWT,
};
const char* device_evt_str_arr[DEVICE_EVT_COUNT_INDEX] = {
    [DEVICE_EVT_KEY_INDEX] = DEVICE_EVT_KEY_STR,
    [DEVICE_EVT_REL_INDEX] = DEVICE_EVT_REL_STR,
    [DEVICE_EVT_ABS_INDEX] = DEVICE_EVT_ABS_STR,
    [DEVICE_EVT_SWT_INDEX] = DEVICE_EVT_SWT_STR,
};
const int device_evt_str_length = sizeof(device_evt_val_arr) / sizeof(device_evt_val_arr[0]);

static int has_active_key(int dev_fd) {
    if (dev_fd <= 0)
        DO_GOTO(fprintf(stderr, "err: dev_fd is <= 0\n"), err);
    uint8_t keybits[MAX_KEY_BYTES] = { 0 };
    CHECK(ioctl(dev_fd, EVIOCGKEY(sizeof(keybits)), keybits) == -1);
    for (int i = 0; i < (int)sizeof(keybits); i++) {
        if (keybits[i] != 0)
            return 1;
    }
    return 0;
err:
    ERR_LOG("");
    return -1;
}

static int has_any_active_inputs(struct device* device) {
    if (HAS_BIT(device->info.evtbit, EV_KEY)) {
        if (TRY(has_active_key(device->stream->fd), -1) == 1)
            return 1;
    }
    return 0;
err:
    ERR_LOG("");
    return -1;
}

static int drain_events(struct device* dev) {
    for (;;) {
        usleep(1000);
        int has = TRY(has_any_active_inputs(dev), -1);
        if (has == 0)
            break;
    }
    return 0;
err:
    ERR_LOG("");
    return -1;
}

static int filter_dev_set_info(uint8_t* evt_bits, enum event_types filter, enum event_types type, int evt, enum event_types* out) {
    bool has_bit = HAS_BIT(evt_bits, evt);
    if (has_bit)
        *out |= type;
    if (!has_bit && (filter & type))
        return -1;
    return 0;
}

int dev_get_all_devices(basic_dev_info_arr* dev_binfos, enum event_types filter) {
    DIR* dir = TRY(opendir(RCN_DEV_EVENT_PATH), NULL);
    errno = 0;
    const size_t input_path_len = strlen(RCN_DEV_EVENT_PATH);
    for (;;) {
        struct dirent* ent = readdir(dir);
        if (ent == NULL) {
            CHECK(errno != 0); // if errno does not equal 0, readdir failed, else end of directory is reached
            break;
        }
        if (strncmp(ent->d_name, "event", 5) != 0)
            continue;
        size_t event_path_len = input_path_len + strlen(ent->d_name) + 2;
        char event_path[event_path_len];
        snprintf(event_path, event_path_len, "%s/%s", RCN_DEV_EVENT_PATH, ent->d_name);
        const int dev_fd = TRY(open(event_path, O_RDONLY), -1);
        uint8_t evt_bits[MAX_EVT_BYTES] = {};
        CHECK(ioctl(dev_fd, EVIOCGBIT(0, sizeof(evt_bits)), evt_bits) == -1);
        struct basic_device_info binfo = {};
        if (filter_dev_set_info(evt_bits, filter, DEVICE_EVT_KEY, EV_KEY, &binfo.events) == -1)
            continue;
        if (filter_dev_set_info(evt_bits, filter, DEVICE_EVT_REL, EV_REL, &binfo.events) == -1)
            continue;
        if (filter_dev_set_info(evt_bits, filter, DEVICE_EVT_ABS, EV_ABS, &binfo.events) == -1)
            continue;
        if (filter_dev_set_info(evt_bits, filter, DEVICE_EVT_SWT, EV_SW, &binfo.events) == -1)
            continue;
        snprintf(binfo.eventX, sizeof(binfo.eventX), "%s", ent->d_name);
        CHECK(ioctl(dev_fd, EVIOCGNAME(sizeof(binfo.name)), binfo.name) == -1);
        CHECK(u_array_add(&dev_binfos->r, &binfo) == -1);
    }
    closedir(dir);
    return 0;
err:
    ERR_LOG("");
    return -1;
}

int dev_print_basic_info(struct basic_device_info binfo) {
    putchar('[');
    int c = 0;
    for (int i = 0; i < DEVICE_EVT_COUNT_INDEX; i++) {
        if (binfo.events & device_evt_val_arr[i]) {
            if (c > 0)
                printf(",");
            printf("%s", device_evt_str_arr[i]);
            c++;
        }
    }
    putchar(']');
    if (c <= 1)
        putchar('\t');
    printf("\t%s\t\t%s", binfo.eventX, binfo.name);
    return 0;
}

int dev_init_device(struct epoll_context* ep_ctx, device_arr* devices, const char *dev_path, struct device** out_dev) {
    struct device* tmp_dev = TRY(calloc(1, sizeof(struct device)), NULL);
    int dev_fd = TRY(open(dev_path, O_RDONLY | O_NONBLOCK), -1);
    CHECK(e_epoll_add_device(ep_ctx, dev_fd, tmp_dev, FD_DEV) == -1);
    CHECK(dev_get_device_info(dev_fd, tmp_dev) == -1);
    CHECK(u_array_add(&devices->r, tmp_dev) == -1);
    u_safe_free((void**)&tmp_dev);
    CHECK(u_array_getr(&devices->r, (void**)out_dev, devices->r.length-1) == -1);
    LOG("init device '%s' (%s)", (*out_dev)->info.name, dev_path);
    return 0;
err:
    *out_dev = NULL;
    ERR_LOG("");
    return -1;
}

int dev_init_device_ctx(struct d_context* d_ctx, char_arr* devices_arg, enum daemon_type type) {
    CHECK(u_array_init(&d_ctx->device_ctx->devices.r, sizeof(struct device), RCN_STD_CAPACITY) == -1);
    if (type == DAEMON_CLIENT) {
        CHECK(dev_init_devices_arg(d_ctx->ep_ctx, d_ctx->device_ctx, d_ctx->peer_ctx, devices_arg) == -1);
        CHECK(dev_ctrl_devices(d_ctx->ep_ctx, d_ctx->peer_ctx, &d_ctx->device_ctx->devices, DEV_CTRL_CAPTURE) == -1);
        CHECK(u_array_free(&devices_arg->r) == -1);
    }
    return 0;
err:
    ERR_LOG("");
    return -1;
}

int dev_init_devices_arg(struct epoll_context* ep_ctx, struct device_context* dev_ctx, struct peer_context* p_ctx, char_arr* dev_paths) {
    for (size_t i = 0; i < dev_paths->r.length; i++) {
        char* dev_path = {};
        CHECK(u_array_getv(&dev_paths->r, &dev_path, i) == -1);
        struct device* dev = {};
        CHECK(dev_init_device(ep_ctx, &dev_ctx->devices, dev_path, &dev) == -1);
        CHECK(stream_queue_writing_socket(ep_ctx, p_ctx->peer_stream, PEER_HEADER_DEV_CRT, sizeof(struct device_info), &dev->info) == -1);
    }
    return 0;
err:
    ERR_LOG("");
    return -1;
}

int dev_close_device_ctx(struct epoll_context* ep_ctx, struct device_context* dev_ctx) {
    while (dev_ctx->devices.r.length > 0) {
        struct device* dev = {};
        CHECK(u_array_getr(&dev_ctx->devices.r, (void**)&dev, 0) == -1);
        CHECK(e_epoll_close_remove_simple(ep_ctx, dev->stream) == -1);
        CHECK(u_array_remove(&dev_ctx->devices.r, 0) == -1);
    }
    CHECK(u_array_free(&dev_ctx->devices.r) == -1);
    return 0;
err:
    ERR_LOG("");
    return -1;
}

int dev_grab_device_by_ptr(struct epoll_context* ep_ctx, struct peer_context* p_ctx, struct device* dev, enum device_ctrl ctrl) {
    CHECK(drain_events(dev) == -1);
    CHECK(ioctl(dev->stream->fd, EVIOCGRAB, ctrl == DEV_CTRL_CAPTURE) == -1);
    if (ctrl == DEV_CTRL_CAPTURE) {
        dev->grabbed = true;
        LOG("grabbed device '%s'", dev->info.name);
    } else if (ctrl == DEV_CTRL_RELEASE) {
        dev->grabbed = false;
        LOG("ungrabbed device '%s'", dev->info.name);
    }
    struct peer_msg_dev_upd_grab msg = {};
    msg.random_id = dev->info.random_id;
    msg.grab = dev->grabbed;
    size_t size = sizeof(struct peer_msg_dev_upd_grab);
    CHECK(stream_queue_writing_socket(ep_ctx, p_ctx->peer_stream, PEER_HEADER_DEV_UPD_GRAB, size, &msg) == -1);
    return 0;
err:
    ERR_LOG("");
    return -1;
}

int dev_get_device_info(int dev_fd, struct device* device) {
    struct device_info* info = &device->info;
    CHECK(getrandom(&device->info.random_id, sizeof(device->info.random_id), 0) == -1);
    CHECK(ioctl(dev_fd, EVIOCGID, &info->dev_id) == -1);
    CHECK(ioctl(dev_fd, EVIOCGNAME(sizeof(info->name)-1), info->name) == -1);
    CHECK(ioctl(dev_fd, EVIOCGBIT(0, sizeof(info->evtbit)), info->evtbit) == -1);
    CHECK(ioctl(dev_fd, EVIOCGBIT(EV_KEY, sizeof(info->keybit)), info->keybit) == -1);
    CHECK(ioctl(dev_fd, EVIOCGBIT(EV_ABS, sizeof(info->absbit)), info->absbit) == -1);
    CHECK(ioctl(dev_fd, EVIOCGBIT(EV_REL, sizeof(info->relbit)), info->relbit) == -1);
    CHECK(ioctl(dev_fd, EVIOCGPROP(sizeof(info->propbit)), info->propbit) == -1);
    if (!HAS_BIT(info->evtbit, EV_ABS))
        return 0;
    for (int i = 0; i < ABS_MAX; i++) {
        if (!HAS_BIT(info->absbit, i))
            continue;
        CHECK(ioctl(dev_fd, EVIOCGABS(i), &info->absinfo[i]) == -1);
    }
    return 0;
err:
    ERR_LOG("");
    return -1;
}

static int set_udev_bits(int u_fd, unsigned long set_ioctl, uint8_t* bitmap, int max) {
    for (int i = 0; i < max; i++) {
        if (HAS_BIT(bitmap, i))
            CHECK(ioctl(u_fd, set_ioctl, i) == -1);
    }
    return 0;
err:
    ERR_LOG("");
    return -1;
}

static int apply_udev_info(int u_fd, struct device* device, struct device_info* template) {
    CHECK(ioctl(u_fd, UI_SET_EVBIT, EV_SYN) == -1);
    CHECK(set_udev_bits(u_fd, UI_SET_PROPBIT, template->propbit, INPUT_PROP_MAX) == -1);
    if (HAS_BIT(template->evtbit, EV_KEY)) {
        CHECK(ioctl(u_fd, UI_SET_EVBIT, EV_KEY) == -1);
        CHECK(set_udev_bits(u_fd, UI_SET_KEYBIT, template->keybit, KEY_MAX) == -1);
    }
    if (HAS_BIT(template->evtbit, EV_REL)) {
        CHECK(ioctl(u_fd, UI_SET_EVBIT, EV_REL) == -1);
        CHECK(set_udev_bits(u_fd, UI_SET_RELBIT, template->relbit, REL_MAX) == -1);
    }
    if (HAS_BIT(template->evtbit, EV_ABS)) {
        CHECK(ioctl(u_fd, UI_SET_EVBIT, EV_ABS) == -1);
        for (int i = 0; i < ABS_MAX; i++) {
            if (!HAS_BIT(template->absbit, i))
                continue;
            CHECK(ioctl(u_fd, UI_SET_ABSBIT, i) == -1);
            struct uinput_abs_setup abs_setup = { 0 };
            abs_setup.code = i;
            abs_setup.absinfo = template->absinfo[i];
            CHECK(ioctl(u_fd, UI_ABS_SETUP, &abs_setup) == -1);
        }
    }
    struct uinput_setup setup = {};
    setup.id = template->dev_id;
    char tmp_buff[UINPUT_MAX_NAME_SIZE*2];
    snprintf(tmp_buff, sizeof(tmp_buff), "(rcn-virt) %s", template->name);
    strncpy(setup.name, tmp_buff, UINPUT_MAX_NAME_SIZE);
    strncpy(template->name, tmp_buff, UINPUT_MAX_NAME_SIZE);
    memcpy(&device->info, template, sizeof(struct device_info));
    CHECK(ioctl(u_fd, UI_DEV_SETUP, &setup) == -1);
    CHECK(ioctl(u_fd, UI_DEV_CREATE) == -1);
    return 0;
err:
    ERR_LOG("");
    return -1;
}

int dev_init_udev(struct epoll_context* ep_ctx, device_arr* devices, struct device_info* template) {
    int u_fd = -1;
    u_fd = TRY(open("/dev/uinput", O_WRONLY | O_NONBLOCK), -1);
    struct device* device = TRY(calloc(1, sizeof(struct device)), NULL);
    CHECK(apply_udev_info(u_fd, device, template) == -1);
    CHECK(e_epoll_add_device(ep_ctx, u_fd, device, FD_UDEV) == -1);
    CHECK(u_array_add(&devices->r, device) == -1);
    LOG("recreated device '%s'", template->name);
    return 0;
err:
    if (u_fd != -1)
        close(u_fd);
    ERR_LOG("");
    return -1;
}

int dev_find_by_id(device_arr* devices, size_t random_id, struct device** out_dev) {
    for (size_t i = 0; i < devices->r.length; i++) {
        CHECK(u_array_getr(&devices->r, (void**)out_dev, i) == -1);
        if ((*out_dev)->info.random_id == random_id)
            break;
    }
    return 0;
err:
    ERR_LOG("");
    return -1;
}

int dev_emit_event_msg(struct d_context* d_ctx, struct peer_msg_event event) {
    device_arr* devices = &d_ctx->device_ctx->devices;
    struct device *dev = NULL;
    CHECK(dev_find_by_id(devices, event.random_id, &dev) == -1);
    CHECK(dev == NULL);
    CHECK(stream_queue_writing_device(d_ctx->ep_ctx, dev->stream, event.evt_data) == -1);
    if (event.evt_data.type == EV_REL) {
        struct input_event evt = {};
        evt.type = EV_SYN;
        evt.code = SYN_REPORT;
        evt.value = 0;
        CHECK(stream_queue_writing_device(d_ctx->ep_ctx, dev->stream, evt) == -1);
    }
    return 0;
err:
    ERR_LOG("");
    return -1;
}

int dev_release_virt_keys(struct epoll_context* ep_ctx, struct device* device) {
    struct input_event evt = {};
    if (HAS_BIT(device->info.evtbit, EV_KEY)) {
        for (int i = 0; i < KEY_MAX; i++) {
            if (!HAS_BIT(device->info.keybit, i))
                continue;
            evt.type = EV_KEY;
            evt.code = i;
            evt.value = 0;
            CHECK(stream_queue_writing_device(ep_ctx, device->stream, evt) == -1);
        }
        evt.type = EV_SYN;
        evt.code = SYN_REPORT;
        evt.value = 0;
        CHECK(stream_queue_writing_device(ep_ctx, device->stream, evt) == -1);
    }
    return 0;
err:
    ERR_LOG("");
    return -1;
}

int dev_release_virt_keys_all(struct epoll_context* ep_ctx, device_arr* devices) {
    for (size_t i = 0; i < devices->r.length; i++) {
        struct device* dev = {};
        CHECK(u_array_getr(&devices->r, (void**)&dev, i) == -1);
        CHECK(dev_release_virt_keys(ep_ctx, dev) == -1);
    }
    return 0;
err:
    ERR_LOG("");
    return -1;
}

int dev_close_dev(struct device_context* dev_ctx, struct epoll_stream* stream) {
    size_t index = TRY(u_array_find_index(&dev_ctx->devices.r, &stream), -1);
    CHECK(u_array_remove(&dev_ctx->devices.r, index) == -1);
    return 0;
err:
    ERR_LOG("");
    return -1;
}

int dev_ctrl_devices(struct epoll_context* ep_ctx, struct peer_context* p_ctx, device_arr* devices, enum device_ctrl ctrl) {
    for (size_t i = 0; i < devices->r.length; i++) {
        struct device* dev = {};
        CHECK(u_array_getr(&devices->r, (void**)&dev, i) == -1);
        CHECK(dev_grab_device_by_ptr(ep_ctx, p_ctx, dev, ctrl) == -1);
        struct epoll_event ep_evt = {};
        ep_evt.events = dev->stream->fd_type == FD_DEV ? EPOLLIN : EPOLLOUT;
        ep_evt.data.ptr = dev->stream;
        int res = 0;
        if (ctrl == DEV_CTRL_RELEASE)
            res = epoll_ctl(ep_ctx->epoll_fd, EPOLL_CTL_DEL, dev->stream->fd, &ep_evt);
        else if (ctrl == DEV_CTRL_CAPTURE)
            res = epoll_ctl(ep_ctx->epoll_fd, EPOLL_CTL_ADD, dev->stream->fd, &ep_evt);
        CHECK(res == -1 && errno != EEXIST && errno != ENOENT);
    }
    return 0;
err:
    ERR_LOG("");
    return -1;
}

int dev_handler(struct d_context* d_ctx, struct epoll_stream* stream, struct stream_item* stream_item) {
    bool found_dev = false;
    struct device* dev = {};
    for (size_t i = 0; i < d_ctx->device_ctx->devices.r.length; i++) {
        CHECK(u_array_getr(&d_ctx->device_ctx->devices.r, (void**)&dev, i) == -1);
        if (dev->stream != stream)
            continue;
        found_dev = true;
        break;
    }
    CHECK(found_dev == false);
    struct peer_msg_event msg = {};
    msg.evt_data = stream_item->payload.evt;
    msg.random_id = dev->info.random_id;
    CHECK(stream_queue_writing_socket(d_ctx->ep_ctx, d_ctx->peer_ctx->peer_stream, PEER_HEADER_EVENT, sizeof(struct peer_msg_event), &msg) == -1);
    return 0;
err:
    ERR_LOG("");
    return -1;
}