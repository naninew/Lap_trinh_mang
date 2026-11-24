#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

#define DEFAULT_PORT 8888
#define BUFFER_SIZE 4096
#define MAX_CLIENTS 64
#define MAX_USERNAME_LEN 32
#define MAX_PASSWORD_LEN 64
#define MAX_FILENAME_LEN 128
#define RANK_DIFF_LIMIT 10

#define SCORE_WIN 3
#define SCORE_DRAW 0
#define SCORE_LOSS -3

typedef enum {
    USER_STATE_DISCONNECTED = 0,
    USER_STATE_IDLE,
    USER_STATE_CHALLENGING,
    USER_STATE_IN_GAME
} UserState;

#endif
