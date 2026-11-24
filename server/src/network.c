#include "network.h"
#include "protocol.h"
#include "sha1.h"
#include "base64.h"
#include "account.h"
#include "chess_engine.h"
#include "game_room.h"
#include "file_transfer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ClientSession g_sessions[MAX_CLIENTS];
static const char *WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

static void broadcast_idle_list(void);
static void send_idle_list_to_session(ClientSession *sess);
static void escape_json_str(const char *src, char *out, size_t out_size);

bool net_init(void) {
#ifdef _WIN32
    WSADATA wsa;
    return (WSAStartup(MAKEWORD(2, 2), &wsa) == 0);
#else
    return true;
#endif
}

void net_cleanup(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

static bool set_nonblocking(socket_t fd) {
#ifdef _WIN32
    u_long mode = 1;
    return (ioctlsocket(fd, FIONBIO, &mode) == 0);
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return false;
    return (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0);
#endif
}

socket_t net_create_server(int port) {
    socket_t fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == INVALID_SOCK) return INVALID_SOCK;

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        fprintf(stderr, "[ERROR] Port %d is already in use by another server instance.\n", port);
        fprintf(stderr, "        Please terminate the existing server or specify a different port (e.g. server.exe 8889).\n");
        CLOSE_SOCK(fd);
        return INVALID_SOCK;
    }

    if (listen(fd, 16) != 0) {
        CLOSE_SOCK(fd);
        return INVALID_SOCK;
    }

    set_nonblocking(fd);
    return fd;
}

static int send_all(socket_t fd, const void *buf, size_t len) {
    const char *p = (const char *)buf;
    size_t total = 0;
    while (total < len) {
        int n = send(fd, p + total, (int)(len - total), 0);
        if (n <= 0) return -1;
        total += (size_t)n;
    }
    return 0;
}

bool net_send_raw(socket_t fd, const void *data, size_t len) {
    return (send_all(fd, data, len) == 0);
}

bool net_send_ws_text(socket_t fd, const char *text) {
    if (!text) return false;
    size_t payload_len = strlen(text);
    char header[10];
    size_t header_len = 0;

    header[0] = (char)0x81; /* FIN + Text frame */
    if (payload_len <= 125) {
        header[1] = (char)payload_len;
        header_len = 2;
    } else if (payload_len <= 65535) {
        header[1] = (char)126;
        header[2] = (char)((payload_len >> 8) & 0xFF);
        header[3] = (char)(payload_len & 0xFF);
        header_len = 4;
    } else {
        header[1] = (char)127;
        for (int i = 7; i >= 0; i--) {
            header[2 + (7 - i)] = (char)((payload_len >> (i * 8)) & 0xFF);
        }
        header_len = 10;
    }

    if (send_all(fd, header, header_len) != 0) return false;
    return (send_all(fd, text, payload_len) == 0);
}

static void send_to_session(ClientSession *sess, const char *json_msg) {
    if (!sess || !sess->active) return;
    if (sess->conn_type == CONN_WEBSOCKET_OPEN) {
        net_send_ws_text(sess->fd, json_msg);
    } else {
        net_send_raw(sess->fd, json_msg, strlen(json_msg));
        net_send_raw(sess->fd, "\n", 1);
    }
}

static ClientSession* find_session_by_username(const char *username) {
    if (!username) return NULL;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_sessions[i].active && g_sessions[i].logged_in &&
            strcmp(g_sessions[i].username, username) == 0) {
            return &g_sessions[i];
        }
    }
    return NULL;
}

static ClientSession* find_session_by_fd(socket_t fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_sessions[i].active && g_sessions[i].fd == fd) {
            return &g_sessions[i];
        }
    }
    return NULL;
}

typedef struct {
    char user[MAX_USERNAME_LEN];
    int score;
    int rank;
    int tier;
    int online;
    int can_challenge;
    char status_text[64];
} LobbyEntry;

static int compare_lobby_entries(const void *a, const void *b) {
    const LobbyEntry *ea = (const LobbyEntry *)a;
    const LobbyEntry *eb = (const LobbyEntry *)b;
    if (ea->tier != eb->tier) return ea->tier - eb->tier;
    if (ea->rank != eb->rank) return ea->rank - eb->rank;
    if (ea->score != eb->score) return eb->score - ea->score;
    return strcmp(ea->user, eb->user);
}

