#ifndef RCN_H
#define RCN_H

#include "rcn_types.h"
#include <errno.h>
#include <stdio.h>

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

#define RCN_DAEMON_DIR_PATH "/tmp/rcn/"
#define RCN_SERVER_LOG_PATH RCN_DAEMON_DIR_PATH "server.log"
#define RCN_CLIENT_LOG_PATH RCN_DAEMON_DIR_PATH "client.log"
#define RCN_SERVER_SOCKET_PATH RCN_DAEMON_DIR_PATH "server.sock"
#define RCN_SERVER_SOCKET_LEN sizeof(RCN_SERVER_SOCKET_PATH)
#define RCN_CLIENT_SOCKET_PATH RCN_DAEMON_DIR_PATH "client.sock"
#define RCN_CLIENT_SOCKET_LEN sizeof(RCN_CLIENT_SOCKET_PATH)
#define RCN_STD_CAPACITY 10
#define RCN_DEV_MAX_NAME_LEN 255

#define RCN_PROC_NAME_SERVER "rcn_server"
#define RCN_PROC_NAME_CLIENT "rcn_client"
#define RCN_PROC_NAME_RELAY  "rcn_relay"

#endif // !RCN_H
