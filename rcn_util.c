#include "include/rcn.h"
#include <stdlib.h>
#include <string.h>

/* u_array */
static int resize(void* data, size_t elm_size, size_t* capacity,size_t extra_elements) {
    if (data == NULL)
        DO_GOTO(errno = EFAULT, err);
    size_t new_size = (*capacity + extra_elements) * elm_size;
    void* tmp = realloc(data, new_size);
    CHECK(tmp == NULL);
    data = tmp;
    *capacity += extra_elements;
    return 0;
err:
    ERR_LOG("u_array_resize");
    return -1;
}

struct u_array u_array_create(size_t size, size_t capacity) {
    struct u_array arr = {
        .length = 0,
        .capacity = capacity,
        .size = size,
        .data = TRY(calloc(capacity, size), NULL)
    };
    return arr;
err:
    ERR_LOG("u_array_create");
    return (struct u_array){.data = NULL};
}

int u_array_init(struct u_array* arr, size_t size, size_t capacity) {
    CHECK(size <= 0);
    CHECK(capacity <= 0);
    arr->length = 0;
    arr->capacity = capacity;
    arr->size = size;
    arr->data = calloc(capacity, size);
    CHECK(arr->data == NULL);
    return 0;
err:
    ERR_LOG("u_array_init");
    return -1;
}

int u_array_resize(struct u_array* arr, size_t extra_elements) {
    CHECK(resize(&arr->data, arr->size, &arr->capacity, extra_elements) == -1);
    return 0;
err:
    ERR_LOG("u_array_resize");
    return -1;
}

int u_array_add(struct u_array* arr, void* data) {
    if (arr->data == NULL)
        DO_GOTO(errno = EFAULT, err);
    if (arr->length == arr->capacity)
        CHECK(u_array_resize(arr, arr->capacity) == -1);
    void* offset = arr->data + (arr->size * arr->length);
    memcpy(offset, data, arr->size);
    arr->length++;
    return 0;
err:
    ERR_LOG("u_array_add");
    return -1;
}

int u_array_set(struct u_array* arr, void* data, size_t i) {
    if (arr->data == NULL)
        DO_GOTO(errno = EFAULT, err);
    while (i >= arr->capacity)
        CHECK(u_array_resize(arr, arr->capacity) == -1);
    void* offset = arr->data + (arr->size * i);
    memcpy(offset, data, arr->size);
    if (i >= arr->length)
        arr->length = i + 1;
    return 0;
err:
    ERR_LOG("u_array_set");
    return -1;
}

int u_array_delete(struct u_array* arr) {
    if (arr->data == NULL)
        DO_GOTO(errno = EFAULT, err);
    CHECK(arr == NULL);
    arr->length--;
    return 0;
err:
    ERR_LOG("u_array_delete");
    return -1;
}

int u_array_remove(struct u_array* arr, size_t i) {
    if (arr->data == NULL)
        DO_GOTO(errno = EFAULT, err);
    CHECK(arr->length == 0);
    CHECK(i >= arr->length);
    void* dest = arr->data + (arr->size * i);
    void* src = arr->data + (arr->size * (i+1));
    size_t n = (arr->length - i - 1) * arr->size;
    memmove(dest, src, n);
    arr->length--;
    return 0;
err:
    ERR_LOG("u_array_remove");
    return -1;
}

int u_array_getr(struct u_array* arr, void** element, size_t i) {
    if (arr->data == NULL)
        DO_GOTO(errno = EFAULT, err);
    CHECK(i >= arr->length);
    *element = arr->data + (arr->size * i);
    return 0;
err:
    ERR_LOG("u_array_getr");
    return -1;
}

int u_array_getv(struct u_array* arr, void* element, size_t i) {
    if (arr->data == NULL)
        DO_GOTO(errno = EFAULT, err);
    CHECK(i >= arr->length);
    void* offset = arr->data + (arr->size * i);
    memcpy(element, offset, arr->size);
    return 0;
err:
    ERR_LOG("u_array_getv");
    return -1;
}

ssize_t u_array_find_index(struct u_array* arr, void* element) {
    if (arr->data == NULL)
        DO_GOTO(errno = EFAULT, err);
    for (size_t i = 0; i < arr->length; i++) {
        void* offset = arr->data + (arr->size * i);
        if (memcmp(offset, element, arr->size) == 0)
            return (ssize_t)i;
    }
    return -1;
err:
    ERR_LOG("u_array_find_index");
    return -1;
}

int u_array_free(struct u_array* arr) {
    CHECK(arr == NULL);
    free(arr->data);
    arr->data = NULL;
    return 0;
err:
    ERR_LOG("u_array_free");
    return -1;
}