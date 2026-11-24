#include "chess_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void chess_init(ChessBoard *board) {
    memset(board, 0, sizeof(ChessBoard));
    board->turn = COLOR_WHITE;
    board->castling = CASTLE_WK | CASTLE_WQ | CASTLE_BK | CASTLE_BQ;
    board->en_passant = -1;
    board->halfmove_clock = 0;
    board->fullmove_number = 1;

    board->squares[0] = W_ROOK;   board->squares[1] = W_KNIGHT;
    board->squares[2] = W_BISHOP; board->squares[3] = W_QUEEN;
    board->squares[4] = W_KING;   board->squares[5] = W_BISHOP;
    board->squares[6] = W_KNIGHT; board->squares[7] = W_ROOK;
    for (int i = 8; i < 16; i++) board->squares[i] = W_PAWN;

    for (int i = 48; i < 56; i++) board->squares[i] = B_PAWN;
    board->squares[56] = B_ROOK;   board->squares[57] = B_KNIGHT;
    board->squares[58] = B_BISHOP; board->squares[59] = B_QUEEN;
    board->squares[60] = B_KING;   board->squares[61] = B_BISHOP;
    board->squares[62] = B_KNIGHT; board->squares[63] = B_ROOK;
}

static inline int piece_color(int8_t p) {
    if (p > 0) return COLOR_WHITE;
    if (p < 0) return COLOR_BLACK;
    return -1;
}

void chess_sq_to_str(int sq, char *out) {
    out[0] = 'a' + (sq % 8);
    out[1] = '1' + (sq / 8);
    out[2] = '\0';
}

int chess_str_to_sq(const char *str) {
    if (!str || strlen(str) < 2) return -1;
    int f = str[0] - 'a';
    int r = str[1] - '1';
    if (f < 0 || f > 7 || r < 0 || r > 7) return -1;
    return r * 8 + f;
}

bool chess_is_square_attacked(const ChessBoard *board, int sq, int by_color) {
    int r = sq / 8, f = sq % 8;

    /* Pawn attacks */
    if (by_color == COLOR_WHITE) {
        if (r > 0) {
            if (f > 0 && board->squares[(r - 1) * 8 + (f - 1)] == W_PAWN) return true;
            if (f < 7 && board->squares[(r - 1) * 8 + (f + 1)] == W_PAWN) return true;
        }
    } else {
        if (r < 7) {
            if (f > 0 && board->squares[(r + 1) * 8 + (f - 1)] == B_PAWN) return true;
            if (f < 7 && board->squares[(r + 1) * 8 + (f + 1)] == B_PAWN) return true;
        }
    }

    /* Knight attacks */
    static const int kn_moves[8][2] = {
        {-2,-1}, {-2,1}, {-1,-2}, {-1,2}, {1,-2}, {1,2}, {2,-1}, {2,1}
    };
    int target_kn = (by_color == COLOR_WHITE) ? W_KNIGHT : B_KNIGHT;
    for (int i = 0; i < 8; i++) {
        int nr = r + kn_moves[i][0];
        int nf = f + kn_moves[i][1];
        if (nr >= 0 && nr <= 7 && nf >= 0 && nf <= 7) {
            if (board->squares[nr * 8 + nf] == target_kn) return true;
        }
    }

    /* King attacks */
    static const int k_moves[8][2] = {
        {-1,-1}, {-1,0}, {-1,1}, {0,-1}, {0,1}, {1,-1}, {1,0}, {1,1}
    };
    int target_k = (by_color == COLOR_WHITE) ? W_KING : B_KING;
    for (int i = 0; i < 8; i++) {
        int nr = r + k_moves[i][0];
        int nf = f + k_moves[i][1];
        if (nr >= 0 && nr <= 7 && nf >= 0 && nf <= 7) {
            if (board->squares[nr * 8 + nf] == target_k) return true;
        }
    }

    /* Straight rays (Rook / Queen) */
    static const int straight[4][2] = {{-1,0}, {1,0}, {0,-1}, {0,1}};
    int target_r = (by_color == COLOR_WHITE) ? W_ROOK : B_ROOK;
    int target_q = (by_color == COLOR_WHITE) ? W_QUEEN : B_QUEEN;
    for (int i = 0; i < 4; i++) {
        int nr = r + straight[i][0], nf = f + straight[i][1];
        while (nr >= 0 && nr <= 7 && nf >= 0 && nf <= 7) {
            int8_t p = board->squares[nr * 8 + nf];
            if (p != EMPTY) {
                if (p == target_r || p == target_q) return true;
                break;
            }
            nr += straight[i][0]; nf += straight[i][1];
        }
    }

    /* Diagonal rays (Bishop / Queen) */
    static const int diag[4][2] = {{-1,-1}, {-1,1}, {1,-1}, {1,1}};
    int target_b = (by_color == COLOR_WHITE) ? W_BISHOP : B_BISHOP;
    for (int i = 0; i < 4; i++) {
        int nr = r + diag[i][0], nf = f + diag[i][1];
        while (nr >= 0 && nr <= 7 && nf >= 0 && nf <= 7) {
            int8_t p = board->squares[nr * 8 + nf];
            if (p != EMPTY) {
                if (p == target_b || p == target_q) return true;
                break;
            }
            nr += diag[i][0]; nf += diag[i][1];
        }
    }

    return false;
}

