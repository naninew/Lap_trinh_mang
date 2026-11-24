#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "chess_engine.h"

static void test_initial_board(void) {
    ChessBoard b;
    chess_init(&b);
    assert(b.turn == COLOR_WHITE);
    assert(b.squares[4] == W_KING);
    assert(b.squares[60] == B_KING);
    assert(b.squares[12] == W_PAWN); /* e2 */
    assert(b.squares[52] == B_PAWN); /* e7 */
    printf("[PASS] test_initial_board\n");
}

static void test_pawn_and_knight_moves(void) {
    ChessBoard b;
    chess_init(&b);
    char err[64];

    /* e2 -> e4 */
    Move m1 = {12, 28, 0};
    assert(chess_validate_and_apply(&b, m1, err, sizeof(err)));
    assert(b.turn == COLOR_BLACK);
    assert(b.squares[12] == EMPTY);
    assert(b.squares[28] == W_PAWN);

    /* Illegal: Black knight g8 -> g6 */
    Move illegal_kn = {62, 46, 0};
    assert(!chess_validate_and_apply(&b, illegal_kn, err, sizeof(err)));

    /* Legal: Black knight g8 -> f6 (62 -> 45) */
    Move legal_kn = {62, 45, 0};
    assert(chess_validate_and_apply(&b, legal_kn, err, sizeof(err)));
    assert(b.turn == COLOR_WHITE);
    assert(b.squares[45] == B_KNIGHT);

    printf("[PASS] test_pawn_and_knight_moves\n");
}

static void test_scholars_mate(void) {
    ChessBoard b;
    chess_init(&b);
    char err[64];

    /* 1. e4 (12->28) e5 (52->36) */
    Move m1 = {12, 28, 0}; assert(chess_validate_and_apply(&b, m1, err, sizeof(err)));
    Move m2 = {52, 36, 0}; assert(chess_validate_and_apply(&b, m2, err, sizeof(err)));

    /* 2. Bc4 (5->26) Nc6 (57->42) */
    Move m3 = {5, 26, 0}; assert(chess_validate_and_apply(&b, m3, err, sizeof(err)));
    Move m4 = {57, 42, 0}; assert(chess_validate_and_apply(&b, m4, err, sizeof(err)));

    /* 3. Qh5 (3->39) Nf6 (62->45) */
    Move m5 = {3, 39, 0}; assert(chess_validate_and_apply(&b, m5, err, sizeof(err)));
    Move m6 = {62, 45, 0}; assert(chess_validate_and_apply(&b, m6, err, sizeof(err)));

    /* 4. Qxf7# (39->53) Checkmate! */
    Move m7 = {39, 53, 0}; assert(chess_validate_and_apply(&b, m7, err, sizeof(err)));

    bool is_check = false;
    char reason[64];
    GameResult res = chess_evaluate_status(&b, &is_check, reason, sizeof(reason));
    assert(res == GAME_RESULT_WHITE_WIN);
    assert(is_check == true);
    assert(strstr(reason, "Checkmate") != NULL);

    printf("[PASS] test_scholars_mate: Successfully verified Checkmate!\n");
}

static void test_legal_moves_generator(void) {
    ChessBoard b;
    chess_init(&b);

    uint8_t dests[64];
    /* e2 pawn (sq 12) -> should have e3 (20) and e4 (28) */
    int n_e2 = chess_get_legal_moves(&b, 12, dests, 64);
    assert(n_e2 == 2);
    assert((dests[0] == 20 && dests[1] == 28) || (dests[0] == 28 && dests[1] == 20));

    /* b1 knight (sq 1) -> should have a3 (16) and c3 (18) */
    int n_b1 = chess_get_legal_moves(&b, 1, dests, 64);
    assert(n_b1 == 2);
    assert((dests[0] == 16 && dests[1] == 18) || (dests[0] == 18 && dests[1] == 16));

    /* e1 king (sq 4) -> 0 moves initially */
    int n_e1 = chess_get_legal_moves(&b, 4, dests, 64);
    assert(n_e1 == 0);

    printf("[PASS] test_legal_moves_generator\n");
}

static void test_castling(void) {
    ChessBoard b;
    chess_init(&b);
    char err[64];

    /* White Kingside Castling: clear f1 (5) and g1 (6) */
    b.squares[5] = EMPTY;
    b.squares[6] = EMPTY;
    Move wk_castle = {4, 6, 0};
    assert(chess_validate_and_apply(&b, wk_castle, err, sizeof(err)));
    assert(b.squares[4] == EMPTY);
    assert(b.squares[6] == W_KING);
    assert(b.squares[7] == EMPTY);
    assert(b.squares[5] == W_ROOK);

    /* Black Kingside Castling: clear f8 (61) and g8 (62) */
    b.squares[61] = EMPTY;
    b.squares[62] = EMPTY;
    Move bk_castle = {60, 62, 0};
    assert(chess_validate_and_apply(&b, bk_castle, err, sizeof(err)));
    assert(b.squares[60] == EMPTY);
    assert(b.squares[62] == B_KING);
    assert(b.squares[63] == EMPTY);
    assert(b.squares[61] == B_ROOK);

    /* Queenside Castling */
    chess_init(&b);
    b.squares[1] = EMPTY; /* b1 */
    b.squares[2] = EMPTY; /* c1 */
    b.squares[3] = EMPTY; /* d1 */
    Move wq_castle = {4, 2, 0};
    assert(chess_validate_and_apply(&b, wq_castle, err, sizeof(err)));
    assert(b.squares[4] == EMPTY);
    assert(b.squares[2] == W_KING);
    assert(b.squares[0] == EMPTY);
    assert(b.squares[3] == W_ROOK);

    printf("[PASS] test_castling: Verified White & Black kingside and queenside castling with rook moves!\n");
}

int main(void) {
    printf("========================================\n");
    printf("    RUNNING C CHESS ENGINE UNIT TESTS   \n");
    printf("========================================\n");

    test_initial_board();
    test_pawn_and_knight_moves();
    test_scholars_mate();
    test_legal_moves_generator();
    test_castling();

    printf("========================================\n");
    printf("     ALL CHESS ENGINE TESTS PASSED!     \n");
    printf("========================================\n");
    return 0;
}