static void send_idle_list_to_session(ClientSession *sess) {
    if (!sess || !sess->active || !sess->logged_in) return;

    Account *my_acc = account_find(sess->username);
    int my_rank = my_acc ? my_acc->rank : 1;

    int total_accounts = account_get_all_count();
    LobbyEntry entries[256];
    int count = 0;

    for (int i = 0; i < total_accounts && count < 256; i++) {
        Account *acc = account_get_by_index(i);
        if (!acc) continue;
        if (strcmp(acc->username, sess->username) == 0) continue;

        LobbyEntry *e = &entries[count++];
        snprintf(e->user, sizeof(e->user), "%s", acc->username);
        e->score = acc->score;
        e->rank = acc->rank;

        ClientSession *tgt_sess = find_session_by_username(acc->username);
        if (tgt_sess && tgt_sess->active && tgt_sess->logged_in) {
            e->online = 1;
            int rank_diff = abs(my_rank - acc->rank);
            if (tgt_sess->state == USER_STATE_IDLE) {
                if (rank_diff <= RANK_DIFF_LIMIT) {
                    e->tier = 1;
                    e->can_challenge = 1;
                    snprintf(e->status_text, sizeof(e->status_text), "Ready to Challenge");
                } else {
                    e->tier = 2;
                    e->can_challenge = 0;
                    snprintf(e->status_text, sizeof(e->status_text), "Rank Diff: %d (>10)", rank_diff);
                }
            } else {
                e->tier = 2;
                e->can_challenge = 0;
                snprintf(e->status_text, sizeof(e->status_text), "In Game");
            }
        } else {
            e->online = 0;
            e->tier = 3;
            e->can_challenge = 0;
            snprintf(e->status_text, sizeof(e->status_text), "Offline");
        }
    }

    qsort(entries, count, sizeof(LobbyEntry), compare_lobby_entries);

    size_t buf_size = 32768;
    char *buf = (char *)malloc(buf_size);
    if (!buf) return;

    int offset = snprintf(buf, buf_size, "{\"action\":\"IDLE_LIST\",\"players\":[");
    for (int i = 0; i < count; i++) {
        offset += snprintf(buf + offset, buf_size - offset,
            "%s{\"user\":\"%s\",\"score\":%d,\"rank\":%d,\"tier\":%d,\"online\":%d,\"can_challenge\":%d,\"status_text\":\"%s\"}",
            (i > 0) ? "," : "",
            entries[i].user, entries[i].score, entries[i].rank,
            entries[i].tier, entries[i].online, entries[i].can_challenge, entries[i].status_text);
        if ((size_t)offset >= buf_size - 128) break;
    }
    snprintf(buf + offset, buf_size - offset, "]}");
    send_to_session(sess, buf);
    free(buf);
}

static void broadcast_idle_list(void) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_sessions[i].active && g_sessions[i].logged_in && g_sessions[i].state == USER_STATE_IDLE) {
            send_idle_list_to_session(&g_sessions[i]);
        }
    }
}

static void handle_client_disconnect(ClientSession *sess) {
    if (!sess || !sess->active) return;

    /* Handle active game resignation on disconnect */
    if (sess->state == USER_STATE_IN_GAME && sess->room_id > 0) {
        GameRoom *room = room_find_by_id(sess->room_id);
        if (room && room->active) {
            bool is_white = (room->white_fd == (int)sess->fd);
            room->result = is_white ? GAME_RESULT_BLACK_WIN : GAME_RESULT_WHITE_WIN;
            snprintf(room->result_reason, sizeof(room->result_reason), "%s disconnected", sess->username);

            account_update_score(room->white_user, is_white ? SCORE_LOSS : SCORE_WIN, is_white ? 2 : 1);
            account_update_score(room->black_user, is_white ? SCORE_WIN : SCORE_LOSS, is_white ? 1 : 2);

            char log_buf[BUFFER_SIZE * 2];
            log_generate_match_report(room, log_buf, sizeof(log_buf));
            char log_file[64];
            snprintf(log_file, sizeof(log_file), "match_%d.log", room->room_id);
            log_save_to_disk(log_file, log_buf);

            socket_t opp_fd = is_white ? (socket_t)room->black_fd : (socket_t)room->white_fd;
            ClientSession *opp_sess = find_session_by_fd(opp_fd);
            if (opp_sess && opp_sess->active) {
                char go_msg[256];
                snprintf(go_msg, sizeof(go_msg),
                    "{\"action\":\"GAME_OVER\",\"winner\":\"%s\",\"reason\":\"%s\",\"score_delta\":3}",
                    is_white ? room->black_user : room->white_user, room->result_reason);
                send_to_session(opp_sess, go_msg);

            char log_escaped[BUFFER_SIZE * 2];
            escape_json_str(log_buf, log_escaped, sizeof(log_escaped));
            char log_msg[BUFFER_SIZE * 2 + 128];
            snprintf(log_msg, sizeof(log_msg),
                "{\"action\":\"LOG_FILE\",\"filename\":\"%s\",\"content\":\"%s\"}",
                log_file, log_escaped);
            send_to_session(opp_sess, log_msg);

                opp_sess->state = USER_STATE_IDLE;
                opp_sess->room_id = 0;
            }
            room_close(room);
        }
    }

    CLOSE_SOCK(sess->fd);
    sess->active = false;
    sess->logged_in = false;
    sess->state = USER_STATE_DISCONNECTED;
    sess->rx_len = 0;

    broadcast_idle_list();
}

static char* extract_json_str(const char *json, const char *key, char *out, size_t out_size) {
    out[0] = '\0';
    char needle[64];
    snprintf(needle, sizeof(needle), "\"%s\":", key);
    const char *p = strstr(json, needle);
    if (!p) return NULL;
    p += strlen(needle);
    while (*p == ' ' || *p == '\t') p++;

    if (*p == '\"') {
        p++;
        size_t idx = 0;
        while (*p && *p != '\"' && idx < out_size - 1) {
            out[idx++] = *p++;
        }
        out[idx] = '\0';
        return out;
    }
    return NULL;
}

