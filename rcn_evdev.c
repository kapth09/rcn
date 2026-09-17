#include "include/rcn.h"
#include "include/rcn_daemon.h"
#include "include/rcn_evdev.h"
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/random.h>

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
    ERR_LOG("has_active_key");
    return -1;
}

static int has_any_active_inputs(struct device_info* device) {
    if (HAS_BIT(device->evtbit, EV_KEY)) {
        if (TRY(has_active_key(device->entry->fd), -1) == 1)
            return 1;
    }
    return 0;
err:
    ERR_LOG("has_any_active_inputs");
    return -1;
}

static int drain_events(struct device_info* dev) {
    for (;;) {
        usleep(1000);
        int has = TRY(has_any_active_inputs(dev), -1);
        if (has == 0)
            break;
    }
    return 0;
err:
    ERR_LOG("drain_events");
    return -1;
}

static int find_device(struct u_array* devices, struct device_info** out_device, size_t random_it) {
    for (size_t i = 0; i < devices->length; i++) {
        CHECK(u_array_getr(devices, (void**)out_device, i) == -1);
        if ((*out_device)->random_id== random_it) {
            return 0;
        }
    }
err:
    out_device = NULL;
    ERR_LOG("find_device");
    return -1;
}

int e_init_device(struct epoll_context* ep_ctx, device_info_arr* devices, const char *dev_path, struct device_info* out_dev) {
    int dev_fd = TRY(open(dev_path, O_RDONLY | O_NONBLOCK), -1);
    CHECK(e_get_device_info(dev_fd, out_dev) == -1);
    CHECK(u_array_add(&devices->r, out_dev) == -1);
    CHECK(d_epoll_add_device(ep_ctx, out_dev) == -1);
    return 0;
err:
    out_dev = NULL;
    ERR_LOG("client init_dev");
    return -1;
}

int e_grab_device_by_id(device_info_arr* devices, size_t random_id, bool grab) {
    struct device_info* dev;
    CHECK(find_device(&devices->r, &dev,random_id) == -1);
    CHECK(drain_events(dev) == -1);
    // CHECK(ioctl(dev.entry->fd, EVIOCGRAB, &grab) == -1);
    return 0;
err:
    ERR_LOG("grab_dev_by_id");
    return -1;
}

int e_grab_device_by_ptr(struct device_info* dev, bool grab) {
    CHECK(drain_events(dev) == -1);
    // CHECK(ioctl(dev->entry->fd, EVIOCGRAB, &grab) == -1);
    return 0;
err:
    ERR_LOG("grab_dev_by_ptr");
    return -1;
}

int e_get_device_info(int dev_fd, struct device_info* dev) {
    memset(dev, 0, sizeof(struct device_info));
    dev->fd = dev_fd;
    CHECK(getrandom(&dev->random_id, sizeof(dev->random_id), 0) == -1);
    CHECK(ioctl(dev_fd, EVIOCGID, &dev->dev_id) == -1);
    CHECK(ioctl(dev_fd, EVIOCGNAME(sizeof(dev->name)-1), dev->name) == -1);
    CHECK(ioctl(dev_fd, EVIOCGBIT(0, sizeof(dev->evtbit)), dev->evtbit) == -1);
    CHECK(ioctl(dev_fd, EVIOCGBIT(EV_KEY, sizeof(dev->keybit)), dev->keybit) == -1);
    CHECK(ioctl(dev_fd, EVIOCGBIT(EV_ABS, sizeof(dev->absbit)), dev->absbit) == -1);
    CHECK(ioctl(dev_fd, EVIOCGBIT(EV_REL, sizeof(dev->relbit)), dev->relbit) == -1);
    CHECK(ioctl(dev_fd, EVIOCGPROP(sizeof(dev->propbit)), dev->propbit) == -1);
    if (!HAS_BIT(dev->evtbit, EV_ABS))
        return 0;
    for (int i = 0; i < ABS_MAX; i++) {
        if (!HAS_BIT(dev->absbit, i))
            continue;
        CHECK(ioctl(dev_fd, EVIOCGABS(i), &dev->absinfo[i]) == -1);
    }
    return 0;
err:
    ERR_LOG("e_get_device_info");
    return -1;
}

static int set_udev_bits(int u_fd, unsigned long set_ioctl, uint8_t* bitmap, int max) {
    for (int i = 0; i < max; i++) {
        if (HAS_BIT(bitmap, i))
            CHECK(ioctl(u_fd, set_ioctl, i) == -1);
    }
    return 0;
err:
    ERR_LOG("set_udev_bits");
    return -1;
}

int e_create_udev(struct epoll_context* ep_ctx, device_info_arr* devices, struct device_info* new_dev) {
    int u_fd = -1;
    u_fd = TRY(open("/dev/uinput", O_WRONLY | O_NONBLOCK), -1);
    CHECK(ioctl(u_fd, UI_SET_EVBIT, EV_SYN) == -1);
    CHECK(set_udev_bits(u_fd, UI_SET_PROPBIT, new_dev->propbit, INPUT_PROP_MAX) == -1);
    if (HAS_BIT(new_dev->evtbit, EV_KEY)) {
        CHECK(ioctl(u_fd, UI_SET_EVBIT, EV_KEY) == -1);
        CHECK(set_udev_bits(u_fd, UI_SET_KEYBIT, new_dev->keybit, KEY_MAX) == -1);
    }
    if (HAS_BIT(new_dev->evtbit, EV_REL)) {
        CHECK(ioctl(u_fd, UI_SET_EVBIT, EV_REL) == -1);
        CHECK(set_udev_bits(u_fd, UI_SET_RELBIT, new_dev->relbit, REL_MAX) == -1);
    }
    if (HAS_BIT(new_dev->evtbit, EV_ABS)) {
        CHECK(ioctl(u_fd, UI_SET_EVBIT, EV_ABS) == -1);
        for (int i = 0; i < ABS_MAX; i++) {
            if (!HAS_BIT(new_dev->absbit, i))
                continue;
            CHECK(ioctl(u_fd, UI_SET_ABSBIT, i) == -1);
            struct uinput_abs_setup abs_setup = { 0 };
            abs_setup.code = i;
            abs_setup.absinfo = new_dev->absinfo[i];
            CHECK(ioctl(u_fd, UI_ABS_SETUP, &abs_setup) == -1);
        }
    }
    new_dev->fd = u_fd;
    struct uinput_setup setup = { .id = new_dev->dev_id };
    char tmp_buff[UINPUT_MAX_NAME_SIZE*2];
    snprintf(tmp_buff, sizeof(tmp_buff), "(rcn-virt) %s", new_dev->name);
    strncpy(setup.name, tmp_buff, UINPUT_MAX_NAME_SIZE);
    CHECK(ioctl(u_fd, UI_DEV_SETUP, &setup) == -1);
    CHECK(ioctl(u_fd, UI_DEV_CREATE) == -1);
    CHECK(u_array_add(&devices->r, new_dev) == -1);
    // CHECK(d_epoll_add_device(ep_ctx, new_dev) == -1);
    return 0;
err:
    if (u_fd != -1)
        close(u_fd);
    ERR_LOG("e_create_udev");
    return -1;
}