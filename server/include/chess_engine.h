#ifndef CHESS_ENGINE_H
#define CHESS_ENGINE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define COLOR_WHITE 0
#define COLOR_BLACK 1

#define EMPTY 0
#define W_PAWN 1
#define W_KNIGHT 2
#define W_BISHOP 3
#define W_ROOK 4
#define W_QUEEN 5
#define W_KING 6

#define B_PAWN -1
#define B_KNIGHT -2
#define B_BISHOP -3
#define B_ROOK -4
#define B_QUEEN -5
#define B_KING -6

#define CASTLE_WK 1
#define CASTLE_WQ 2
#define CASTLE_BK 4
#define CASTLE_BQ 8

typedef enum {
    GAME_RESULT_ONGOING = 0,
    GAME_RESULT_WHITE_WIN,
    GAME_RESULT_BLACK_WIN,
    GAME_RESULT_DRAW
} GameResult;

typedef struct {
    int8_t squares[64];
    uint8_t turn;
    uint8_t castling;
    int8_t en_passant;
    int halfmove_clock;
    int fullmove_number;
} ChessBoard;

typedef struct {
    uint8_t from;
    uint8_t to;
    int8_t promo;
} Move;

void chess_init(ChessBoard *board);
bool chess_is_square_attacked(const ChessBoard *board, int sq, int by_color);
bool chess_is_move_pseudo_legal(const ChessBoard *board, Move m);
bool chess_validate_and_apply(ChessBoard *board, Move m, char *err_msg, size_t err_size);
bool chess_has_legal_moves(const ChessBoard *board, int color);
int chess_get_legal_moves(const ChessBoard *board, uint8_t from_sq, uint8_t *dest_squares, int max_dests);
GameResult chess_evaluate_status(const ChessBoard *board, bool *is_check, char *reason, size_t reason_size);
void chess_sq_to_str(int sq, char *out);
int chess_str_to_sq(const char *str);

#endif