static int extract_json_int(const char *json, const char *key, int default_val) {
    char needle[64];
    snprintf(needle, sizeof(needle), "\"%s\":", key);
    const char *p = strstr(json, needle);
    if (!p) return default_val;
    p += strlen(needle);
    while (*p == ' ' || *p == '\t') p++;
    return atoi(p);
}

static bool extract_json_bool(const char *json, const char *key, bool default_val) {
    char needle[64];
    snprintf(needle, sizeof(needle), "\"%s\":", key);
    const char *p = strstr(json, needle);
    if (!p) return default_val;
    p += strlen(needle);
    while (*p == ' ' || *p == '\t') p++;
    if (strncmp(p, "true", 4) == 0) return true;
    if (strncmp(p, "false", 5) == 0) return false;
    return default_val;
}

static void escape_json_str(const char *src, char *out, size_t out_size) {
    size_t j = 0;
    for (size_t i = 0; src[i] != '\0' && j < out_size - 2; i++) {
        if (src[i] == '\"' || src[i] == '\\') {
            if (j < out_size - 3) { out[j++] = '\\'; out[j++] = src[i]; }
        } else if (src[i] == '\n') {
            if (j < out_size - 3) { out[j++] = '\\'; out[j++] = 'n'; }
        } else if (src[i] == '\r') {
            if (j < out_size - 3) { out[j++] = '\\'; out[j++] = 'r'; }
        } else {
            out[j++] = src[i];
        }
    }
    out[j] = '\0';
}

