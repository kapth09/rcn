#include "include/rcn.h"
#include "include/rcn_arg.h"
#include "include/rcn_daemon.h"
#include <string.h>

enum debug_level g_debug_level = DEBUG_OFF;

static struct arg_context arg_ctx = {};

static int subaction_daemon(enum daemon_type d_type, enum subaction_type sa_type) {
    struct relay_arg r_arg = {};
    r_arg.d_type = d_type;
    r_arg.header_sent = TRY(subaction_to_rcn_msg(sa_type), -1);
    r_arg.check_daemon_status = false;
    CHECK(relay_start(r_arg) == -1);
    return 0;
err:
    DEBUG_LOG("");
    return -1;
}

/* rcn start -p 8000 */
static int action_start(int argc, char** argv) {
    if (argc < 4)
        EARG_COUNT(ARG_ACTION_START, 4, argc);
    arg_ctx.port.info.needed = true;
    CHECK(parse_args(argc, argv, 2, &arg_ctx) == -1);
    struct daemon_arg d_arg = {};
    d_arg.type = DAEMON_SERVER;
    d_arg.port = arg_ctx.port.val.v_int;
    struct relay_arg r_arg = {};
    r_arg.header_sent = RELAY_HEADER_START;
    r_arg.d_type = DAEMON_SERVER;
    r_arg.check_daemon_status = true;
    CHECK(daemon_start(d_arg, r_arg) == -1);
    return 0;
err:
    DEBUG_LOG("");
    return -1;
}

/* rcn connect -h <...> -p 800 -d <...> */
/* rcn connect -h <...> -p 800 -d <...> <...> <...> */
static int action_connect(int argc, char** argv) {
    if (argc < 8)
        EARG_COUNT(ARG_ACTION_START, 8, argc);
    arg_ctx.devices.info.needed = true;
    arg_ctx.port.info.needed = true;
    arg_ctx.server.info.needed = true;
    CHECK(parse_args(argc, argv, 2, &arg_ctx) == -1);
    struct daemon_arg d_arg = {};
    d_arg.port = arg_ctx.port.val.v_int;
    d_arg.host = arg_ctx.server.val.v_char;
    d_arg.devices_arg = &arg_ctx.devices.val.v_char_arr;
    d_arg.type = DAEMON_CLIENT;
    struct relay_arg r_arg = {};
    r_arg.header_sent = RELAY_HEADER_IDLE;
    r_arg.d_type = DAEMON_CLIENT;
    r_arg.check_daemon_status = true;
    CHECK(daemon_start(d_arg, r_arg) == -1);
    if (d_arg.devices_arg != NULL)
        CHECK(u_array_free(&d_arg.devices_arg->r) == -1);
    return 0;
err:
    DEBUG_LOG("");
    return -1;
}

static int action_server(int argc, char** argv) {
    if (argc < 3)
        EARG_COUNT(ARG_ACTION_SERVER, 3, argc);
    arg_ctx.daemon.info.needed = true;
    CHECK(parse_args(argc, argv, 2, &arg_ctx) == -1);
    if (arg_ctx.daemon.val.saction == SUBACTION_LOG)
        CHECK(d_print_log(DAEMON_SERVER) == -1);
    else
        CHECK(subaction_daemon(DAEMON_SERVER, arg_ctx.daemon.val.saction) == -1);
    return 0;
err:
    DEBUG_LOG("");
    return -1;
}

static int action_client(int argc, char** argv) {
    if (argc < 3)
        EARG_COUNT(ARG_ACTION_SERVER, 3, argc);
    arg_ctx.daemon.info.needed = true;
    CHECK(parse_args(argc, argv, 2, &arg_ctx) == -1);
    if (arg_ctx.daemon.val.saction == SUBACTION_LOG)
        CHECK(d_print_log(DAEMON_CLIENT) == -1);
    else
        CHECK(subaction_daemon(DAEMON_CLIENT, arg_ctx.daemon.val.saction) == -1);
    return 0;
err:
    DEBUG_LOG("");
    return -1;
}

static int help(int argc, char** argv) {
    arg_ctx.help.info.needed = true;
    CHECK(parse_args(argc, argv, 1, &arg_ctx) == -1);
    return 0;
err:
    DEBUG_LOG("");
    return -1;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        help(2, (char*[]){"", ARG_FLAG_HELP});
        ERR_GOTO(err, "\nerr: no action supplied\n");
    }
    arg_ctx.g_debug_level_ptr = &g_debug_level;
    const char* action = argv[1];
    if (strcmp(action, ARG_ACTION_START) == 0)
        CHECK(action_start(argc, argv) == -1);
    else if (strcmp(action, ARG_ACTION_CONNECT) == 0)
        CHECK(action_connect(argc, argv) == -1);
    else if (strcmp(action, ARG_ACTION_SERVER) == 0)
        CHECK(action_server(argc, argv) == -1);
    else if (strcmp(action, ARG_ACTION_CLIENT) == 0)
        CHECK(action_client(argc, argv) == -1);
    else
        CHECK(help(argc, argv) == -1);
    return 0;
err:
    fprintf(stderr, "see 'rcn -h' for help\n");
    return -1;
}
