#ifndef RCN_H
#define RCN_H

#include "rcn_types.h"
#include <errno.h>
#include <stdio.h>

extern enum debug_level g_debug_level;

#define LOG(fmt, ...) fprintf(stdout, "rcn: " fmt "\n", ##__VA_ARGS__)

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

#define ERR_LOG(fmt, ...)										\
    do {                                                		\
        fprintf(stderr, "err: %s:%d", __func__, __LINE__);	    \
        if ((fmt)[0] != 0){										\
        fprintf(stderr, " " fmt, ##__VA_ARGS__);       			\
		}														\
		int tmp_errno = errno;									\
        if (errno != 0) {										\
		  fprintf(stderr, " [%s]", strerror(errno));     		\
		  errno = 0;											\
		}														\
		errno = tmp_errno;										\
		fprintf(stderr, "\n");									\
    } while (0)

#define DEBUG_LOG(...)                                  \
    do {                                                \
		if (g_debug_level == DEBUG_ON) {				\
			ERR_LOG(__VA_ARGS__);						\
		} 												\
    } while (0)

#define CHECK(condition) 		    \
    do { 					        \
	if ((condition)) goto err; 		\
	errno = 0;						\
    } while(0)

#define TRY(expr, err_val) ({            \
    __typeof__(expr) _ret = (expr);      \
    if (_ret == (err_val)) goto err;     \
	errno = 0;	   						 \
    _ret;                                \
})

#define RCN_DAEMON_DIR_PATH "/tmp/rcn/"
#define RCN_SERVER_LOG_PATH RCN_DAEMON_DIR_PATH "server.log"
#define RCN_CLIENT_LOG_PATH RCN_DAEMON_DIR_PATH "client.log"
#define RCN_SERVER_USOCKET_PATH RCN_DAEMON_DIR_PATH "server.sock"
#define RCN_SERVER_USOCKET_LEN sizeof(RCN_SERVER_USOCKET_PATH)
#define RCN_CLIENT_USOCKET_PATH RCN_DAEMON_DIR_PATH "client.sock"
#define RCN_CLIENT_USOCKET_LEN sizeof(RCN_CLIENT_USOCKET_PATH)
#define RCN_STD_CAPACITY 10
#define RCN_DEV_MAX_NAME_LEN 255

#define RCN_PROC_NAME_SERVER "rcn_server"
#define RCN_PROC_NAME_CLIENT "rcn_client"
#define RCN_PROC_NAME_RELAY  "rcn_relay"

#define RCN_CONN_TIMEOUT_MS 5000

#endif // !RCN_H