static void process_app_message(ClientSession *sess, const char *msg) {
    char action[32];
    if (!extract_json_str(msg, "action", action, sizeof(action))) return;

    if (strcmp(action, "REGISTER") == 0) {
        char user[MAX_USERNAME_LEN], pass[MAX_PASSWORD_LEN], err[128];
        extract_json_str(msg, "user", user, sizeof(user));
        extract_json_str(msg, "pass", pass, sizeof(pass));

        char resp[256];
        if (account_register(user, pass, err, sizeof(err))) {
            snprintf(resp, sizeof(resp),
                "{\"action\":\"REGISTER_RESP\",\"status\":\"OK\",\"msg\":\"Registration successful\"}");
        } else {
            snprintf(resp, sizeof(resp),
                "{\"action\":\"REGISTER_RESP\",\"status\":\"ERROR\",\"msg\":\"%s\"}", err);
        }
        send_to_session(sess, resp);
    }
    else if (strcmp(action, "LOGIN") == 0) {
        char user[MAX_USERNAME_LEN], pass[MAX_PASSWORD_LEN], err[128];
        extract_json_str(msg, "user", user, sizeof(user));
        extract_json_str(msg, "pass", pass, sizeof(pass));

        Account *acc = NULL;
        char resp[256];
        if (account_login(user, pass, &acc, err, sizeof(err))) {
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (&g_sessions[i] != sess && g_sessions[i].active && g_sessions[i].logged_in &&
                    strcmp(g_sessions[i].username, user) == 0) {
                    send_to_session(&g_sessions[i], "{\"action\":\"KICKED\",\"reason\":\"Account signed in from another device or session.\"}");
                    CLOSE_SOCK(g_sessions[i].fd);
                    g_sessions[i].active = false;
                    g_sessions[i].logged_in = false;
                    g_sessions[i].state = USER_STATE_DISCONNECTED;
                }
            }

            sess->logged_in = true;
            snprintf(sess->username, sizeof(sess->username), "%s", user);
            sess->state = USER_STATE_IDLE;
            snprintf(resp, sizeof(resp),
                "{\"action\":\"LOGIN_RESP\",\"status\":\"OK\",\"user\":\"%s\",\"score\":%d,\"rank\":%d}",
                acc->username, acc->score, acc->rank);
            send_to_session(sess, resp);
            broadcast_idle_list();
        } else {
            snprintf(resp, sizeof(resp),
                "{\"action\":\"LOGIN_RESP\",\"status\":\"ERROR\",\"msg\":\"%s\"}", err);
            send_to_session(sess, resp);
        }
    }
    else if (strcmp(action, "LOGOUT") == 0) {
        if (sess->logged_in) {
            sess->logged_in = false;
            sess->username[0] = '\0';
            sess->state = USER_STATE_DISCONNECTED;
            send_to_session(sess, "{\"action\":\"LOGOUT_RESP\",\"status\":\"OK\"}");
            broadcast_idle_list();
        }
    }
    else if (strcmp(action, "IDLE_LIST_REQ") == 0) {
        send_idle_list_to_session(sess);
    }
    else if (strcmp(action, "CHALLENGE_REQ") == 0) {
        if (!sess->logged_in || sess->state != USER_STATE_IDLE) return;
        char target[MAX_USERNAME_LEN];
        extract_json_str(msg, "target", target, sizeof(target));
        int time_mode = extract_json_int(msg, "time_mode", 10);
        if (time_mode != 10 && time_mode != 60) time_mode = 10;

        ClientSession *tgt_sess = find_session_by_username(target);
        if (!tgt_sess || tgt_sess->state != USER_STATE_IDLE) {
            char resp[256];
            snprintf(resp, sizeof(resp),
                "{\"action\":\"CHALLENGE_ERR\",\"msg\":\"Target player is not available (offline or busy)\"}");
            send_to_session(sess, resp);
            return;
        }

        int rank_diff = 0;
        if (!account_can_challenge(sess->username, target, &rank_diff)) {
            char resp[256];
            Account *my_acc = account_find(sess->username);
            Account *tgt_acc = account_find(target);
            snprintf(resp, sizeof(resp),
                "{\"action\":\"CHALLENGE_ERR\",\"msg\":\"Cannot challenge: Rank difference is %d (Rank %d vs Rank %d), exceeding limit of 10.\"}",
                rank_diff, my_acc ? my_acc->rank : 0, tgt_acc ? tgt_acc->rank : 0);
            send_to_session(sess, resp);
            return;
        }

        /* Forward challenge to target player with time mode */
        tgt_sess->challenge_time_mode = time_mode;
        snprintf(tgt_sess->challenged_by, sizeof(tgt_sess->challenged_by), "%s", sess->username);
        Account *my_acc = account_find(sess->username);
        char notif[256];
        snprintf(notif, sizeof(notif),
            "{\"action\":\"CHALLENGE_NOTIFY\",\"from\":\"%s\",\"score\":%d,\"rank\":%d,\"time_mode\":%d}",
            sess->username, my_acc ? my_acc->score : 100, my_acc ? my_acc->rank : 1, time_mode);
        send_to_session(tgt_sess, notif);
    }
    else if (strcmp(action, "CHALLENGE_REPLY") == 0) {
        if (!sess->logged_in) return;
        char from_user[MAX_USERNAME_LEN];
        extract_json_str(msg, "from", from_user, sizeof(from_user));
        bool accept = extract_json_bool(msg, "accept", false);

        ClientSession *from_sess = find_session_by_username(from_user);
        if (!from_sess || from_sess->state != USER_STATE_IDLE) {
            char resp[256];
            snprintf(resp, sizeof(resp),
                "{\"action\":\"CHALLENGE_ERR\",\"msg\":\"Challenger is no longer available.\"}");
            send_to_session(sess, resp);
            return;
        }

        if (!accept) {
            char resp[256];
            snprintf(resp, sizeof(resp),
                "{\"action\":\"CHALLENGE_ERR\",\"msg\":\"%s declined your challenge.\"}", sess->username);
            send_to_session(from_sess, resp);
            return;
        }

        /* Accept challenge: randomly assign White/Black sides (50/50) */
        bool challenger_is_white = (rand() % 2 == 0);
        ClientSession *white_sess = challenger_is_white ? from_sess : sess;
        ClientSession *black_sess = challenger_is_white ? sess : from_sess;

        int time_mode = (sess->challenge_time_mode > 0) ? sess->challenge_time_mode : 10;
        GameRoom *room = room_create((int)white_sess->fd, white_sess->username, white_sess->ip,
                                     (int)black_sess->fd, black_sess->username, black_sess->ip,
                                     time_mode);
        if (!room) return;

        white_sess->state = USER_STATE_IN_GAME;
        white_sess->room_id = room->room_id;
        black_sess->state = USER_STATE_IN_GAME;
        black_sess->room_id = room->room_id;

        Account *acc_white = account_find(white_sess->username);
        Account *acc_black = account_find(black_sess->username);

        char start_msg_w[256], start_msg_b[256];
        snprintf(start_msg_w, sizeof(start_msg_w),
            "{\"action\":\"GAME_START\",\"match_id\":%d,\"color\":\"white\",\"opp_name\":\"%s\",\"opp_score\":%d,\"opp_rank\":%d,\"time_control\":%d}",
            room->room_id, black_sess->username, acc_black ? acc_black->score : 100, acc_black ? acc_black->rank : 1, time_mode * 60);
        snprintf(start_msg_b, sizeof(start_msg_b),
            "{\"action\":\"GAME_START\",\"match_id\":%d,\"color\":\"black\",\"opp_name\":\"%s\",\"opp_score\":%d,\"opp_rank\":%d,\"time_control\":%d}",
            room->room_id, white_sess->username, acc_white ? acc_white->score : 100, acc_white ? acc_white->rank : 1, time_mode * 60);

        send_to_session(white_sess, start_msg_w);
        send_to_session(black_sess, start_msg_b);

        broadcast_idle_list();
    }
    else if (strcmp(action, "GET_VALID_MOVES") == 0) {
        if (!sess->logged_in || sess->state != USER_STATE_IN_GAME || sess->room_id <= 0) return;
        GameRoom *room = room_find_by_id(sess->room_id);
        if (!room || !room->active) return;

        bool is_white = (room->white_fd == (int)sess->fd);
        if ((room->board.turn == COLOR_WHITE && !is_white) ||
            (room->board.turn == COLOR_BLACK && is_white)) {
            return;
        }

        uint8_t sq = (uint8_t)extract_json_int(msg, "sq", 255);
        if (sq >= 64) return;

        uint8_t dests[64];
        int n = chess_get_legal_moves(&room->board, sq, dests, 64);

        char resp[256];
        int offset = snprintf(resp, sizeof(resp), "{\"action\":\"VALID_MOVES\",\"from\":%d,\"dests\":[", sq);
        for (int i = 0; i < n; i++) {
            offset += snprintf(resp + offset, sizeof(resp) - offset, "%s%d", (i > 0) ? "," : "", dests[i]);
        }
        snprintf(resp + offset, sizeof(resp) - offset, "]}");
        send_to_session(sess, resp);
    }
    else if (strcmp(action, "MOVE_REQ") == 0) {
        if (!sess->logged_in || sess->state != USER_STATE_IN_GAME || sess->room_id <= 0) return;
        GameRoom *room = room_find_by_id(sess->room_id);
        if (!room || !room->active) return;

        bool is_white = (room->white_fd == (int)sess->fd);
        if ((room->board.turn == COLOR_WHITE && !is_white) ||
            (room->board.turn == COLOR_BLACK && is_white)) {
            send_to_session(sess, "{\"action\":\"MOVE_REJECT\",\"msg\":\"Not your turn\"}");
            return;
        }

        Move m;
        m.from = (uint8_t)extract_json_int(msg, "from", -1);
        m.to = (uint8_t)extract_json_int(msg, "to", -1);
        m.promo = (int8_t)extract_json_int(msg, "promo", 0);

        char err_msg[128];
        char from_str[4], to_str[4];
        chess_sq_to_str(m.from, from_str);
        chess_sq_to_str(m.to, to_str);

        int8_t cap_target = room->board.squares[m.to];
        if (cap_target == EMPTY && (room->board.squares[m.from] == W_PAWN || room->board.squares[m.from] == B_PAWN)) {
            if (m.to == room->board.en_passant) {
                cap_target = is_white ? B_PAWN : W_PAWN;
            }
        }

        if (!chess_validate_and_apply(&room->board, m, err_msg, sizeof(err_msg))) {
            char rej[256];
            snprintf(rej, sizeof(rej), "{\"action\":\"MOVE_REJECT\",\"msg\":\"%s\"}", err_msg);
            send_to_session(sess, rej);
            return;
        }

        /* Update clock */
        time_t now = time(NULL);
        int elapsed = (int)(now - room->last_move_time);
        if (elapsed < 0) elapsed = 0;
        if (is_white) {
            room->white_time_left -= elapsed;
            if (room->white_time_left < 0) room->white_time_left = 0;
        } else {
            room->black_time_left -= elapsed;
            if (room->black_time_left < 0) room->black_time_left = 0;
        }
        room->last_move_time = now;

        /* Record captured piece */
        if (cap_target != EMPTY) {
            if (is_white) {
                if (room->white_captured_count < 16) {
                    room->white_captured[room->white_captured_count++] = cap_target;
                }
            } else {
                if (room->black_captured_count < 16) {
                    room->black_captured[room->black_captured_count++] = cap_target;
                }
            }
        }

        /* Record move in SAN list */
        if (room->move_count < MAX_MOVES_LOG) {
            snprintf(room->moves_san[room->move_count++], 16, "%s-%s", from_str, to_str);
        }

        bool is_check = false;
        char reason[64] = "";
        GameResult status = chess_evaluate_status(&room->board, &is_check, reason, sizeof(reason));

        char bcast[1536];
        int boff = snprintf(bcast, sizeof(bcast),
            "{\"action\":\"MOVE_BROADCAST\",\"from\":%d,\"to\":%d,\"promo\":%d,\"turn\":\"%s\",\"check\":%s,"
            "\"w_time\":%d,\"b_time\":%d,\"w_caps\":[",
            m.from, m.to, m.promo,
            (room->board.turn == COLOR_WHITE) ? "white" : "black",
            is_check ? "true" : "false",
            room->white_time_left, room->black_time_left);
        for (int i = 0; i < room->white_captured_count; i++) {
            boff += snprintf(bcast + boff, sizeof(bcast) - boff, "%s%d", (i > 0) ? "," : "", room->white_captured[i]);
        }
        boff += snprintf(bcast + boff, sizeof(bcast) - boff, "],\"b_caps\":[");
        for (int i = 0; i < room->black_captured_count; i++) {
            boff += snprintf(bcast + boff, sizeof(bcast) - boff, "%s%d", (i > 0) ? "," : "", room->black_captured[i]);
        }
        boff += snprintf(bcast + boff, sizeof(bcast) - boff, "],\"board\":[");
        for (int i = 0; i < 64; i++) {
            boff += snprintf(bcast + boff, sizeof(bcast) - boff, "%s%d", (i > 0) ? "," : "", room->board.squares[i]);
        }
        snprintf(bcast + boff, sizeof(bcast) - boff, "]}");

        ClientSession *w_sess = find_session_by_fd((socket_t)room->white_fd);
        ClientSession *b_sess = find_session_by_fd((socket_t)room->black_fd);
        if (w_sess) send_to_session(w_sess, bcast);
        if (b_sess) send_to_session(b_sess, bcast);

        if (status != GAME_RESULT_ONGOING) {
            room->result = status;
            snprintf(room->result_reason, sizeof(room->result_reason), "%s", reason);

            int w_delta = 0, b_delta = 0;
            const char *winner_name = "None";
            if (status == GAME_RESULT_WHITE_WIN) {
                w_delta = SCORE_WIN; b_delta = SCORE_LOSS;
                winner_name = room->white_user;
                account_update_score(room->white_user, SCORE_WIN, 1);
                account_update_score(room->black_user, SCORE_LOSS, 2);
            } else if (status == GAME_RESULT_BLACK_WIN) {
                w_delta = SCORE_LOSS; b_delta = SCORE_WIN;
                winner_name = room->black_user;
                account_update_score(room->white_user, SCORE_LOSS, 2);
                account_update_score(room->black_user, SCORE_WIN, 1);
            } else {
                account_update_score(room->white_user, SCORE_DRAW, 0);
                account_update_score(room->black_user, SCORE_DRAW, 0);
            }

            char go_w[256], go_b[256];
            snprintf(go_w, sizeof(go_w),
                "{\"action\":\"GAME_OVER\",\"winner\":\"%s\",\"reason\":\"%s\",\"score_delta\":%d}",
                winner_name, reason, w_delta);
            snprintf(go_b, sizeof(go_b),
                "{\"action\":\"GAME_OVER\",\"winner\":\"%s\",\"reason\":\"%s\",\"score_delta\":%d}",
                winner_name, reason, b_delta);

            if (w_sess) send_to_session(w_sess, go_w);
            if (b_sess) send_to_session(b_sess, go_b);

            /* Generate and transmit match log report file */
            char log_raw[BUFFER_SIZE * 2];
            char log_escaped[BUFFER_SIZE * 2];
            log_generate_match_report(room, log_raw, sizeof(log_raw));
            char log_file[64];
            snprintf(log_file, sizeof(log_file), "match_%d.log", room->room_id);
            log_save_to_disk(log_file, log_raw);

            escape_json_str(log_raw, log_escaped, sizeof(log_escaped));
            char log_msg[BUFFER_SIZE * 2 + 128];
            snprintf(log_msg, sizeof(log_msg),
                "{\"action\":\"LOG_FILE\",\"filename\":\"%s\",\"content\":\"%s\"}",
                log_file, log_escaped);

            if (w_sess) send_to_session(w_sess, log_msg);
            if (b_sess) send_to_session(b_sess, log_msg);

            if (w_sess) { w_sess->state = USER_STATE_IDLE; w_sess->room_id = 0; }
            if (b_sess) { b_sess->state = USER_STATE_IDLE; b_sess->room_id = 0; }
            room_close(room);
            broadcast_idle_list();
        }
    }
    else if (strcmp(action, "RESIGN") == 0) {
        if (!sess->logged_in || sess->state != USER_STATE_IN_GAME || sess->room_id <= 0) return;
        GameRoom *room = room_find_by_id(sess->room_id);
        if (!room || !room->active) return;

        bool is_white = (room->white_fd == (int)sess->fd);
        room->result = is_white ? GAME_RESULT_BLACK_WIN : GAME_RESULT_WHITE_WIN;
        snprintf(room->result_reason, sizeof(room->result_reason), "%s resigned", sess->username);

        account_update_score(room->white_user, is_white ? SCORE_LOSS : SCORE_WIN, is_white ? 2 : 1);
        account_update_score(room->black_user, is_white ? SCORE_WIN : SCORE_LOSS, is_white ? 1 : 2);

        ClientSession *w_sess = find_session_by_fd((socket_t)room->white_fd);
        ClientSession *b_sess = find_session_by_fd((socket_t)room->black_fd);

        const char *winner_name = is_white ? room->black_user : room->white_user;
        char go_w[256], go_b[256];
        snprintf(go_w, sizeof(go_w),
            "{\"action\":\"GAME_OVER\",\"winner\":\"%s\",\"reason\":\"%s\",\"score_delta\":%d}",
            winner_name, room->result_reason, is_white ? -3 : 3);
        snprintf(go_b, sizeof(go_b),
            "{\"action\":\"GAME_OVER\",\"winner\":\"%s\",\"reason\":\"%s\",\"score_delta\":%d}",
            winner_name, room->result_reason, is_white ? 3 : -3);

        if (w_sess) send_to_session(w_sess, go_w);
        if (b_sess) send_to_session(b_sess, go_b);

        /* Generate and transfer match log report */
        char log_raw[BUFFER_SIZE * 2];
        char log_escaped[BUFFER_SIZE * 2];
        log_generate_match_report(room, log_raw, sizeof(log_raw));
        char log_file[64];
        snprintf(log_file, sizeof(log_file), "match_%d.log", room->room_id);
        log_save_to_disk(log_file, log_raw);

        escape_json_str(log_raw, log_escaped, sizeof(log_escaped));
        char log_msg[BUFFER_SIZE * 2 + 128];
        snprintf(log_msg, sizeof(log_msg),
            "{\"action\":\"LOG_FILE\",\"filename\":\"%s\",\"content\":\"%s\"}",
            log_file, log_escaped);

        if (w_sess) send_to_session(w_sess, log_msg);
        if (b_sess) send_to_session(b_sess, log_msg);

        if (w_sess) { w_sess->state = USER_STATE_IDLE; w_sess->room_id = 0; }
        if (b_sess) { b_sess->state = USER_STATE_IDLE; b_sess->room_id = 0; }
        room_close(room);
        broadcast_idle_list();
    }
}