bool chess_is_move_pseudo_legal(const ChessBoard *board, Move m) {
    if (m.from > 63 || m.to > 63 || m.from == m.to) return false;
    int8_t p = board->squares[m.from];
    if (p == EMPTY) return false;
    if (piece_color(p) != board->turn) return false;

    int8_t dest = board->squares[m.to];
    if (dest != EMPTY && piece_color(dest) == board->turn) return false;

    int from_r = m.from / 8, from_f = m.from % 8;
    int to_r = m.to / 8, to_f = m.to % 8;
    int dr = to_r - from_r;
    int df = to_f - from_f;
    int abs_dr = abs(dr), abs_df = abs(df);

    switch (abs(p)) {
        case W_PAWN: {
            int dir = (p > 0) ? 1 : -1;
            int start_r = (p > 0) ? 1 : 6;
            if (df == 0) {
                if (dr == dir && dest == EMPTY) return true;
                if (dr == 2 * dir && from_r == start_r && dest == EMPTY &&
                    board->squares[(from_r + dir) * 8 + from_f] == EMPTY) return true;
            } else if (abs_df == 1 && dr == dir) {
                if (dest != EMPTY && piece_color(dest) != board->turn) return true;
                if (m.to == board->en_passant) return true;
            }
            return false;
        }
        case W_KNIGHT:
            return (abs_dr == 1 && abs_df == 2) || (abs_dr == 2 && abs_df == 1);
        case W_BISHOP:
            if (abs_dr != abs_df) return false;
            for (int s = 1; s < abs_dr; s++) {
                int cr = from_r + s * (dr > 0 ? 1 : -1);
                int cf = from_f + s * (df > 0 ? 1 : -1);
                if (board->squares[cr * 8 + cf] != EMPTY) return false;
            }
            return true;
        case W_ROOK:
            if (dr != 0 && df != 0) return false;
            if (dr != 0) {
                for (int s = 1; s < abs_dr; s++) {
                    int cr = from_r + s * (dr > 0 ? 1 : -1);
                    if (board->squares[cr * 8 + from_f] != EMPTY) return false;
                }
            } else {
                for (int s = 1; s < abs_df; s++) {
                    int cf = from_f + s * (df > 0 ? 1 : -1);
                    if (board->squares[from_r * 8 + cf] != EMPTY) return false;
                }
            }
            return true;
        case W_QUEEN:
            if (abs_dr == abs_df) {
                for (int s = 1; s < abs_dr; s++) {
                    int cr = from_r + s * (dr > 0 ? 1 : -1);
                    int cf = from_f + s * (df > 0 ? 1 : -1);
                    if (board->squares[cr * 8 + cf] != EMPTY) return false;
                }
                return true;
            }
            if (dr == 0 || df == 0) {
                if (dr != 0) {
                    for (int s = 1; s < abs_dr; s++) {
                        int cr = from_r + s * (dr > 0 ? 1 : -1);
                        if (board->squares[cr * 8 + from_f] != EMPTY) return false;
                    }
                } else {
                    for (int s = 1; s < abs_df; s++) {
                        int cf = from_f + s * (df > 0 ? 1 : -1);
                        if (board->squares[from_r * 8 + cf] != EMPTY) return false;
                    }
                }
                return true;
            }
            return false;
        case W_KING:
            if (abs_dr <= 1 && abs_df <= 1) return true;
            /* Castling */
            if (abs_dr == 0 && abs_df == 2) {
                int opp = (board->turn == COLOR_WHITE) ? COLOR_BLACK : COLOR_WHITE;
                if (chess_is_square_attacked(board, m.from, opp)) return false;
                if (board->turn == COLOR_WHITE && m.from == 4) {
                    if (m.to == 6 && (board->castling & CASTLE_WK)) {
                        if (board->squares[5] == EMPTY && board->squares[6] == EMPTY &&
                            !chess_is_square_attacked(board, 5, opp) &&
                            !chess_is_square_attacked(board, 6, opp)) return true;
                    }
                    if (m.to == 2 && (board->castling & CASTLE_WQ)) {
                        if (board->squares[1] == EMPTY && board->squares[2] == EMPTY &&
                            board->squares[3] == EMPTY &&
                            !chess_is_square_attacked(board, 3, opp) &&
                            !chess_is_square_attacked(board, 2, opp)) return true;
                    }
                } else if (board->turn == COLOR_BLACK && m.from == 60) {
                    if (m.to == 62 && (board->castling & CASTLE_BK)) {
                        if (board->squares[61] == EMPTY && board->squares[62] == EMPTY &&
                            !chess_is_square_attacked(board, 61, opp) &&
                            !chess_is_square_attacked(board, 62, opp)) return true;
                    }
                    if (m.to == 58 && (board->castling & CASTLE_BQ)) {
                        if (board->squares[57] == EMPTY && board->squares[58] == EMPTY &&
                            board->squares[59] == EMPTY &&
                            !chess_is_square_attacked(board, 59, opp) &&
                            !chess_is_square_attacked(board, 58, opp)) return true;
                    }
                }
            }
            return false;
    }
    return false;
}

