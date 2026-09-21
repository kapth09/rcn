#ifndef RCN_RCN_DEV_H
#define RCN_RCN_DEV_H

#include "rcn_types.h"
#include <stdint.h>
#include <linux/uinput.h>
#include <linux/input.h>
#include <linux/input-event-codes.h>

#define HAS_BIT(array, bit) (((array)[(bit)/8] >> ((bit)%8)) & 1)

#define MAX_EVT_BYTES   ((EV_MAX+7) / 8)
#define MAX_KEY_BYTES   ((KEY_MAX+7) / 8)
#define MAX_ABS_BYTES   ((ABS_MAX+7) / 8)
#define MAX_REL_BYTES   ((REL_MAX+7) / 8)
#define MAX_PROP_BYTES  ((INPUT_PROP_MAX+7) / 8)

struct device_info {
    uint8_t evtbit[MAX_EVT_BYTES];
    uint8_t keybit[MAX_KEY_BYTES];
    uint8_t absbit[MAX_ABS_BYTES];
    uint8_t relbit[MAX_REL_BYTES];
    uint8_t propbit[MAX_PROP_BYTES];
    char name[UINPUT_MAX_NAME_SIZE];
    struct input_absinfo absinfo[ABS_MAX+1];
    struct input_id dev_id;
};

struct device {
    struct device_info info;
    struct epoll_stream stream;
    size_t random_id;
};

struct device_context {
    device_arr devices;
};

int e_init_device(struct epoll_context* ep_ctx, device_arr* devices, const char *dev_path, struct device* out_dev);
int e_grab_device_by_id(device_arr* devices, size_t random_id, bool grab);
int e_grab_device_by_ptr(struct device* dev, bool grab);
int e_get_device_info(int dev_fd, struct device* dev);
int e_create_udev(struct epoll_context* ep_ctx, device_arr* devices, struct device* new_dev);

#endif //RCN_RCN_DEV_H