static bool perform_ws_handshake(ClientSession *sess) {
    char *key_hdr = strstr(sess->rx_buf, "Sec-WebSocket-Key:");
    if (!key_hdr) return false;

    key_hdr += 18;
    while (*key_hdr == ' ') key_hdr++;
    char key[64];
    int ki = 0;
    while (*key_hdr && *key_hdr != '\r' && *key_hdr != '\n' && ki < 63) {
        key[ki++] = *key_hdr++;
    }
    key[ki] = '\0';

    char combined[128];
    snprintf(combined, sizeof(combined), "%s%s", key, WS_GUID);

    SHA1_CTX ctx;
    uint8_t digest[20];
    sha1_init(&ctx);
    sha1_update(&ctx, (const uint8_t *)combined, strlen(combined));
    sha1_final(&ctx, digest);

    char accept_key[64];
    base64_encode(digest, 20, accept_key, sizeof(accept_key));

    char response[256];
    int len = snprintf(response, sizeof(response),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n\r\n",
        accept_key);

    send_all(sess->fd, response, (size_t)len);
    sess->conn_type = CONN_WEBSOCKET_OPEN;
    sess->rx_len = 0;
    return true;
}

static void parse_ws_frames(ClientSession *sess) {
    while (sess->rx_len >= 2) {
        uint8_t *data = (uint8_t *)sess->rx_buf;
        uint8_t b1 = data[0];
        uint8_t b2 = data[1];

        uint8_t opcode = b1 & 0x0F;
        bool masked = (b2 & 0x80) != 0;
        uint64_t payload_len = b2 & 0x7F;

        size_t offset = 2;
        if (payload_len == 126) {
            if (sess->rx_len < 4) return;
            payload_len = ((uint64_t)data[2] << 8) | data[3];
            offset = 4;
        } else if (payload_len == 127) {
            if (sess->rx_len < 10) return;
            payload_len = 0;
            for (int i = 0; i < 8; i++) payload_len = (payload_len << 8) | data[2 + i];
            offset = 10;
        }

        uint8_t mask[4] = {0};
        if (masked) {
            if (sess->rx_len < offset + 4) return;
            memcpy(mask, data + offset, 4);
            offset += 4;
        }

        size_t total_frame = offset + (size_t)payload_len;
        if (sess->rx_len < total_frame) return;

        if (opcode == 0x08) { /* WS Close */
            handle_client_disconnect(sess);
            return;
        }

        if (opcode == 0x01) { /* Text Frame */
            char *payload = (char *)malloc(payload_len + 1);
            if (payload) {
                for (size_t i = 0; i < payload_len; i++) {
                    payload[i] = data[offset + i] ^ (masked ? mask[i % 4] : 0);
                }
                payload[payload_len] = '\0';
                process_app_message(sess, payload);
                free(payload);
            }
        }

        memmove(sess->rx_buf, sess->rx_buf + total_frame, sess->rx_len - total_frame);
        sess->rx_len -= total_frame;
    }
}

