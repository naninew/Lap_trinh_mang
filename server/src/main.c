#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include "network.h"
#include "account.h"
#include "game_room.h"

#ifdef _WIN32
    #include <process.h>
    #define GET_PID() _getpid()
#else
    #include <netdb.h>
    #include <unistd.h>
    #define GET_PID() getpid()
#endif

static socket_t g_server_sock = INVALID_SOCK;

static void handle_sigint(int sig) {
    (void)sig;
    printf("\n[*] Shutting down NetChess server...\n");
    if (g_server_sock != INVALID_SOCK) {
        CLOSE_SOCK(g_server_sock);
    }
    net_cleanup();
    exit(0);
}

static void get_local_lan_ip(char *out_ip, size_t max_len) {
    strncpy(out_ip, "127.0.0.1", max_len - 1);
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        struct addrinfo hints, *res = NULL, *p = NULL;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(hostname, NULL, &hints, &res) == 0) {
            for (p = res; p != NULL; p = p->ai_next) {
                struct sockaddr_in *sin = (struct sockaddr_in *)p->ai_addr;
                char *addr_str = inet_ntoa(sin->sin_addr);
                if (addr_str && strncmp(addr_str, "127.", 4) != 0) {
                    strncpy(out_ip, addr_str, max_len - 1);
                    break;
                }
            }
            freeaddrinfo(res);
        }
    }
}

static void print_server_network_banner(int port) {
    char lan_ip[64] = "127.0.0.1";
    get_local_lan_ip(lan_ip, sizeof(lan_ip));

    printf("\n");
    printf("====================================================\n");
    printf("         NETCHESS SERVER - HUST IT4062             \n");
    printf("====================================================\n");
    printf("[+] Server initialized on port %d with select() I/O\n", port);
    printf("[+] Accounts loaded: %d users from accounts.txt\n", account_get_all_count());
    printf("[+] Ready for player connections.\n\n");

    printf("--------------- CONNECTION URLS --------------------\n");
    printf("-> LOCALHOST CONNECTION:\n");
    printf("   * WebSocket:  ws://localhost:%d\n", port);
    printf("   * Web App UI: Open client/ui/index.html (Host: localhost:%d)\n\n", port);

    printf("-> LAN / INTERNAL WI-FI CONNECTION:\n");
    printf("   * Server IP:  %s\n", lan_ip);
    printf("   * WebSocket:  ws://%s:%d\n", lan_ip, port);
    printf("   * Web App UI: Open client/ui/index.html on devices on same Wi-Fi\n");
    printf("                 (Enter Server Host: %s:%d)\n", lan_ip, port);
    printf("----------------------------------------------------\n\n");
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    if (argc >= 2) {
        port = atoi(argv[1]);
        if (port <= 1024 || port > 65535) {
            fprintf(stderr, "Error: Invalid port number %d. Must be in range 1025-65535.\n", port);
            return 1;
        }
    }

    signal(SIGINT, handle_sigint);
#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif

    srand((unsigned int)time(NULL) ^ (unsigned int)GET_PID());

    if (!net_init()) {
        fprintf(stderr, "Failed to initialize network subsystem.\n");
        return 1;
    }

    account_init("accounts.txt");
    room_init_all();

    g_server_sock = net_create_server(port);
    if (g_server_sock == INVALID_SOCK) {
        fprintf(stderr, "Failed to create listening socket on port %d.\n", port);
        net_cleanup();
        return 1;
    }

    print_server_network_banner(port);

    net_run_loop(g_server_sock);

    CLOSE_SOCK(g_server_sock);
    net_cleanup();
    return 0;
}
