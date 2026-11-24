# NetChess System Architecture

## 1. Overview
NetChess is a client-server online chess platform designed for the **IT4062 - Network Programming** course at Hanoi University of Science and Technology (HUST). The system implements 100% of its networking, chess rule validation, ranking calculation, and match logging in the C programming language, while utilizing a lightweight, zero-dependency HTML/CSS/JavaScript interface strictly for visual presentation.

## 2. Layered Architecture

```
+-------------------------------------------------------------------------------+
|                                CLIENT LAYER                                   |
|                                                                               |
|   +-----------------------------------------------------------------------+   |
|   |                  Zero-Dependency Web UI (Lichess Style)               |   |
|   |   - Modern light wooden board (#f0d9b5 / #b58863), SVG pieces         |   |
|   |   - Captures user clicks and sends moves via standard WebSocket frame |   |
|   |   - Renders board state, active players, and match logs               |   |
|   |   - Zero game logic, zero npm dependencies, zero build steps          |   |
|   +-----------------------------------------------------------------------+   |
|                                      |                                        |
|                                      | WebSocket (RFC 6455) / Raw TCP         |
+--------------------------------------|----------------------------------------+
                                       |
+--------------------------------------|----------------------------------------+
|                                SERVER LAYER                                   |
|                                      |                                        |
|   +----------------------------------v------------------------------------+   |
|   |                     I/O Multiplexing Core                             |   |
|   |   - Single-threaded event loop driven by POSIX / Winsock select()     |   |
|   |   - Handles multiple concurrent clients non-blockingly                |   |
|   +----------------------------------+------------------------------------+   |
|                                      |                                        |
|   +----------------------------------v------------------------------------+   |
|   |                   Protocol Framing & Handshake                        |   |
|   |   - Standalone SHA-1 and Base64 implementations in pure C             |   |
|   |   - Decodes/encodes WebSocket frames and Raw TCP packets              |   |
|   +----------------------------------+------------------------------------+   |
|                                      |                                        |
|   +----------------------------------v------------------------------------+   |
|   |                     Core Logic Modules (Pure C)                       |   |
|   |   - Account Manager: Auth, accounts.txt disk sync, Leaderboard sorting|   |
|   |   - Challenge Manager: Enforces |Rank_A - Rank_B| <= 10               |   |
|   |   - Chess Rule Engine: 8x8 Board, Move legality, Check/Checkmate/Stale|   |
|   |   - Game Coordinator: Turn management, Resignation (-3 pts), Rematch  |   |
|   |   - File Transfer Engine: Match log generation & socket transmission  |   |
|   +-----------------------------------------------------------------------+   |
+-------------------------------------------------------------------------------+
```

## 3. Client-Server Communication Flow
1. **Connection**: Client establishes TCP connection to server (default port `8888`).
2. **Handshake**: Browser initiates HTTP Upgrade to WebSocket (`GET / HTTP/1.1`). C server processes header, computes `Sec-WebSocket-Accept` using internal SHA-1 and Base64 routines, and responds with `101 Switching Protocols`.
3. **Authentication**: Client sends `LOGIN` or `REGISTER` command. Server verifies against `accounts.txt` and replies with status, current score, and rank.
4. **Lobby & Idle Discovery**: Server pushes a personalized list of online players with status `IDLE` to each idle client, excluding the client themselves. For each candidate, the server pre-evaluates challenge eligibility ($|Rank_A - Rank_B| \le 10$) in C and attaches `can_challenge` (1 or 0) along with an explanatory `status_text`.
5. **Challenge Validation**: When Player A challenges Player B, server verifies Player B is still idle and enforces the $\le 10$ rank difference limit. If valid, challenge is forwarded; otherwise, server returns `CHALLENGE_ERR`.
6. **In-Game Move Exchange**: Each move click is sent to server as `MOVE_REQ`. The C chess engine validates the move against all chess rules (geometry, path blockage, check, pins). If valid, move is applied and broadcast via `MOVE_BROADCAST`.
7. **Game Termination & Log File Transfer**: When game ends (Checkmate, Resignation, Stalemate, Draw), server updates player scores (+3 win, 0 draw, -3 loss), updates `accounts.txt`, writes a complete match log to `logs/match_<id>.log`, and streams the log text to both players. Both players return to `IDLE` state.