static void check_room_timeouts(void) {
    time_t now = time(NULL);
    for (int i = 0; i < MAX_ROOMS; i++) {
        GameRoom *room = room_get_by_index(i);
        if (!room || !room->active || room->result != GAME_RESULT_ONGOING) continue;

        int elapsed = (int)(now - room->last_move_time);
        int w_left = room->white_time_left;
        int b_left = room->black_time_left;
        if (room->board.turn == COLOR_WHITE) {
            w_left -= elapsed;
        } else {
            b_left -= elapsed;
        }

        if (w_left <= 0 || b_left <= 0) {
            bool white_timeout = (w_left <= 0);
            room->result = white_timeout ? GAME_RESULT_BLACK_WIN : GAME_RESULT_WHITE_WIN;
            snprintf(room->result_reason, sizeof(room->result_reason), "%s hết giờ",
                     white_timeout ? room->white_user : room->black_user);
            account_update_score(room->white_user, white_timeout ? SCORE_LOSS : SCORE_WIN, white_timeout ? 2 : 1);
            account_update_score(room->black_user, white_timeout ? SCORE_WIN : SCORE_LOSS, white_timeout ? 1 : 2);

            ClientSession *w_sess = find_session_by_fd((socket_t)room->white_fd);
            ClientSession *b_sess = find_session_by_fd((socket_t)room->black_fd);
            const char *winner_name = white_timeout ? room->black_user : room->white_user;

            char go_w[256], go_b[256];
            snprintf(go_w, sizeof(go_w),
                "{\"action\":\"GAME_OVER\",\"winner\":\"%s\",\"reason\":\"%s\",\"score_delta\":%d}",
                winner_name, room->result_reason, white_timeout ? -3 : 3);
            snprintf(go_b, sizeof(go_b),
                "{\"action\":\"GAME_OVER\",\"winner\":\"%s\",\"reason\":\"%s\",\"score_delta\":%d}",
                winner_name, room->result_reason, white_timeout ? 3 : -3);

            if (w_sess) send_to_session(w_sess, go_w);
            if (b_sess) send_to_session(b_sess, go_b);

            char log_raw[BUFFER_SIZE * 2];
            char log_escaped[BUFFER_SIZE * 2];
            log_generate_match_report(room, log_raw, sizeof(log_raw));
            char log_file[64];
            snprintf(log_file, sizeof(log_file), "match_%d.log", room->room_id);
            log_save_to_disk(log_file, log_raw);
            escape_json_str(log_raw, log_escaped, sizeof(log_escaped));

            char log_msg[BUFFER_SIZE * 2 + 128];
            snprintf(log_msg, sizeof(log_msg),
                "{\"action\":\"LOG_FILE\",\"filename\":\"%s\",\"content\":\"%s\"}",
                log_file, log_escaped);

            if (w_sess) send_to_session(w_sess, log_msg);
            if (b_sess) send_to_session(b_sess, log_msg);

            if (w_sess) { w_sess->state = USER_STATE_IDLE; w_sess->room_id = 0; }
            if (b_sess) { b_sess->state = USER_STATE_IDLE; b_sess->room_id = 0; }
            room_close(room);
            broadcast_idle_list();
        }
    }
}

