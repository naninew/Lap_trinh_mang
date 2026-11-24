#ifndef NETWORK_H
#define NETWORK_H

#include <stdbool.h>
#include "protocol.h"

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef SOCKET socket_t;
    #define INVALID_SOCK INVALID_SOCKET
    #define CLOSE_SOCK closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    typedef int socket_t;
    #define INVALID_SOCK -1
    #define CLOSE_SOCK close
#endif

typedef enum {
    CONN_RAW_TCP,
    CONN_WEBSOCKET_HANDSHAKING,
    CONN_WEBSOCKET_OPEN
} ConnectionType;

typedef struct {
    socket_t fd;
    bool active;
    ConnectionType conn_type;
    char ip[64];
    int port;
    char rx_buf[BUFFER_SIZE * 2];
    size_t rx_len;
    bool logged_in;
    char username[MAX_USERNAME_LEN];
    UserState state;
    int room_id;
    char challenged_by[MAX_USERNAME_LEN];
    int challenge_time_mode;
} ClientSession;

bool net_init(void);
void net_cleanup(void);
socket_t net_create_server(int port);
void net_run_loop(socket_t server_fd);
bool net_send_ws_text(socket_t fd, const char *text);
bool net_send_raw(socket_t fd, const void *data, size_t len);

#endif
