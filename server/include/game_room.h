#ifndef GAME_ROOM_H
#define GAME_ROOM_H

#include <time.h>
#include "chess_engine.h"
#include "protocol.h"

#define MAX_ROOMS 32
#define MAX_MOVES_LOG 256

typedef struct {
    int room_id;
    bool active;
    int white_fd;
    int black_fd;
    char white_user[MAX_USERNAME_LEN];
    char black_user[MAX_USERNAME_LEN];
    char white_ip[64];
    char black_ip[64];
    time_t start_time;
    time_t end_time;
    int time_mode;
    int white_time_left;
    int black_time_left;
    time_t last_move_time;
    int8_t white_captured[16];
    int white_captured_count;
    int8_t black_captured[16];
    int black_captured_count;
    ChessBoard board;
    char moves_san[MAX_MOVES_LOG][16];
    int move_count;
    GameResult result;
    char result_reason[64];
} GameRoom;

void room_init_all(void);
GameRoom* room_create(int white_fd, const char *white_user, const char *white_ip,
                      int black_fd, const char *black_user, const char *black_ip,
                      int time_mode_minutes);
GameRoom* room_find_by_id(int room_id);
GameRoom* room_find_by_fd(int fd);
GameRoom* room_find_by_user(const char *username);
GameRoom* room_get_by_index(int idx);
void room_close(GameRoom *room);

#endif
