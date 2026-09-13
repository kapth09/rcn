#ifndef RCN_RCN_UTIL_H
#define RCN_RCN_UTIL_H

#include <stdio.h>

struct u_array {
    void* data;
    size_t size; 	    // size of each element
    size_t capacity; 	// total capacity
    size_t length; 	    // current amount of elements
};

#define U_DEFINE_ARR(name, type)  \
typedef struct {                 \
    struct u_array r;            \
} (name);

/* rcn_util.c, u_array */
struct u_array u_array_create(size_t size, size_t capacity);
int u_array_init(struct u_array* arr, size_t size, size_t capacity);
int u_array_resize(struct u_array* arr, size_t extra_elements);
int u_array_add(struct u_array* arr, void* data);
int u_array_set(struct u_array* arr, void* data, size_t i);
int u_array_delete(struct u_array* arr);
int u_array_remove(struct u_array* arr, size_t i);
int u_array_getr(struct u_array* arr, void** element, size_t i);
int u_array_getv(struct u_array* arr, void* element, size_t i);
ssize_t u_array_find_index(struct u_array* arr, void* element);
int u_array_free(struct u_array* arr);

#endif //RCN_RCN_UTIL_H