static void apply_move_internal(ChessBoard *board, Move m) {
    int8_t p = board->squares[m.from];
    int from_r = m.from / 8, from_f = m.from % 8;
    int to_r = m.to / 8, to_f = m.to % 8;

    /* En passant capture */
    if (abs(p) == W_PAWN && m.to == board->en_passant) {
        int cap_sq = (board->turn == COLOR_WHITE) ? (m.to - 8) : (m.to + 8);
        board->squares[cap_sq] = EMPTY;
    }

    /* Set next en passant square */
    board->en_passant = -1;
    if (abs(p) == W_PAWN && abs(to_r - from_r) == 2) {
        board->en_passant = (board->turn == COLOR_WHITE) ? (m.from + 8) : (m.from - 8);
    }

    /* Castling rook movement */
    if (abs(p) == W_KING && abs(to_f - from_f) == 2) {
        if (m.to == 6) { board->squares[7] = EMPTY; board->squares[5] = W_ROOK; }
        else if (m.to == 2) { board->squares[0] = EMPTY; board->squares[3] = W_ROOK; }
        else if (m.to == 62) { board->squares[63] = EMPTY; board->squares[61] = B_ROOK; }
        else if (m.to == 58) { board->squares[56] = EMPTY; board->squares[59] = B_ROOK; }
    }

    /* Update castling rights */
    if (m.from == 4 || m.to == 4) board->castling &= ~(CASTLE_WK | CASTLE_WQ);
    if (m.from == 60 || m.to == 60) board->castling &= ~(CASTLE_BK | CASTLE_BQ);
    if (m.from == 0 || m.to == 0) board->castling &= ~CASTLE_WQ;
    if (m.from == 7 || m.to == 7) board->castling &= ~CASTLE_WK;
    if (m.from == 56 || m.to == 56) board->castling &= ~CASTLE_BQ;
    if (m.from == 63 || m.to == 63) board->castling &= ~CASTLE_BK;

    /* Move piece and handle promotion */
    board->squares[m.from] = EMPTY;
    if (abs(p) == W_PAWN && (to_r == 7 || to_r == 0)) {
        int8_t promo_piece = (m.promo != 0) ? abs(m.promo) : W_QUEEN;
        board->squares[m.to] = (board->turn == COLOR_WHITE) ? promo_piece : -promo_piece;
    } else {
        board->squares[m.to] = p;
    }

    if (board->turn == COLOR_BLACK) board->fullmove_number++;
    board->turn = (board->turn == COLOR_WHITE) ? COLOR_BLACK : COLOR_WHITE;
}

