# Chess Engine Specification (Pure C)

## 1. Board Representation

The board is represented as a single-dimensional 64-element array of 8-bit integers (`int8_t squares[64]`):
```
  8 | 56 57 58 59 60 61 62 63   (Black side)
  7 | 48 49 50 51 52 53 54 55
  6 | 40 41 42 43 44 45 46 47
  5 | 32 33 34 35 36 37 38 39
  4 | 24 25 26 27 28 29 30 31
  3 | 16 17 18 19 20 21 22 23
  2 |  8  9 10 11 12 13 14 15
  1 |  0  1  2  3  4  5  6  7   (White side)
    +------------------------
       a  b  c  d  e  f  g  h
```

### 1.1. Piece Encoding
- Empty: `0`
- White pieces (positive): Pawn = `+1`, Knight = `+2`, Bishop = `+3`, Rook = `+4`, Queen = `+5`, King = `+6`
- Black pieces (negative): Pawn = `-1`, Knight = `-2`, Bishop = `-3`, Rook = `-4`, Queen = `-5`, King = `-6`

---

## 2. Move Validation Pipeline

When a client submits `MOVE_REQ(from, to, promo)`, the C engine validates it through 4 sequential stages:

### Stage 1: Basic Validity
- Verify `from` and `to` are within $[0, 63]$ and $from \ne to$.
- Verify square `from` contains a piece belonging to the active player.
- Verify square `to` does not contain a friendly piece.

### Stage 2: Geometric & Path Legality
- **Pawn**:
  - Single step forward: target square must be empty.
  - Double step forward: only from rank 2 (White) or rank 7 (Black); both intermediate and target squares must be empty.
  - Diagonal capture: target square must contain an enemy piece, or match `en_passant_sq`.
  - Promotion: pawn moving to rank 8 (White) or rank 1 (Black) requires promotion to Queen/Rook/Bishop/Knight.
- **Knight**: Valid hops: $|\Delta rank| \cdot |\Delta file| = 2$. Bypasses obstacles.
- **Bishop**: $|\Delta rank| = |\Delta file|$; all intermediate squares along the diagonal must be empty.
- **Rook**: $\Delta rank = 0$ or $\Delta file = 0$; all intermediate squares along the rank/file must be empty.
- **Queen**: Combines Bishop and Rook path rules.
- **King**: 1 step in any direction. Castling requires:
  - King and corresponding Rook have not moved.
  - Intermediate squares are empty.
  - King is not currently in check, and does not pass through or land on an attacked square.

### Stage 3: King Safety & Pin Check
- Clone board state: `ChessBoard temp = *board;`
- Apply proposed move to `temp`.
- Locate active player's King square.
- Call `is_square_attacked(&temp, king_sq, opponent_color)`.
- If King is attacked, the move is rejected with `ERR_KING_IN_CHECK`.

### Stage 4: Game Termination Check
After move is applied:
1. Locate opponent King.
2. Determine if opponent King is currently attacked (`in_check`).
3. Generate all pseudo-legal moves for the opponent and test if any move leaves their King safe.
4. If opponent has **0 legal moves**:
   - If `in_check == true`: **Checkmate** (Current player wins, opponent loses).
   - If `in_check == false`: **Stalemate** (Draw).
5. Check for **Insufficient Material** (K vs K, K+N vs K, K+B vs K) -> Draw.