void net_run_loop(socket_t server_fd) {
    memset(g_sessions, 0, sizeof(g_sessions));
    printf("[*] Server listening on port %d with select() I/O multiplexing...\n", DEFAULT_PORT);

    while (1) {
        static time_t last_timeout_check = 0;
        time_t now_sec = time(NULL);
        if (now_sec != last_timeout_check) {
            last_timeout_check = now_sec;
            check_room_timeouts();
        }

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        socket_t max_fd = server_fd;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (g_sessions[i].active) {
                FD_SET(g_sessions[i].fd, &read_fds);
                if (g_sessions[i].fd > max_fd) max_fd = g_sessions[i].fd;
            }
        }

        struct timeval tv = {0, 100000}; /* 100ms timeout */
        int activity = select((int)(max_fd + 1), &read_fds, NULL, NULL, &tv);
        if (activity < 0) break;

        /* Check new connections */
        if (FD_ISSET(server_fd, &read_fds)) {
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            socket_t new_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
            if (new_fd != INVALID_SOCK) {
                set_nonblocking(new_fd);
                bool slot_found = false;
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (!g_sessions[i].active) {
                        g_sessions[i].fd = new_fd;
                        g_sessions[i].active = true;
                        g_sessions[i].conn_type = CONN_WEBSOCKET_HANDSHAKING;
                        g_sessions[i].rx_len = 0;
                        g_sessions[i].logged_in = false;
                        g_sessions[i].state = USER_STATE_DISCONNECTED;
                        g_sessions[i].room_id = 0;
                        inet_ntop(AF_INET, &client_addr.sin_addr, g_sessions[i].ip, sizeof(g_sessions[i].ip));
                        g_sessions[i].port = ntohs(client_addr.sin_port);
                        printf("[+] New client connected from %s:%d\n", g_sessions[i].ip, g_sessions[i].port);
                        slot_found = true;
                        break;
                    }
                }
                if (!slot_found) {
                    CLOSE_SOCK(new_fd);
                }
            }
        }

        /* Check active client sockets */
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (g_sessions[i].active && FD_ISSET(g_sessions[i].fd, &read_fds)) {
                ClientSession *sess = &g_sessions[i];
                int space = (int)(sizeof(sess->rx_buf) - sess->rx_len - 1);
                if (space <= 0) {
                    handle_client_disconnect(sess);
                    continue;
                }
                int bytes = recv(sess->fd, sess->rx_buf + sess->rx_len, space, 0);
                if (bytes <= 0) {
                    handle_client_disconnect(sess);
                    continue;
                }
                sess->rx_len += (size_t)bytes;
                sess->rx_buf[sess->rx_len] = '\0';

                if (sess->conn_type == CONN_WEBSOCKET_HANDSHAKING) {
                    if (sess->rx_buf[0] == '{') {
                        sess->conn_type = CONN_RAW_TCP;
                    } else if (strstr(sess->rx_buf, "\r\n\r\n")) {
                        if (strstr(sess->rx_buf, "Upgrade: websocket") ||
                            strstr(sess->rx_buf, "Upgrade: WebSocket")) {
                            perform_ws_handshake(sess);
                        } else {
                            /* Raw TCP line mode */
                            sess->conn_type = CONN_RAW_TCP;
                        }
                    }
                }

                if (sess->conn_type == CONN_WEBSOCKET_OPEN) {
                    parse_ws_frames(sess);
                } else if (sess->conn_type == CONN_RAW_TCP) {
                    char *line_end = strchr(sess->rx_buf, '\n');
                    while (line_end) {
                        *line_end = '\0';
                        process_app_message(sess, sess->rx_buf);
                        size_t consumed = (size_t)(line_end - sess->rx_buf + 1);
                        memmove(sess->rx_buf, sess->rx_buf + consumed, sess->rx_len - consumed);
                        sess->rx_len -= consumed;
                        sess->rx_buf[sess->rx_len] = '\0';
                        line_end = strchr(sess->rx_buf, '\n');
                    }
                }
            }
        }
    }
}
