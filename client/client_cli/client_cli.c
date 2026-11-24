#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

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
    typedef int socket_t;
    #define INVALID_SOCK -1
    #define CLOSE_SOCK close
#endif

#define BUFFER_SIZE 4096

static void print_menu(void) {
    printf("\n================ NETCHESS CLI ================\n");
    printf(" 1. Register Account\n");
    printf(" 2. Login Account\n");
    printf(" 3. View Online Idle Players\n");
    printf(" 4. Challenge Player (+/- 10 ranks only)\n");
    printf(" 5. Respond to Challenge (Accept/Decline)\n");
    printf(" 6. Make Move (e.g. e2 e4)\n");
    printf(" 7. Resign Current Match\n");
    printf(" 8. Read Incoming Server Messages\n");
    printf(" 0. Exit\n");
    printf("==============================================\n");
    printf("Enter choice: ");
}

static int str_to_sq(const char *s) {
    if (!s || strlen(s) < 2) return -1;
    int f = s[0] - 'a';
    int r = s[1] - '1';
    if (f < 0 || f > 7 || r < 0 || r > 7) return -1;
    return r * 8 + f;
}

int main(int argc, char *argv[]) {
    const char *host = (argc >= 2) ? argv[1] : "127.0.0.1";
    int port = (argc >= 3) ? atoi(argv[2]) : 8888;

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }
#endif

    socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCK) {
        fprintf(stderr, "Failed to create socket\n");
        return 1;
    }

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons((uint16_t)port);
    inet_pton(AF_INET, host, &saddr.sin_addr);

    printf("[*] Connecting to NetChess server at %s:%d...\n", host, port);
    if (connect(sock, (struct sockaddr *)&saddr, sizeof(saddr)) != 0) {
        fprintf(stderr, "[-] Connection failed.\n");
        CLOSE_SOCK(sock);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    printf("[+] Connected successfully!\n");

    char buf[BUFFER_SIZE];
    int choice = -1;

    while (1) {
        print_menu();
        if (scanf("%d", &choice) != 1) break;
        while (getchar() != '\n'); /* Flush newline */

        if (choice == 0) break;

        if (choice == 1) {
            char u[64], p[64];
            printf("Username: "); scanf("%63s", u);
            printf("Password: "); scanf("%63s", p);
            snprintf(buf, sizeof(buf), "{\"action\":\"REGISTER\",\"user\":\"%s\",\"pass\":\"%s\"}\n", u, p);
            send(sock, buf, (int)strlen(buf), 0);
        } else if (choice == 2) {
            char u[64], p[64];
            printf("Username: "); scanf("%63s", u);
            printf("Password: "); scanf("%63s", p);
            snprintf(buf, sizeof(buf), "{\"action\":\"LOGIN\",\"user\":\"%s\",\"pass\":\"%s\"}\n", u, p);
            send(sock, buf, (int)strlen(buf), 0);
        } else if (choice == 3) {
            snprintf(buf, sizeof(buf), "{\"action\":\"IDLE_LIST_REQ\"}\n");
            send(sock, buf, (int)strlen(buf), 0);
        } else if (choice == 4) {
            char target[64];
            printf("Target username: "); scanf("%63s", target);
            snprintf(buf, sizeof(buf), "{\"action\":\"CHALLENGE_REQ\",\"target\":\"%s\"}\n", target);
            send(sock, buf, (int)strlen(buf), 0);
        } else if (choice == 5) {
            char challenger[64];
            int acc = 0;
            printf("Challenger username: "); scanf("%63s", challenger);
            printf("Accept? (1 = Yes, 0 = No): "); scanf("%d", &acc);
            snprintf(buf, sizeof(buf),
                "{\"action\":\"CHALLENGE_REPLY\",\"from\":\"%s\",\"accept\":%s}\n",
                challenger, acc ? "true" : "false");
            send(sock, buf, (int)strlen(buf), 0);
        } else if (choice == 6) {
            char from_s[8], to_s[8];
            printf("From square (e.g. e2): "); scanf("%7s", from_s);
            printf("To square (e.g. e4): "); scanf("%7s", to_s);
            int from_sq = str_to_sq(from_s);
            int to_sq = str_to_sq(to_s);
            if (from_sq < 0 || to_sq < 0) {
                printf("Invalid algebraic notation square.\n");
                continue;
            }
            snprintf(buf, sizeof(buf),
                "{\"action\":\"MOVE_REQ\",\"from\":%d,\"to\":%d,\"promo\":0}\n",
                from_sq, to_sq);
            send(sock, buf, (int)strlen(buf), 0);
        } else if (choice == 7) {
            snprintf(buf, sizeof(buf), "{\"action\":\"RESIGN\"}\n");
            send(sock, buf, (int)strlen(buf), 0);
        }

        /* Read immediate responses */
        int bytes = recv(sock, buf, sizeof(buf) - 1, 0);
        if (bytes > 0) {
            buf[bytes] = '\0';
            printf("\n[Server Response]:\n%s\n", buf);
        }
    }

    CLOSE_SOCK(sock);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
