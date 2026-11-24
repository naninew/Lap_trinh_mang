#include "game_room.h"
#include <string.h>

static GameRoom g_rooms[MAX_ROOMS];
static int g_next_room_id = 1;

void room_init_all(void) {
    memset(g_rooms, 0, sizeof(g_rooms));
}

GameRoom* room_create(int white_fd, const char *white_user, const char *white_ip,
                      int black_fd, const char *black_user, const char *black_ip,
                      int time_mode_minutes) {
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (!g_rooms[i].active) {
            GameRoom *r = &g_rooms[i];
            memset(r, 0, sizeof(GameRoom));
            r->room_id = g_next_room_id++;
            r->active = true;
            r->white_fd = white_fd;
            r->black_fd = black_fd;
            strncpy(r->white_user, white_user, sizeof(r->white_user) - 1);
            strncpy(r->black_user, black_user, sizeof(r->black_user) - 1);
            strncpy(r->white_ip, white_ip, sizeof(r->white_ip) - 1);
            strncpy(r->black_ip, black_ip, sizeof(r->black_ip) - 1);
            r->start_time = time(NULL);
            r->time_mode = (time_mode_minutes > 0) ? time_mode_minutes : 10;
            r->white_time_left = r->time_mode * 60;
            r->black_time_left = r->time_mode * 60;
            r->last_move_time = r->start_time;
            r->white_captured_count = 0;
            r->black_captured_count = 0;
            r->move_count = 0;
            r->result = GAME_RESULT_ONGOING;
            chess_init(&r->board);
            return r;
        }
    }
    return NULL;
}

GameRoom* room_find_by_id(int room_id) {
    if (room_id <= 0) return NULL;
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (g_rooms[i].active && g_rooms[i].room_id == room_id) {
            return &g_rooms[i];
        }
    }
    return NULL;
}

GameRoom* room_find_by_fd(int fd) {
    if (fd <= 0) return NULL;
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (g_rooms[i].active && (g_rooms[i].white_fd == fd || g_rooms[i].black_fd == fd)) {
            return &g_rooms[i];
        }
    }
    return NULL;
}

GameRoom* room_find_by_user(const char *username) {
    if (!username) return NULL;
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (g_rooms[i].active &&
            (strcmp(g_rooms[i].white_user, username) == 0 ||
             strcmp(g_rooms[i].black_user, username) == 0)) {
            return &g_rooms[i];
        }
    }
    return NULL;
}

GameRoom* room_get_by_index(int idx) {
    if (idx < 0 || idx >= MAX_ROOMS) return NULL;
    return &g_rooms[idx];
}

void room_close(GameRoom *room) {
    if (room) {
        room->active = false;
        room->end_time = time(NULL);
    }
}