bool chess_validate_and_apply(ChessBoard *board, Move m, char *err_msg, size_t err_size) {
    if (!chess_is_move_pseudo_legal(board, m)) {
        snprintf(err_msg, err_size, "Illegal move geometry");
        return false;
    }

    ChessBoard temp = *board;
    int moving_color = board->turn;
    apply_move_internal(&temp, m);

    /* Verify own king is not in check */
    int k_target = (moving_color == COLOR_WHITE) ? W_KING : B_KING;
    int k_sq = -1;
    for (int i = 0; i < 64; i++) {
        if (temp.squares[i] == k_target) { k_sq = i; break; }
    }
    if (k_sq != -1 && chess_is_square_attacked(&temp, k_sq, temp.turn)) {
        snprintf(err_msg, err_size, "Illegal move: King in check");
        return false;
    }

    apply_move_internal(board, m);
    return true;
}

bool chess_has_legal_moves(const ChessBoard *board, int color) {
    ChessBoard test_b = *board;
    test_b.turn = color;

    for (int from = 0; from < 64; from++) {
        if (piece_color(test_b.squares[from]) != color) continue;
        for (int to = 0; to < 64; to++) {
            Move m = {(uint8_t)from, (uint8_t)to, 0};
            if (chess_is_move_pseudo_legal(&test_b, m)) {
                ChessBoard temp = test_b;
                apply_move_internal(&temp, m);
                int k_target = (color == COLOR_WHITE) ? W_KING : B_KING;
                int k_sq = -1;
                for (int i = 0; i < 64; i++) {
                    if (temp.squares[i] == k_target) { k_sq = i; break; }
                }
                if (k_sq != -1 && !chess_is_square_attacked(&temp, k_sq, temp.turn)) {
                    return true;
                }
            }
        }
    }
    return false;
}

int chess_get_legal_moves(const ChessBoard *board, uint8_t from_sq, uint8_t *dest_squares, int max_dests) {
    if (!board || from_sq >= 64 || !dest_squares || max_dests <= 0) return 0;
    int8_t p = board->squares[from_sq];
    if (p == EMPTY) return 0;
    int color = piece_color(p);
    if (color != (int)board->turn) return 0;

    int count = 0;
    for (int to = 0; to < 64 && count < max_dests; to++) {
        Move m = {from_sq, (uint8_t)to, 0};
        if (chess_is_move_pseudo_legal(board, m)) {
            ChessBoard temp = *board;
            apply_move_internal(&temp, m);
            int k_target = (color == COLOR_WHITE) ? W_KING : B_KING;
            int k_sq = -1;
            for (int i = 0; i < 64; i++) {
                if (temp.squares[i] == k_target) { k_sq = i; break; }
            }
            if (k_sq != -1 && !chess_is_square_attacked(&temp, k_sq, temp.turn)) {
                dest_squares[count++] = (uint8_t)to;
            }
        }
    }
    return count;
}

GameResult chess_evaluate_status(const ChessBoard *board, bool *is_check, char *reason, size_t reason_size) {
    int opp_color = (board->turn == COLOR_WHITE) ? COLOR_BLACK : COLOR_WHITE;
    int k_target = (board->turn == COLOR_WHITE) ? W_KING : B_KING;
    int k_sq = -1;
    for (int i = 0; i < 64; i++) {
        if (board->squares[i] == k_target) { k_sq = i; break; }
    }

    bool in_check = (k_sq != -1) && chess_is_square_attacked(board, k_sq, opp_color);
    if (is_check) *is_check = in_check;

    bool has_moves = chess_has_legal_moves(board, board->turn);
    if (!has_moves) {
        if (in_check) {
            if (board->turn == COLOR_WHITE) {
                snprintf(reason, reason_size, "Checkmate: Black wins");
                return GAME_RESULT_BLACK_WIN;
            } else {
                snprintf(reason, reason_size, "Checkmate: White wins");
                return GAME_RESULT_WHITE_WIN;
            }
        } else {
            snprintf(reason, reason_size, "Stalemate: Draw");
            return GAME_RESULT_DRAW;
        }
    }

    /* Insufficient material */
    int w_pieces = 0, b_pieces = 0;
    for (int i = 0; i < 64; i++) {
        int8_t p = board->squares[i];
        if (p > 0 && p != W_KING) w_pieces++;
        if (p < 0 && p != B_KING) b_pieces++;
    }
    if (w_pieces == 0 && b_pieces == 0) {
        snprintf(reason, reason_size, "Insufficient material: Draw");
        return GAME_RESULT_DRAW;
    }

    return GAME_RESULT_ONGOING;
}
