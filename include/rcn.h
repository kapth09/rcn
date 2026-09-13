#ifndef RCN_H
#define RCN_H

#include "rcn_util.h"
#include <stdio.h>
#include <errno.h>

#define ERR_GOTO(label, ...) 					\
    do { 							            \
	fprintf(stderr, __VA_ARGS__); 				\
	goto label; 						        \
    } while (0)

#define DO_GOTO(expr, label)                    \
    do {                                        \
    (expr);                                     \
    goto label;                                 \
    } while (0)

#define ERRNO_GOTO(label, msg) 					        \
    do { 							                    \
	fprintf(stderr, "%s: %s\n", msg, strerror(errno)); 	\
	goto label; 						                \
    } while (0)

#define ERR_LOG(...)                                    \
    do {                                                \
        fprintf(stderr, "err: ");                       \
        fprintf(stderr, __VA_ARGS__);       			\
        fprintf(stderr, ": %s\n", strerror(errno));     \
    } while (0)

#define CHECK(condition) 		    \
    do { 					        \
	if ((condition)) goto err; 		\
    } while(0)

#define TRY(expr, err_val) ({            \
    __typeof__(expr) _ret = (expr);      \
    if (_ret == (err_val)) goto err;     \
    _ret;                                \
})

#define RCN_GROUP "rcn"
#define RCN_DAEMON_DIR_PATH "/tmp/rcn/"
#define RCN_SERVER_LOG_PATH RCN_DAEMON_DIR_PATH "server.log"
#define RCN_SERVER_SOCKET_PATH RCN_DAEMON_DIR_PATH "server.sock"
#define RCN_SERVER_SOCKET_LEN sizeof(RCN_SERVER_SOCKET_PATH)
#define RCN_CLIENT_LOG_PATH RCN_DAEMON_DIR_PATH "client.log"
#define RCN_CLIENT_SOCKET_PATH RCN_DAEMON_DIR_PATH "client.sock"
#define RCN_CLIENT_SOCKET_LEN sizeof(RCN_CLIENT_SOCKET_PATH)
#define RCN_STD_CAPACITY 10
#define RCN_DEV_MAX_NAME_LEN 255

U_DEFINE_ARR(device_info_arr, struct device_info);
U_DEFINE_ARR(epoll_entry_arr, struct epoll_entry);
U_DEFINE_ARR(char_arr, char);

enum daemon_type {
    DAEMON_SERVER,
    DAEMON_CLIENT,
};

enum fd_type {
    FD_USOCK,
    FD_RELAY,
    FD_ISOCK,
    FD_PEER,
    FD_DEV,
};

struct epoll_entry {
    int fd;
    enum fd_type type;
    struct device_info* device; // only useable if type==FD_DEV, else NULL
};

struct epoll_context {
    epoll_entry_arr entries;    // stores pointers to struct epoll_entry
    int epoll_fd;
    int relay_count;
    int peer_count;
};

/* rcn_server.c */
int s_start(int port);

/* rcn_client.c */
int c_start(int port, char* host, char_arr devices_arg);
int c_close_connection(int fd);

#endif // !RCN_H
