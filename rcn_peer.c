#include "include/rcn.h"
#include "include/rcn_epoll.h"
#include "include/rcn_peer.h"
#include "include/rcn_stream.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <string.h>

int p_init_peer_ctx(struct epoll_context* ep_ctx, struct peer_context* p_ctx, int isock_fd, int port, enum daemon_type d_type) {
    p_ctx->isock_port = port;
    if (d_type == DAEMON_SERVER) {
        p_ctx->peer_stream = NULL;
        CHECK(e_epoll_add_getr(ep_ctx, isock_fd, FD_ISOCK, &p_ctx->isock_stream) == -1);
        CHECK(stream_set_default(p_ctx->isock_stream, STREAM_OP_READING, STREAM_TYPE_SOCKET) == -1);
        p_ctx->peer_state = PEER_DISCONNECTED;
    } else if (d_type == DAEMON_CLIENT) {
        p_ctx->isock_stream = NULL;
        CHECK(e_epoll_add_getr(ep_ctx, isock_fd, FD_PEER, &p_ctx->peer_stream) == -1);
        CHECK(stream_set_default(p_ctx->peer_stream, STREAM_OP_READING, STREAM_TYPE_SOCKET) == -1);
        p_ctx->peer_state = PEER_CONNECTED;
    }
    return 0;
err:
    ERR_LOG("");
    return -1;
}

int p_close_peer_ctx(struct epoll_context* ep_ctx, struct peer_context* p_ctx) {
    if (p_ctx->peer_stream != NULL && p_ctx->peer_state != PEER_DISCONNECTED) {
        p_ctx->peer_state = PEER_DISCONNECTED;
        CHECK(e_epoll_close_remove_simple(ep_ctx, p_ctx->peer_stream) == -1);
    }
    return 0;
err:
    ERR_LOG("");
    return -1;
}

int p_close_peer(struct epoll_context* ep_ctx, struct peer_context* p_ctx) {
    (void)ep_ctx;
    p_ctx->peer_state = PEER_DISCONNECTED;
    LOG("peer closed connection");
    return 0;
}

int p_init_sock(int port) {
    const int isock_fd = TRY(socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0), -1);
    const int reuse = 1;
    CHECK(setsockopt(isock_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1);
    struct sockaddr_in s_iaddr = {};
    s_iaddr.sin_family = AF_INET;
    s_iaddr.sin_port = htons(port);
    s_iaddr.sin_addr.s_addr = INADDR_ANY;
    CHECK(bind(isock_fd, (struct sockaddr*)&s_iaddr, sizeof(s_iaddr)) == -1);
    CHECK(listen(isock_fd, 0) == -1);
    LOG("listening on port %d", port);
    return isock_fd;
err:
    ERR_LOG("");
    return -1;
}

static int resolve_host(const int port, const char *host, struct addrinfo** addr) {
    char port_str[6];
    snprintf(port_str, sizeof(port_str), "%d", port);
    struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    int res = getaddrinfo(host, port_str, &hints, addr);
    if (res != 0)
        goto err;
    return 0;
    err:
        errno = EHOSTUNREACH;
    ERR_LOG(" %s", gai_strerror(res));
    return -1;
}

int p_connect_sock(int port, char* host) {
    const int isock_fd = TRY(socket(AF_INET, SOCK_STREAM, 0), -1);
    const int timeout_ms = RCN_CONN_TIMEOUT_MS;
    CHECK(setsockopt(isock_fd, IPPROTO_TCP, TCP_USER_TIMEOUT, &timeout_ms, sizeof(timeout_ms)) == -1);
    const int no_delay = 1;
    CHECK(setsockopt(isock_fd, SOL_TCP, TCP_NODELAY, &no_delay, sizeof(no_delay)) == -1);
    struct addrinfo* p_info = NULL;
    CHECK(resolve_host(port, host, &p_info) == -1);
    bool connected = false;
    for (struct addrinfo* i = p_info; i != NULL; i = i->ai_next) {
        int res = connect(isock_fd, i->ai_addr, i->ai_addrlen);
        if (res == 0) {
            connected = true;
            break;
        }
        CHECK(errno != ETIMEDOUT);
        printf("rcn: timeout\n");
    }
    freeaddrinfo(p_info);
    CHECK(connected == false);
    int fd_flags = TRY(fcntl(isock_fd, F_GETFL, 0), 1);
    CHECK(fcntl(isock_fd, F_SETFL, O_NONBLOCK | fd_flags) == -1);
    LOG("connected to server %s", host);
    return isock_fd;
err:
    ERR_LOG("");
    return -1;
}