#include "include/rcn.h"
#include "include/rcn_daemon.h"
#include "include/rcn_evdev.h"
#include <fcntl.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/random.h>

static int get_slot_count(int dev_fd) {
    struct input_absinfo absinfo = { 0 };
    if (ioctl(dev_fd, EVIOCGABS(ABS_MT_SLOT), &absinfo) == -1)
        return 0;
    int slots = absinfo.maximum - absinfo.minimum;
    if (slots > 0)
        slots++;
    return slots;
}

static int has_active_slot_code(int dev_fd, int slot_count) {
    if (slot_count <= 0)
        return 0;
    const int req_size = sizeof(uint32_t) + (sizeof(int32_t) * slot_count);
    struct {
        uint32_t code;
        int32_t values[];
    } *req = TRY(calloc(1, req_size), NULL);
    req->code = ABS_MT_TRACKING_ID;
    CHECK(ioctl(dev_fd, EVIOCGMTSLOTS(req_size), req) == -1);
    for (int i = 0; i < slot_count; i++) {
        if (req->values[i] > -1) {
            free(req);
            return 1;
        }
    }
    free(req);
    return 0;
err:
    ERR_LOG("has_active_slot_code");
    return -1;
}

static float calc_deadzone(struct input_absinfo absinfo, float expected_idle) {
    float deadzone = absinfo.flat;
    if (deadzone != 0)
        return deadzone;
    int range_up = absinfo.maximum - expected_idle;
    int range_down = expected_idle - absinfo.minimum;
    int travel_range = (range_up > range_down) ? range_up : range_down;
    return deadzone;
}

static int has_active_abs(int dev_fd, uint8_t* absbit, int size) {
    for (int i = 0; i < size; i++) {
        if (!HAS_BIT(absbit, i))
            continue;
        struct input_absinfo absinfo = { 0 };
        CHECK(ioctl(dev_fd, EVIOCGABS(i), &absinfo) == -1);
        float expected_idle;
        if (i == ABS_Z || i == ABS_RZ || i == ABS_BRAKE || i == ABS_GAS)
            expected_idle = absinfo.minimum;
        else
            expected_idle = (absinfo.maximum - absinfo.minimum) / 2;

        float deadzone = absinfo.flat;
        if (deadzone == 0)
            deadzone = expected_idle * 1.05;
        printf("expected: %f\n", expected_idle);
        printf("deadzone: %f\n", deadzone);
        printf("value: %d\n", absinfo.value);

        if (fabs(absinfo.value - expected_idle) > fabsf(deadzone))
            return 1;
    }
    const int slot_count = TRY(get_slot_count(dev_fd), -1);
    if (TRY(has_active_slot_code(dev_fd, slot_count), -1) > 0)
        return 1;
    return 0;
err:
    ERR_LOG("has_active_abs");
    return -1;
}

static int has_active_key(int dev_fd) {
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
        if (TRY(has_active_key(device->dev_fd), -1) == 1)
            return 1;
    }
/* temporarily disabled
    if (HAS_BIT(device->evtbit, EV_ABS)) {
        if (TRY(has_active_abs(device->dev_fd, device->absbit, sizeof(device->absbit)), -1) == 1)
        return 1;
    }
*/
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

static int find_device(struct u_array* devices, struct device_info* out_device, size_t random_it) {
    for (size_t i = 0; i < devices->length; i++) {
        CHECK(u_array_getr(devices, (void**)&out_device, i) == -1);
        if (out_device->random_id == random_it) {
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
    CHECK(d_epoll_add(ep_ctx, dev_fd, FD_DEV) == -1);
    CHECK(e_get_device_info(dev_fd, out_dev) == -1);
    CHECK(u_array_add(&devices->r, out_dev) == -1);
    return 0;
err:
    out_dev = NULL;
    ERR_LOG("client init_dev");
    return -1;
}

int e_grab_device_by_id(device_info_arr* devices, size_t random_id, bool grab) {
    struct device_info dev = { 0 };
    CHECK(find_device(&devices->r, &dev,random_id) == -1);
    CHECK(drain_events(&dev) == -1);
    CHECK(ioctl(dev.dev_fd, EVIOCGRAB, &grab) == -1);
    return 0;
err:
    ERR_LOG("grab_dev_by_id");
    return -1;
}

int e_grab_device_by_ptr(struct device_info* dev, bool grab) {
    CHECK(drain_events(dev) == -1);
    CHECK(ioctl(dev->dev_fd, EVIOCGRAB, &grab) == -1);
    return 0;
err:
    ERR_LOG("grab_dev_by_ptr");
    return -1;
}

int e_get_device_info(int dev_fd, struct device_info* dev) {
    memset(dev, 0, sizeof(struct device_info));
    CHECK(getrandom(&dev->random_id, sizeof(dev->random_id), 0) == -1);
    dev->dev_fd = dev_fd;
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
    int u_fd = TRY(open("/dev/uinput", O_WRONLY | O_NONBLOCK), -1);
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
    struct uinput_setup setup = { .id = new_dev->dev_id };
    strncpy(setup.name, new_dev->name, UINPUT_MAX_NAME_SIZE-1);
    CHECK(ioctl(u_fd, UI_DEV_SETUP, &setup) == -1);
    CHECK(ioctl(u_fd, UI_DEV_CREATE) == -1);
    new_dev->dev_fd = u_fd; // overwrite the old peer's FD
    CHECK(d_epoll_add(ep_ctx, new_dev->dev_fd, FD_DEV) == -1);
    CHECK(u_array_add(&devices->r, new_dev) == -1);
    return 0;
err:
    close(u_fd);
    ERR_LOG("e_create_udev");
    return -1;
}