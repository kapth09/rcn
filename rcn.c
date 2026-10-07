#include "include/rcn.h"
#include "include/rcn_arg.h"
#include "include/rcn_daemon.h"
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

enum debug_level g_debug_level = DEBUG_OFF;

static struct arg_context arg_ctx = {};

static int daemon_is_running(enum daemon_type d_type) {
    DIR* proc_dir = TRY(opendir("/proc"), NULL);
    char* proc_name = {};
    if (d_type == DAEMON_SERVER)
        proc_name = RCN_PROC_NAME_SERVER;
    else if (d_type == DAEMON_CLIENT)
        proc_name = RCN_PROC_NAME_CLIENT;
    struct dirent* ent = {};
    while ((ent = readdir(proc_dir)) != NULL) {
        char path_buff[512] = {};
        snprintf(path_buff, sizeof(path_buff), "/proc/%s/comm", ent->d_name);
        int file = open(path_buff, O_RDONLY);
        if (file == -1) {
            if (errno == ENOENT || errno == ENOTDIR)
                continue;
            goto err;
        }
        char name_buff[128] = {};
        const int length = TRY(read(file, name_buff, sizeof(name_buff)), -1);
        name_buff[length-1] = '\0';   // remove '\n' from name
        if (strcmp(name_buff, proc_name) == 0) {
            close(file);
            closedir(proc_dir);
            return 0;
        }
    }
    return -1;
err:
    DEBUG_LOG("");
    return -1;
}

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

static int action_show(int argc, char** argv) {
    if (daemon_is_running(DAEMON_CLIENT) == 0) {

    } else if (daemon_is_running(DAEMON_SERVER) == 0) {

    } else {

    }
    return 0;
}

static int auto_subaction(int argc, char** argv) {
    arg_ctx.daemon.info.needed = true;
    CHECK(parse_args(argc, argv, 1, &arg_ctx) == -1);
    enum daemon_type d_type = DAEMON_CLIENT;
    if (daemon_is_running(DAEMON_CLIENT) == -1) {
        d_type = DAEMON_SERVER;
        if (daemon_is_running(DAEMON_SERVER) == -1) {
            fprintf(stderr, "err: no daemon running\n");
            goto err;
        }
    }
    if (arg_ctx.daemon.val.saction == SUBACTION_LOG)
        CHECK(d_print_log(d_type) == -1);
    else
        CHECK(subaction_daemon(d_type, arg_ctx.daemon.val.saction) == -1);
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
    else if (strcmp(action, ARG_ACTION_SHOW) == 0)
        CHECK(action_show(argc, argv));
    else if (strcmp(action, ARG_FLAG_HELP) == 0)
        CHECK(help(argc, argv) == -1);
    else
        CHECK(auto_subaction(argc, argv) == -1);
    return 0;
err:
    fprintf(stderr, "see 'rcn -h' for help\n");
    return -1;
}
