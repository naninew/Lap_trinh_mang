# NetChess Protocol Specification

## 1. Transport & Framing

NetChess operates over TCP port `8888`. To enable direct browser access without Node.js or proxies, the server natively supports standard **RFC 6455 WebSocket framing** implemented purely in C.

### 1.1. WebSocket Handshake (RFC 6455)
Client initiates handshake:
```http
GET / HTTP/1.1
Host: localhost:8888
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==
Sec-WebSocket-Version: 13
```

C Server response:
```http
HTTP/1.1 101 Switching Protocols\r\n
Upgrade: websocket\r\n
Connection: Upgrade\r\n
Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n
```
*Note: `Sec-WebSocket-Accept` is computed in C as `Base64(SHA1(Key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"))`.*

### 1.2. WebSocket Frame Format
Server reads and writes standard frames:
- Byte 0: `FIN (0x80) | Opcode (0x01 for Text)`
- Byte 1: `MASK flag (1 bit) | Payload Length (7 bits, or 16-bit extended length)`
- Bytes 2-5: Masking key (present for client-to-server frames, unmasked by XOR)
- Remaining: Payload data (JSON formatted)

---

## 2. Application Message Schema (JSON)

### 2.1. Authentication
* **Register Request** (`C -> S`):
  ```json
  {"action": "REGISTER", "user": "alice", "pass": "secret123"}
  ```
* **Register Response** (`S -> C`):
  ```json
  {"action": "REGISTER_RESP", "status": "OK", "msg": "Registration successful"}
  ```
  *(Or `status: "ERROR", "msg": "Username already exists"`)*

* **Login Request** (`C -> S`):
  ```json
  {"action": "LOGIN", "user": "alice", "pass": "secret123"}
  ```
* **Login Response** (`S -> C`):
  ```json
  {"action": "LOGIN_RESP", "status": "OK", "user": "alice", "score": 100, "rank": 1}
  ```

* **Logout Request** (`C -> S`):
  ```json
  {"action": "LOGOUT"}
  ```
* **Logout Response** (`S -> C`):
  ```json
  {"action": "LOGOUT_RESP", "status": "OK"}
  ```

---

### 2.2. Lobby & Matchmaking
* **Request Idle Players** (`C -> S`):
  ```json
  {"action": "IDLE_LIST_REQ"}
  ```
* **Idle Players Broadcast** (`S -> C`):
  *Server categorizes players into 3 distinct tiers, sorted by tier ascending then rank ascending:*
  * - **Tier 1**: Online & idle within ±10 rank positions (`can_challenge: 1`, `status_text: "Ready to Challenge"`)
  * - **Tier 2**: Online & idle outside ±10 rank range or in match (`can_challenge: 0`, `status_text: "Rank Diff: %d (>10)"` or `"In Game"`)
  * - **Tier 3**: Registered offline accounts (`online: 0`, `can_challenge: 0`, `status_text: "Offline"`)
  ```json
  {
    "action": "IDLE_LIST",
    "players": [
      {"user": "bob", "score": 100, "rank": 2, "tier": 1, "online": 1, "can_challenge": 1, "status_text": "Ready to Challenge"},
      {"user": "grandmaster", "score": 300, "rank": 1, "tier": 2, "online": 1, "can_challenge": 0, "status_text": "Rank Diff: 13 (>10)"},
      {"user": "offline_user", "score": 85, "rank": 16, "tier": 3, "online": 0, "can_challenge": 0, "status_text": "Offline"}
    ]
  }
  ```

* **Send Challenge** (`C -> S`):
  ```json
  {"action": "CHALLENGE_REQ", "target": "bob", "time_mode": 10}
  ```
  *(time_mode: 10 = Rapid 10 min, 60 = Classical 60 min)*

* **Challenge Error (Rank constraint violation)** (`S -> C`):
  ```json
  {
    "action": "CHALLENGE_ERR",
    "msg": "Target player rank (15) is outside +/-10 range of your rank (1)."
  }
  ```
* **Incoming Challenge Notification** (`S -> C`):
  ```json
  {"action": "CHALLENGE_NOTIFY", "from": "alice", "score": 100, "rank": 1, "time_mode": 10}
  ```
* **Challenge Reply** (`C -> S`):
  ```json
  {"action": "CHALLENGE_REPLY", "from": "alice", "accept": true}
  ```

---

### 2.3. In-Game Play
* **Game Start Notification** (`S -> C`):
  ```json
  {
    "action": "GAME_START",
    "match_id": 1,
    "color": "white",
    "opp_name": "bob",
    "opp_score": 97,
    "opp_rank": 2,
    "time_control": 600
  }
  ```

* **Query Legal Moves for Square** (`C -> S`):
  ```json
  {"action": "GET_VALID_MOVES", "sq": 12}
  ```

* **Legal Moves Response** (`S -> C`):
  ```json
  {"action": "VALID_MOVES", "from": 12, "dests": [20, 28]}
  ```

* **Move Request** (`C -> S`):
  ```json
  {"action": "MOVE_REQ", "from": 12, "to": 28, "promo": 0}
  ```
  *Square encoding: `0 = a1, 7 = h1, 56 = a8, 63 = h8`.*

* **Move Broadcast** (`S -> C`):
  ```json
  {
    "action": "MOVE_BROADCAST",
    "from": 12,
    "to": 28,
    "promo": 0,
    "turn": "black",
    "check": false,
    "w_time": 598,
    "b_time": 600,
    "w_caps": [-1],
    "b_caps": [],
    "board": [4,2,3,5,6,3,2,4,1,1,1,1,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,-1,-1,-1,-1,-1,-1,-1,-1,-4,-2,-3,-5,-6,-3,-2,-4]
  }
  ```
  *(Note: `board` delivers the authoritative 64-square array from the C engine, ensuring castling rook displacements, en-passant captures, and promotions are perfectly synchronized across clients).*

* **Session Kicked (Concurrency Handling)** (`S -> C`):
  ```json
  {
    "action": "KICKED",
    "reason": "Tài khoản vừa được đăng nhập ở một thiết bị hoặc phiên khác."
  }
  ```

* **Move Reject** (`S -> C`):
  ```json
  {"action": "MOVE_REJECT", "msg": "Illegal move: King in check"}
  ```

* **Resignation / Forfeit Request** (`C -> S`):
  ```json
  {"action": "RESIGN"}
  ```

* **Game Over Notification** (`S -> C`):
  ```json
  {
    "action": "GAME_OVER",
    "winner": "alice",
    "reason": "Resignation / Checkmate",
    "new_score": 103,
    "delta": 3
  }
  ```

---

### 2.4. File Transfer (Match Log)
* **Log File Transfer** (`S -> C`):
  ```json
  {
    "action": "LOG_FILE",
    "filename": "match_1.log",
    "content": "=== MATCH REPORT LOG ===\nMatch ID: 1\n..."
  }
  ```
