# **Thiết kế hệ thống cờ vua mạng socket C**

[https\://gemini.google.com/app/e92e075f2a67218e](https://gemini.google.com/app/e92e075f2a67218e)

*User prompt: tình hình là tôi cần bạn thiết kế hệ thống chơi cờ trực tuyến, sử dụng các hàm socket của c và có giao diện sử dụng js. Về cơ bản sẽ là thiết kế server-client. Cái này chạy trên mạng nội bộ cũng đc, nhưng mà sử dụng hàm của C để làm socket, js để làm giao diện là điều cần thiết. format giao thức thì đơn giản vừa vừa, có thêm phần mã hóa thông tin trước khi gửi là được. Về cơ bản cần tránh kết nối http do nó đã handle lượng lớn task. Do project này muốn làm nổi bật và đây là thực hiện để có thể show rõ các kiến thức môn lập trình mạng được ứng dụng vào như nào nên cần sử dụng C và các hàm của C được học. Bạn có thể thiết kế 2 chế độ http và mạng local ko dung http cũng được, nói chung là % trong việc kết nối là cần thiết. Lập kế hoạch chi tiết, giải trình chi tiết về thiết kế hệ thống, các module, các giao thức, nội dung giao thức, thiết kế chi tiết các file và các hàm. Về cơ thì tôi sẽ sử dụng cờ vua, tôi chỉ cần UI, check kỹ luật chơi, các theme, có phần tính giờ, lưu trữ lịch sử đánh. Nói chung là sẽ có 2 người chơi thực tế chứ ko phải là đánh với máy. Nói rõ cho tôi về thiết kế như này thì file C và js trên máy user được lưu thế nào, đóng gói thành ứng dụng như nào, có thể dùng shortcut để khởi động như ứng dụng đc ko, js thì làm kiểu app naitive để đỡ giống webapp nhá.*

Response: Để làm nổi bật tối đa kiến thức môn **Lập trình mạng (Network Programming)** mà vẫn có giao diện mượt như ứng dụng desktop thật (Native App), giải pháp tối ưu nhất là tách biệt hoàn toàn **Tầng Mạng (viết 100% bằng C Socket)** và **Tầng Giao diện (viết bằng JS trên nền Electron)**.

Thay vì để JS gọi HTTP hay mở socket bằng Node.js, ứng dụng JS sẽ chạy một **tiến trình con (Sidecar Process)** biên dịch từ code C. Toàn bộ kết nối mạng giữa máy người chơi và Server đi qua **Raw TCP Socket của C**, hoàn toàn không dính dáng đến HTTP.

## **1\. Kiến trúc tổng thể & Giải trình thiết kế**

### **Mô hình 2 chế độ kết nối (Dual-Mode Transport)**

Để đáp ứng yêu cầu tránh HTTP nhưng vẫn linh hoạt, Server viết bằng C sẽ mở 2 port lắng nghe đồng thời bằng select() / poll() / epoll():

> 1. **Chế độ 1 — Native Raw TCP (Chế độ chính, khuyến nghị dùng khi chấm đồ án):**  
   * **Luồng dữ liệu:** JS UI (Electron) Local IPC (stdin/stdout)​ C Client (Sidecar) Raw TCP \+ Custom Protocol \+ Ma˜ hoˊa​ C Server.  
   * **Ưu điểm:** 100% lưu lượng trên mạng là gói tin tự định nghĩa chạy trực tiếp trên TCP Socket của C (socket, connect, send, recv). Khi mở **Wireshark** bắt gói tin để demo cho giảng viên, bạn sẽ thấy rõ từng byte header, payload đã mã hóa và cơ chế chống dính gói (TCP framing).  
> 2. **Chế độ 2 — Web/Browser Fallback (Tự viết WebSocket Handshake bằng C):**  
   * Nếu muốn hỗ trợ thêm người chơi mở qua trình duyệt mà không dùng thư viện HTTP có sẵn, C Server mở thêm port phụ, tự đọc chuỗi HTTP Upgrade bằng recv(), tự tính SHA-1 \+ Base64 theo chuẩn RFC 6455 để nâng cấp lên WebSocket rồi bọc đúng gói tin nhị phân của Chế độ 1 bên trong.  
   * *Lưu ý:* Trong thiết kế chi tiết bên dưới, ta tập trung sâu vào **Chế độ 1** vì nó thỏa mãn tốt nhất cả 2 tiêu chí: **thuần C Socket** và **Native Desktop App**.

\+--------------------------------------------------+         \+--------------------------------------------------+  
\|             MÁY NGƯỜI CHƠI (CLIENT)              \|         \|                MÁY CHỦ (C SERVER)                \|  
\|                                                  \|         \|                                                  \|  
\|  \+--------------------------------------------+  \|         \|  \+--------------------------------------------+  \|  
\|  \|         Electron Desktop App (JS)          \|  \|         \|  \|          Network I/O Multiplexer           \|  \|  
\|  \|  \- Render bàn cờ, Theme, Sound, Animation  \|  \|         \|  \|     (epoll / poll / select \+ pthread)      \|  \|  
\|  \|  \- Không khung trình duyệt (Frameless UI)  \|  \|         \|  \+---------------------+----------------------+  \|  
\|  \+---------------------+----------------------+  \|         \|                        \|                         \|  
\|                        \| Local Pipe (JSON Lines) \|         \|  \+---------------------+----------------------+  \|  
\|  \+---------------------+----------------------+  \| Raw TCP \|  \|    Protocol Framing & Crypto Engine (C)    \|  \|  
\|  \|        C Network Client (Sidecar)          \+==+=========+==+  \- Chống dính gói (Length-Prefix Framing)  \|  \|  
\|  \|  \- socket(), connect(), send(), recv()     \|  \| (Port   \|  \|  \- Mã hóa AES-256-CTR / Diffie-Hellman     \|  \|  
\|  \|  \- Đóng gói nhị phân & Mã hóa/Giải mã      \|  \|  9000\)  \|  \+---------------------+----------------------+  \|  
\|  \+--------------------------------------------+  \|         \|                        \|                         \|  
\+--------------------------------------------------+         \|  \+---------------------+----------------------+  \|  
                                                             \|  \|     Game Logic & State Manager (C)         \|  \|  
                                                             \|  \|  \- Luật cờ vua đầy đủ (Server-Authoritative)\| \|  
                                                             \|  \|  \- Đồng hồ đếm ngược chuẩn theo Server     \|  \|  
                                                             \|  \|  \- Lưu lịch sử ván đấu (SQLite / PGN)      \|  \|  
                                                             \|  \+--------------------------------------------+  \|  
                                                             \+--------------------------------------------------+

### **Nguyên tắc Server-Authoritative (Chống gian lận & Đồng bộ)**

> * **Luật cờ vua:** JS phía Client có thể kiểm tra sơ bộ để gợi ý nước đi hợp lệ (UX mượt), nhưng **C Server mới là nơi kiểm tra luật cuối cùng** (nhập thành, bắt tốt qua đường, phong cấp, chiếu, chiếu bí, hòa cờ).  
> * **Tính giờ (Chess Clock):** Server lưu timestamp (dùng clock\_gettime(CLOCK\_MONOTONIC) trên Linux hoặc QueryPerformanceCounter trên Windows) khi bắt đầu lượt. Client chỉ hiển thị đồng hồ chạy nội suy; mỗi khi có nước đi hợp lệ, Server trừ thời gian thực tế, cộng increment (nếu có) và gửi lại thời gian chuẩn cho cả 2 máy.

## **2\. Thiết kế giao thức ứng dụng (Custom Protocol) & Mã hóa**

Vì TCP là giao thức hướng luồng byte (**byte-stream**), 2 lần send() có thể bị gộp thành 1 lần recv() (dính gói — sticky packet) hoặc 1 lần send() bị cắt làm nhiều mảnh (fragmentation). Do đó, giao thức bắt buộc phải có **Header cố định chứa độ dài gói tin**.

### **Cấu trúc gói tin (Binary Packet Format)**

Mọi số nguyên nhiều byte đều chuyển về **Network Byte Order (Big-Endian)** bằng htons() / htonl() trước khi gửi và giải mã bằng ntohs() / ntohl() khi nhận.

 0                   1                   2                   3  
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1  
\+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  
\|         Magic (0x4348)        \|    Version    \|    Opcode     \|  
\+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  
\|         Payload Length        \|         Flags / Flags         \|  
\+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  
\|                      Sequence Number (Seq)                    \|  
\+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  
\|                                                               \|  
\|            Encrypted Payload (0 .. Payload Length)            \|  
\|                                                               \|  
\+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  
\|                        CRC32 / HMAC Tag                       \|  
\+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

> * **Magic (2 bytes):** 0x4348 (ký tự 'C', 'H' — Chess). Dùng để nhận diện gói tin hợp lệ và loại bỏ kết nối rác.  
> * **Version (1 byte):** 0x01.  
> * **Opcode (1 byte):** Mã lệnh định nghĩa hành động.  
> * **Payload Length (2 bytes):** Độ dài phần dữ liệu phía sau (tối đa 65535 bytes).  
> * **Flags (2 bytes):** Bit 0 \= 1 nếu payload đã mã hóa; Bit 1 \= 1 nếu cần xác nhận.  
> * **Seq (4 bytes):** Số thứ tự gói tin, chống tấn công phát lại (replay attack) và làm **Nonce/IV** cho mã hóa luồng.  
> * **CRC32 / HMAC (4 bytes):** Kiểm tra toàn vẹn dữ liệu.

### **Bảng mã lệnh (Opcodes)**

| Opcode | Tên lệnh | Hướng | Nội dung Payload (trước khi mã hóa) |
| :---- | :---- | :---- | :---- |
| 0x01 | KEY\_EXCHANGE | C ↔ S | Public Key (Diffie-Hellman) để hai bên tự sinh Session Key chung |
| 0x02 | AUTH\_LOGIN | C → S | username\_len (1B) \| username \| password\_hash (32B) |
| 0x03 | AUTH\_RESP | S → C | status (1B) \| user\_id (4B) \| elo (2B) |
| 0x10 | ROOM\_CREATE | C → S | time\_control\_sec (2B) \| increment\_sec (1B) \| color\_pref (1B) |
| 0x11 | ROOM\_JOIN | C → S | room\_id (4B) |
| 0x12 | ROOM\_LIST | C ↔ S | Danh sách phòng đang chờ (Room ID, tên chủ phòng, thời gian) |
| 0x20 | GAME\_START | S → C | match\_id (4B) \| assigned\_color (1B) \| opponent\_name \| initial\_fen |
| 0x21 | MOVE\_REQ | C → S | from\_sq (1B: 0-63) \| to\_sq (1B: 0-63) \| promo\_piece (1B) |
| 0x22 | MOVE\_BROADCAST | S → C | from\_sq (1B) \| to\_sq (1B) \| promo (1B) \| white\_ms (4B) \| black\_ms (4B) \| game\_flags (1B) |
| 0x23 | MOVE\_REJECT | S → C | error\_code (1B) (ví dụ: sai lượt, nước đi để vua bị chiếu) |
| 0x24 | GAME\_OVER | S → C | result (1B: W/B/Draw) \| reason (1B: Checkmate/Timeout/Resign/Stalemate) |
| 0x30 | HISTORY\_REQ | C → S | user\_id (4B) \| page (1B) |
| 0x31 | HISTORY\_RESP | S → C | Danh sách ván đấu \+ chuỗi nước đi PGN/UCI để xem lại (Replay) |
| 0xFF | HEARTBEAT | C ↔ S | Rỗng (kiểm tra mất kết nối / rớt mạng mỗi 5 giây) |

### **Cơ chế mã hóa thông tin trước khi gửi**

Để cân bằng giữa tính bảo mật và độ phức tạp vừa phải khi code bằng C:

> 1. **Trao đổi khóa (Handshake):** Ngay khi connect() thành công, Client và Server gửi gói 0x01 (KEY\_EXCHANGE) (Flag mã hóa \= 0\) thực hiện **Diffie-Hellman** (hoặc gửi khóa phiên bằng RSA) để tạo ra session\_key (16 hoặc 32 bytes) duy nhất cho phiên TCP đó.  
> 2. **Mã hóa Payload:** Mọi gói tin từ 0x02 trở đi bật Flag mã hóa \= 1\. Dùng **AES-128-CTR** hoặc **ChaCha20** (có thể tích hợp thư viện C gọn nhẹ 1 file như tiny-AES-c hoặc OpenSSL libcrypto) với IV/Nonce kết hợp từ Seq của Header.  
   * *Lý do chọn CTR / ChaCha20:* Không cần padding độ dài khối (payload bao nhiêu byte mã hóa ra đúng bấy nhiêu byte), tốc độ cực nhanh, dễ giải thích khi vấn đáp.

## **3\. Phân rã Module & Thiết kế chi tiết File / Hàm**

### **Cấu trúc thư mục dự án**

chess-network-app/  
├── shared/                     \# Code C dùng chung giữa Server và Client  
│   ├── protocol.h              \# Định nghĩa struct Header, Opcode, Error codes  
│   ├── framing.c / .h          \# send\_all(), recv\_all(), đóng/mở gói tin TCP  
│   └── crypto.c / .h           \# Diffie-Hellman, AES-CTR / ChaCha20, CRC32  
│  
├── server/                     \# Chương trình C Server (chạy trên Linux/Windows)  
│   ├── main.c                  \# Khởi tạo socket, bind, listen, vòng lặp chính  
│   ├── net\_loop.c / .h         \# Đa luồng / I/O Multiplexing (poll/epoll) quản lý client  
│   ├── session.c / .h          \# Quản lý trạng thái kết nối, buffer nhận dở dang, timeout  
│   ├── chess\_engine.c / .h     \# Luật cờ vua đầy đủ (bàn cờ 8x8, sinh nước đi, chiếu bí)  
│   ├── room\_manager.c / .h     \# Ghép phòng, quản lý đồng hồ, broadcast nước đi  
│   ├── storage.c / .h          \# Lưu tài khoản & lịch sử ván đấu (SQLite3 C API hoặc file PGN)  
│   └── Makefile  
│  
├── client-core/                \# C Client Sidecar (được đóng gói bên trong app JS)  
│   ├── main.c                  \# Vòng lặp đa luồng: 1 luồng đọc TCP, 1 luồng đọc stdin từ JS  
│   ├── net\_client.c / .h       \# socket(), connect(), tự động gửi Heartbeat  
│   ├── ipc\_bridge.c / .h       \# Chuyển đổi giữa Binary Packet (C) \<-\> JSON Line (JS)  
│   └── Makefile  
│  
└── client-ui/                  \# Ứng dụng Desktop JS (Electron)  
    ├── package.json            \# Cấu hình Electron & electron-builder  
    ├── main.js                 \# Electron Main Process: tạo cửa sổ Native, spawn C Client  
    ├── preload.js              \# Cầu nối bảo mật IPC giữa Main Process và Renderer  
    └── renderer/  
        ├── index.html          \# Giao diện frameless (có thanh tiêu đề tự thiết kế)  
        ├── css/  
        │   ├── base.css        \# Layout chính  
        │   └── themes/         \# classic.css, dark-wood.css, neon.css, minimalist.css  
        └── js/  
            ├── app.js          \# Điều phối màn hình (Login, Lobby, Match, History Replay)  
            ├── board\_ui.js     \# Render bàn cờ, kéo-thả quân cờ, highlight nước đi/chiếu  
            ├── clock\_ui.js     \# Hiển thị đồng hồ đếm ngược, đồng bộ lại mỗi khi Server gửi  
            ├── history\_ui.js   \# Xem lại lịch sử ván đấu (nút Next/Prev từng nước đi)  
            └── theme\_mgr.js    \# Chuyển đổi theme động, lưu cấu hình vào local

### **Chi tiết các hàm cốt lõi (Function-Level Design)**

#### **1\. Module shared/framing.c & shared/crypto.c (Trọng tâm Lập trình mạng)**

> * int send\_all(int sockfd, const uint8\_t \*buf, size\_t len): Vòng lặp gọi send() cho đến khi gửi đủ len byte (xử lý hiện tượng partial send của TCP).  
> * int recv\_all(int sockfd, uint8\_t \*buf, size\_t len): Vòng lặp gọi recv() cho đến khi đọc đủ len byte, trả về 0 nếu đối phương đóng kết nối, \-1 nếu lỗi.  
> * int packet\_send(int sockfd, Packet \*pkt, CryptoCtx \*ctx): Chuyển header sang Big-Endian, mã hóa pkt-\>payload, tính CRC32, gọi send\_all().  
> * int packet\_recv(int sockfd, Packet \*pkt, CryptoCtx \*ctx): Đọc đúng 12 byte Header trước \-\> kiểm tra Magic \-\> lấy payload\_len \-\> đọc tiếp đúng payload\_len \+ 4 byte \-\> kiểm tra CRC32 \-\> giải mã payload.

#### **2\. Module server/net\_loop.c & server/session.c**

> * int server\_init(uint16\_t port): Gọi socket(AF\_INET, SOCK\_STREAM, 0), đặt setsockopt(SO\_REUSEADDR), bind(), listen().  
> * void server\_run\_loop(int listen\_fd): Dùng poll() (tương thích dễ dàng cả Linux & Windows/WSAPoll) hoặc epoll() (Linux) để theo dõi đồng thời hàng trăm client socket mà không cần tạo hàng trăm thread. Đặt timeout 100ms cho poll() để định kỳ kiểm tra hết giờ (timeout) của các bàn cờ đang đấu và Heartbeat của client.  
> * void handle\_client\_packet(ClientSession \*sess, Packet \*pkt): Bộ định tuyến (dispatcher) rẽ nhánh theo pkt-\>opcode.

#### **3\. Module server/chess\_engine.c (Kiểm tra kỹ luật cờ vua)**

Dùng cấu trúc ChessBoard lưu mảng int8\_t squares\[64\], turn (Trắng/Đen), castling\_rights (4 bit: KQkq), en\_passant\_sq (0..63 hoặc \-1), halfmove\_clock (luật 50 nước), fullmove\_number, và mảng hash lịch sử thế cờ (Zobrist Hashing để bắt luật lặp lại 3 lần).

> * void board\_init(ChessBoard \*b): Khởi tạo thế cờ chuẩn ban đầu.  
> * bool is\_square\_attacked(const ChessBoard \*b, int sq, int by\_color): Kiểm tra ô sq có đang bị quân màu by\_color khống chế không (dùng cho kiểm tra chiếu và luật nhập thành không đi qua ô bị chiếu).  
> * bool validate\_and\_apply\_move(ChessBoard \*b, Move m):  
  1. Kiểm tra đúng quân của người đến lượt.  
  2. Kiểm tra quy tắc di chuyển của từng loại quân (Tốt đi 1/2 ô, ăn chéo, bắt tốt qua đường, phong cấp; Mã, Tượng, Xe, Hậu, Vua, Nhập thành ngắn/dài).  
  3. **Đi thử trên bản sao bàn cờ**, gọi is\_square\_attacked() với vị trí Vua của mình. Nếu Vua vẫn bị chiếu \-\> **từ chối nước đi**.  
  4. Cập nhật bàn cờ thật, cập nhật quyền nhập thành, en\_passant\_sq, halfmove\_clock.  
> * GameState check\_game\_status(const ChessBoard \*b): Sinh toàn bộ nước đi hợp lệ của đối phương. Nếu số nước hợp lệ \= 0: đang bị chiếu → CHECKMATE, không bị chiếu → STALEMATE. Đồng thời kiểm tra hòa do thiếu quân (Vua vs Vua, Vua+Mã vs Vua, Vua+Tượng vs Vua), luật 50 nước, và lặp lại thế cờ 3 lần.

#### **4\. Module server/room\_manager.c & server/storage.c**

> * void room\_tick\_clocks(uint64\_t now\_ms): Chạy mỗi 100ms trong vòng lặp mạng. Nếu phòng đang PLAYING và now\_ms \- last\_move\_ts \>= remaining\_ms\[turn\], lập tức kết thúc ván đấu do hết giờ, gửi GAME\_OVER và gọi storage\_save\_match().  
> * int storage\_save\_match(const MatchRecord \*rec): Lưu ID 2 người chơi, kết quả, lý do thắng, thời gian, và chuỗi các nước đi vào bảng matches của **SQLite3** (sqlite3\_exec / prepared statement).

#### **5\. Module client-core/main.c & client-core/ipc\_bridge.c**

> * void \*network\_recv\_thread(void \*arg): Luồng nền chuyên gọi packet\_recv() từ C Server. Nhận được gói tin nào thì giải mã, chuyển thành 1 dòng JSON (ví dụ: {"type":"MOVE\_BROADCAST","from":12,"to":28,"w\_ms":295000,"b\_ms":300000}\\n) rồi fprintf(stdout, ...) \+ fflush(stdout) để Electron nhận.  
> * void stdin\_command\_loop(): Luồng chính đọc từng dòng JSON từ stdin do JS gửi xuống (ví dụ: {"type":"MOVE\_REQ","from":52,"to":36}), đóng gói thành struct nhị phân Packet, mã hóa và gọi packet\_send() lên Server.

#### **6\. Module client-ui/main.js (Electron)**

> * createNativeWindow(): Khởi tạo BrowserWindow với frame: false (ẩn thanh tiêu đề trình duyệt), autoHideMenuBar: true, đặt icon ứng dụng riêng \-\> cảm giác 100% là phần mềm desktop native.  
> * spawnClientCore(): Dùng child\_process.spawn() gọi file thực thi C (chess\_client.exe trên Windows hoặc chess\_client trên Linux), lắng nghe sự kiện stdout.on('data') và chuyển thẳng sang giao diện qua mainWindow.webContents.send('net-event', data).

## **4\. Lưu trữ file trên máy User, Đóng gói & Khởi động bằng Shortcut**

Đây là phần giải quyết triệt để việc kết hợp C và JS thành một phần mềm desktop hoàn chỉnh:

### **1\. Biên dịch và Đóng gói (Packaging)**

Bạn dùng **electron-builder** để đóng gói toàn bộ dự án JS cùng với file thực thi C đã biên dịch sẵn thành 1 bộ cài đặt duy nhất (.exe / NSIS Installer trên Windows, .AppImage / .deb trên Linux).

Trong client-ui/package.json, bạn khai báo trường extraResources để nhúng file C đã biên dịch vào bên trong ứng dụng:

"build": {  
  "appId": "com.chessnet.app",  
  "productName": "NetChess",  
  "extraResources": \[  
    {  
      "from": "../client-core/bin/chess\_client.exe",  
      "to": "bin/chess\_client.exe"  
    }  
  \],  
  "win": {  
    "target": "nsis",  
    "icon": "assets/icon.ico"  
  }  
}

Khi chạy lệnh npm run build, electron-builder sẽ đóng gói toàn bộ HTML/CSS/JS vào một file nén bảo mật duy nhất tên là app.asar và đặt file C chess\_client.exe ngay bên cạnh.

### **2\. File C và JS nằm ở đâu trên máy User sau khi cài đặt?**

Người dùng **không cần** cài Node.js, không cần cài trình biên dịch C (GCC), cũng không cần mở trình duyệt. Sau khi chạy bộ cài NetChess-Setup.exe, cấu trúc trên máy người dùng sẽ là:

> * **Thư mục cài đặt ứng dụng** (VD: C:\\Users\\\<User\>\\AppData\\Local\\Programs\\NetChess\\):  
>   NetChess/  
>   ├── NetChess.exe              \# File chạy chính (Electron)  
>   ├── resources/  
>   │   ├── app.asar              \# Toàn bộ code JS/HTML/CSS/Themes đã được đóng gói gọn  
>   │   └── bin/  
>   │       └── chess\_client.exe  \# File thực thi C Socket (chạy ngầm khi app mở)  
>   └── ... (các file thư viện động .dll của Chromium/Electron)

> * **Thư mục dữ liệu cá nhân của User** (VD: C:\\Users\\\<User\>\\AppData\\Roaming\\NetChess\\):  
  * Lưu file cấu hình settings.json (Theme đang chọn, âm lượng, địa chỉ IP Server mạng LAN gần nhất).  
  * Lưu cache lịch sử ván đấu nếu người dùng muốn xem lại offline.

### **3\. Khởi động bằng Shortcut & Giao diện Native**

> * **Shortcut:** Trình cài đặt NSIS do electron-builder tạo ra sẽ **tự động tạo Shortcut trên Desktop và Start Menu** trỏ thẳng đến NetChess.exe kèm icon cờ vua riêng.  
> * **Trải nghiệm khi bấm Shortcut:**  
  1. Người dùng double-click vào icon NetChess trên Desktop.  
  2. Cửa sổ ứng dụng bật lên ngay lập tức (**không có thanh địa chỉ URL, không có tab, không có menu của trình duyệt**; bạn tự vẽ nút thu nhỏ/phóng to/đóng góc trên phải bằng CSS và gắn sự kiện Electron).  
  3. Ngầm bên dưới, NetChess.exe tự tìm đường dẫn path.join(process.resourcesPath, 'bin', 'chess\_client.exe') và khởi chạy tiến trình C ở chế độ ẩn cửa sổ dòng lệnh (windowsHide: true).  
  4. Khi người dùng bấm nút X đóng app, Electron gửi tín hiệu đóng socket và tắt tiến trình chess\_client.exe gọn gàng.

## **5\. Kế hoạch thực hiện chi tiết (5 Giai đoạn)**

| Giai đoạn | Mục tiêu chính | Đầu ra (Deliverables) cần kiểm chứng |
| :---- | :---- | :---- |
| **Giai đoạn 1: Nền tảng Mạng C (Tuần 1\)** | Xây dựng shared/ và khung server/ \+ client-core/. | 2 tiến trình C kết nối được qua Raw TCP, gửi nhận gói tin theo đúng Header \+ Length-Prefix, mã hóa AES-CTR sau khi trao đổi khóa, test chịu tải gửi 1000 gói liên tục không bị dính gói/mất gói (kiểm tra bằng Wireshark). |
| **Giai đoạn 2: Lõi Cờ vua & Quản lý phòng trên Server (Tuần 2\)** | Hoàn thiện chess\_engine.c, room\_manager.c, storage.c. | Chạy unit test cho toàn bộ luật đặc biệt (nhập thành, bắt tốt qua đường, phong cấp, chiếu bí, hòa cờ), kiểm tra đồng hồ đếm ngược chuẩn trên Server, lưu & truy vấn được ván đấu từ SQLite. |
| **Giai đoạn 3: Cầu nối IPC & Khung Electron (Tuần 3\)** | Viết ipc\_bridge.c, main.js, preload.js. | Mở cửa sổ Electron Native (frameless), tự động bật chess\_client.exe ngầm, bấm nút trên giao diện gửi được lệnh LOGIN / CREATE\_ROOM lên C Server và nhận phản hồi hiển thị lên UI. |
| **Giai đoạn 4: Giao diện Cờ vua, Theme & Lịch sử (Tuần 4\)** | Hoàn thiện board\_ui.js, clock\_ui.js, history\_ui.js, bộ CSS Themes. | Chơi trọn vẹn ván cờ 2 người trên 2 máy trong mạng LAN, kéo-thả mượt, đổi được 3–4 theme, có âm thanh khi đi quân/ăn quân/chiếu, xem lại được các ván đã đánh. |
| **Giai đoạn 5: Đóng gói & Kiểm thử thực tế (Tuần 5\)** | Cấu hình electron-builder, test rớt mạng (Heartbeat/Disconnect). | Xuất ra file cài đặt NetChess-Setup.exe, cài thử lên một máy trắng không có môi trường dev, chạy bằng Desktop Shortcut và kết nối vào C Server trong mạng nội bộ. |

Nếu bạn muốn bắt tay vào triển khai ngay, bước đầu tiên khó và quan trọng nhất là thiết kế chuẩn bộ protocol.h \+ framing.c (xử lý TCP stream và mã hóa) để tránh lỗi vặt về sau. Bạn muốn tôi viết mẫu code hoàn chỉnh cho tầng **C Framing \+ Protocol** trước, hay viết khung **Electron gọi C Sidecar (IPC)** trước để chạy thử kết nối?

---

*User prompt: Viết cụ thể kế hoạch tiếp đi, viết như tài liệu đặc tả cho tôi*

# **Response: TÀI LIỆU ĐẶC TẢ THIẾT KẾ HỆ THỐNG (SDS)**

**Dự án:** Hệ thống Chơi Cờ Vua Trực Tuyến (NetChess)

**Kiến trúc:** Client–Server thuần C Socket \+ Giao diện Desktop Native (Electron/JS)

**Phiên bản tài liệu:** 1.0

## **1\. Tổng quan & Yêu cầu hệ thống**

### **1.1. Mục tiêu**

Xây dựng hệ thống chơi cờ vua trực tuyến giữa 2 người chơi thực tế (PvP) hoạt động trên mạng nội bộ (LAN) hoặc Internet, đáp ứng đồng thời hai yêu cầu cốt lõi:

> * **Về kỹ thuật lập trình mạng:** Tầng truyền tải viết 100% bằng ngôn ngữ C sử dụng BSD/Winsock Socket API (socket, bind, listen, accept, connect, send, recv, poll/epoll), tự xây dựng giao thức nhị phân, cơ chế chống dính gói (TCP Framing), mã hóa đầu cuối và quản lý đa kết nối mà không phụ thuộc vào HTTP framework.  
> * **Về trải nghiệm người dùng:** Ứng dụng chạy độc lập như phần mềm desktop native (không khung trình duyệt), hỗ trợ đóng gói thành bộ cài đặt có Shortcut khởi động, cung cấp đầy đủ luật cờ vua chuẩn, đồng hồ tính giờ, đổi giao diện (Theme) và lưu/xem lại lịch sử ván đấu.

### **1.2. Yêu cầu chức năng (Functional Requirements)**

| Mã | Nhóm chức năng | Mô tả chi tiết |
| :---- | :---- | :---- |
| **FR-01** | Kết nối & Xác thực | Người dùng nhập IP/Port Server (mặc định LAN), thực hiện bắt tay mã hóa, đăng ký hoặc đăng nhập bằng Username/Password. |
| **FR-02** | Quản lý phòng đấu | Tạo phòng mới (chọn thời gian: 3+2, 5+0, 10+0, 15+10; chọn màu quân hoặc ngẫu nhiên), xem danh sách phòng chờ, tham gia phòng bằng Room ID. |
| **FR-03** | Thi đấu cờ vua PvP | Di chuyển quân cờ bằng kéo-thả hoặc click; hiển thị gợi ý nước đi hợp lệ; hiển thị nước đi gần nhất và cảnh báo khi Vua bị chiếu. |
| **FR-04** | Kiểm tra luật chuẩn | Server kiểm duyệt 100% luật: di chuyển từng quân, nhập thành ngắn/dài, bắt tốt qua đường (En Passant), phong cấp (Promotion), chiếu, chiếu bí (Checkmate), hòa cờ (Stalemate, thiếu quân, 50 nước, lặp lại thế cờ 3 lần). |
| **FR-05** | Đồng hồ tính giờ | Đếm ngược độc lập cho hai bên theo chuẩn thời gian của Server; cộng thêm thời gian (increment) sau mỗi nước đi; tự động xử thua khi hết giờ. |
| **FR-06** | Hành động trong ván | Đầu hàng (Resign), cầu hòa (Offer Draw) và chấp nhận/từ chối hòa. |
| **FR-07** | Lịch sử & Xem lại | Tự động lưu mọi ván đấu đã kết thúc trên Server; cho phép người dùng tra cứu danh sách ván cũ và mở trình Replay (tiến/lùi từng nước đi). |
| **FR-08** | Cá nhân hóa (Themes) | Chuyển đổi linh hoạt giữa tối thiểu 4 bộ Theme (Classic Wood, Dark Slate, Tournament Green, Neon Cyber) và lưu cấu hình cục bộ. |

### **1.3. Yêu cầu phi chức năng (Non-Functional Requirements)**

> * **Độ trễ (Latency):** Thời gian phản hồi một nước đi trong mạng LAN dưới 50ms.  
> * **Khả năng chịu tải:** Server xử lý đồng thời tối thiểu 100 kết nối (50 bàn cờ song song) trên một tiến trình đơn luồng I/O Multiplexing (poll/epoll) với mức chiếm dụng RAM dưới 64MB.  
> * **An toàn truyền tải:** Toàn bộ dữ liệu nghiệp vụ (mật khẩu, nước đi, lịch sử) phải được mã hóa trước khi gọi send(); phát hiện được gói tin lỗi hoặc bị chỉnh sửa trên đường truyền.  
> * **Khả năng phục hồi:** Tự động phát hiện client rớt mạng qua cơ chế Heartbeat (tối đa 15 giây) và xử lý đóng kết nối an toàn, tránh rò rỉ file descriptor (FD leak).

## **2\. Kiến trúc hệ thống & Chế độ kết nối**

### **2.1. Kiến trúc phân tầng (Layered Architecture)**

Hệ thống tách bạch hoàn toàn **Tầng hiển thị (UI)** và **Tầng mạng (Network Core)** trên máy Client thông qua mô hình **Sidecar Process**:

\================================================================================  
                          MÁY NGƯỜI CHƠI (CLIENT)  
\================================================================================  
\+------------------------------------------------------------------------------+  
\| \[Tầng UI \- Electron Renderer (HTML/CSS/JS)\]                                  \|  
\|   \- Board UI, Chess Clock Display, Theme Engine, History Replay              \|  
\+--------------------------------------+---------------------------------------+  
                                       \| Electron IPC (contextBridge)  
\+--------------------------------------+---------------------------------------+  
\| \[Tầng Điều phối \- Electron Main Process (Node.js)\]                           \|  
\|   \- Quản lý cửa sổ Frameless Native, đọc/ghi settings.json                   \|  
\|   \- Spawn & quản lý vòng đời tiến trình con (chess\_client.exe / bin)         \|  
\+--------------------------------------+---------------------------------------+  
                                       \| Local OS Pipe (stdin / stdout)  
                                       \| Format: JSON Lines (\\n-delimited)  
\+--------------------------------------+---------------------------------------+  
\| \[Tầng Mạng \- C Client Core (chess\_client)\]                                   \|  
\|   \- Luồng 1 (Main): Đọc stdin \-\> Đóng gói Binary \-\> Mã hóa \-\> send\_all()     \|  
\|   \- Luồng 2 (Recv): recv\_all() \-\> Giải mã \-\> Kiểm tra CRC \-\> Ghi stdout      \|  
\|   \- Luồng 3 (Timer): Gửi gói HEARTBEAT định kỳ mỗi 5 giây                    \|  
\+--------------------------------------+---------------------------------------+  
                                       \|\|  
                    Raw TCP Socket (IPv4 / Port 9000\)  
               \[Chế độ 1: Native Binary Protocol \+ AES-CTR\]  
                                       \|\|  
\================================================================================  
                           MÁY CHỦ (C SERVER)  
\================================================================================  
\+--------------------------------------+---------------------------------------+  
\| \[Tầng Transport & Multiplexing \- net\_loop.c\]                                 \|  
\|   \- Port 9000: Native Raw TCP Listener (Chế độ chính)                        \|  
\|   \- Port 9001: Raw WebSocket Listener (Chế độ 2 \- Tùy chọn mở rộng)          \|  
\|   \- Vòng lặp poll() / epoll() Non-blocking (timeout 100ms cho Clock Tick)    \|  
\+--------------------------------------+---------------------------------------+  
\| \[Tầng Giao thức & Bảo mật \- framing.c, crypto.c\]                             \|  
\|   \- Per-client Ring/Linear Buffer chống dính gói & phân mảnh TCP             \|  
\|   \- Diffie-Hellman Handshake, AES-128-CTR Encrypt/Decrypt, CRC32             \|  
\+--------------------------------------+---------------------------------------+  
\| \[Tầng Nghiệp vụ \- room\_manager.c, chess\_engine.c, storage.c\]                 \|  
\|   \- Quản lý phòng, đồng hồ chuẩn hệ thống, kiểm tra luật cờ vua              \|  
\|   \- Lưu trữ bền vững bằng SQLite3 (users, matches, match\_moves)              \|  
\+------------------------------------------------------------------------------+

### **2.2. Đặc tả 2 chế độ kết nối trên Server**

Để tối ưu hóa việc trình diễn kiến thức Lập trình mạng, Server quản lý hai socket lắng nghe trong cùng mảng pollfd:

> 1. **Chế độ 1 — Native Raw TCP (Port 9000, Mặc định):**  
   * Kết nối trực tiếp từ chess\_client (viết bằng C).  
   * Giao thức nhị phân thuần, không có bất kỳ header HTTP nào.  
   * Dễ dàng kiểm chứng bằng Wireshark: lọc theo tcp.port \== 9000 sẽ thấy rõ 2 byte Magic 43 48 và payload đã bị mã hóa.  
> 2. **Chế độ 2 — Web/Browser Fallback (Port 9001, Tùy chọn):**  
   * Khi có kết nối mới vào Port 9001, Server đánh dấu trạng thái WS\_HANDSHAKE.  
   * Server dùng recv() đọc raw HTTP Upgrade request, tách chuỗi Sec-WebSocket-Key, ghép chuỗi GUID chuẩn RFC 6455 (258EAFA5-E914-47DA-95CA-C5AB0DC85B11), băm SHA-1, mã hóa Base64 và tự gửi phản hồi HTTP/1.1 101 Switching Protocols.  
   * Sau khi nâng cấp, mọi gói tin nhị phân của Chế độ 1 được bọc trong WebSocket Binary Frame (Opcode 0x02) và đẩy vào chung bộ xử lý handle\_client\_packet().

## **3\. Đặc tả Giao thức mạng & Bảo mật**

### **3.1. Định dạng gói tin nhị phân (Wire Format)**

Mọi trường số nguyên nhiều hơn 1 byte đều được truyền theo chuẩn **Network Byte Order (Big-Endian)** sử dụng htons()/htonl() khi đóng gói và ntohs()/ntohl() khi giải gói.

\#define PROTO\_MAGIC   0x4348  // 'C', 'H'  
\#define PROTO\_VERSION 0x01  
\#define MAX\_PAYLOAD   4096    // Giới hạn tối đa 4KB/gói để chống tấn công tràn bộ nhớ

\#pragma pack(push, 1\)  
typedef struct {  
    uint16\_t magic;        // Offset 0:  0x4348  
    uint8\_t  version;      // Offset 2:  0x01  
    uint8\_t  opcode;       // Offset 3:  Mã lệnh (xem bảng 3.3)  
    uint16\_t payload\_len;  // Offset 4:  Độ dài payload thực tế (0 .. 4096\)  
    uint16\_t flags;        // Offset 6:  Bit 0 \= ENCRYPTED (0x0001)  
    uint32\_t seq;          // Offset 8:  Số thứ tự tăng dần (cũng dùng làm IV/Nonce)  
} PacketHeader;            // Tổng kích thước cố định: 12 bytes  
\#pragma pack(pop)

Cấu trúc toàn bộ khung truyền (Frame) trên TCP Stream:

\[ PacketHeader (12 bytes) \] \+ \[ Payload (payload\_len bytes) \] \+ \[ CRC32 (4 bytes) \]

> * **CRC32 (4 bytes cuối):** Tính trên toàn bộ PacketHeader (12 bytes) nối với Payload (đã mã hóa nếu cờ ENCRYPTED \= 1). Giúp phát hiện ngay lỗi đường truyền hoặc gói tin bị giả mạo trước khi đưa vào giải mã.

### **3.2. Cơ chế chống dính gói & phân mảnh (Non-blocking TCP Framing)**

Trên Server, mỗi ClientSession sở hữu một bộ đệm nhận riêng (rx\_buf\[8192\], rx\_len) và bộ đệm gửi (tx\_buf\[8192\], tx\_len). Quy trình đọc gói tin mỗi khi poll() báo POLLIN:

> 1. Gọi recv(fd, rx\_buf \+ rx\_len, sizeof(rx\_buf) \- rx\_len, 0\) (chế độ O\_NONBLOCK).  
> 2. Cập nhật rx\_len \+= n.  
> 3. Chạy vòng lặp tách gói (có thể có nhiều gói dính liền trong rx\_buf):  
   * Nếu rx\_len nhỏ hơn 12 byte: dừng vòng lặp, chờ lần recv() tiếp theo.  
   * Đọc PacketHeader, kiểm tra ntohs(hdr.magic) \== 0x4348 và ntohs(hdr.payload\_len) nhỏ hơn hoặc bằng MAX\_PAYLOAD. Nếu sai: ngắt kết nối ngay (protocol violation).  
   * Tính total\_frame\_len \= 12 \+ payload\_len \+ 4\.  
   * Nếu rx\_len nhỏ hơn total\_frame\_len: chưa đủ 1 gói hoàn chỉnh, dừng vòng lặp chờ thêm dữ liệu.  
   * Khi đủ: kiểm tra CRC32, giải mã payload (nếu cờ ENCRYPTED bật), chuyển vào handle\_client\_packet().  
   * Dịch chuyển phần byte dư còn lại về đầu mảng bằng memmove(rx\_buf, rx\_buf \+ total\_frame\_len, rx\_len \- total\_frame\_len) và trừ rx\_len \-= total\_frame\_len.

### **3.3. Quy trình bắt tay & Mã hóa (Security Handshake)**

> 1. **Khởi tạo phiên (Diffie-Hellman):**  
   * Hai bên dùng chung tham số nguyên tố công khai P (64-bit hoặc 128-bit/256-bit tùy mức độ, hoặc dùng đường cong X25519 gọn nhẹ \~1 file C monocypher / tweetnacl hoặc tiny-AES-c \+ DH).  
   * Client sinh khóa bí mật a, tính A \= g^a mod P, gửi gói 0x01 (KEY\_EXCHANGE) (Flag ENCRYPTED \= 0).  
   * Server sinh khóa bí mật b, tính B \= g^b mod P, trả gói 0x01 (KEY\_EXCHANGE) chứa B và session\_salt (8 bytes ngẫu nhiên).  
   * Cả hai bên tính bí mật chung S \= B^a mod P \= A^b mod P, sau đó băm SHA-256(S \|\| session\_salt) lấy 16 bytes đầu làm aes\_key (128-bit) và 8 bytes tiếp theo làm iv\_prefix.  
> 2. **Mã hóa gói tin (AES-128-CTR):**  
   * Từ gói tin thứ 2 trở đi, cờ ENCRYPTED \= 1\.  
   * Vector khởi tạo 16 bytes (Counter Block) cho từng gói tin được tạo từ: \[iv\_prefix (8B) \| direction\_bit (4B) \| seq (4B)\].  
   * Nhờ seq tăng dần không trùng lặp và direction\_bit khác nhau giữa chiều gửi/nhận, hai gói tin có cùng nội dung sẽ cho ra bản mã hoàn toàn khác nhau trên Wireshark.

### **3.4. Bảng đặc tả chi tiết Payload theo từng Opcode**

| Opcode | Tên lệnh | Chiều | Cấu trúc Payload (Big-Endian) |
| :---- | :---- | :---- | :---- |
| 0x01 | KEY\_EXCHANGE | C ↔ S | pub\_key (32B) \+ salt (8B, chỉ chiều S → C) |
| 0x02 | AUTH\_REQ | C → S | mode (1B: 0=Login, 1=Register) \| user\_len (1B) \| username \| pass\_sha256 (32B) |
| 0x03 | AUTH\_RESP | S → C | status (1B: 0=OK, 1=WrongPass, 2=Exists) \| user\_id (4B) \| elo (2B) |
| 0x10 | ROOM\_CREATE | C → S | base\_time\_sec (2B) \| inc\_sec (1B) \| color\_pref (1B: 0=Rand, 1=W, 2=B) |
| 0x11 | ROOM\_JOIN | C → S | room\_id (4B) |
| 0x12 | ROOM\_LIST\_REQ | C → S | *(Rỗng — 0 bytes)* |
| 0x13 | ROOM\_LIST\_RESP | S → C | count (1B) \| lặp count lần: \[room\_id (4B) \| host\_name (16B) \| base\_sec (2B) \| inc (1B)\] |
| 0x14 | ROOM\_LEAVE | C → S | room\_id (4B) |
| 0x20 | GAME\_START | S → C | match\_id (4B) \| your\_color (1B: 0=W, 1=B) \| base\_ms (4B) \| inc\_ms (2B) \| opp\_name (16B) \| opp\_elo (2B) |
| 0x21 | MOVE\_REQ | C → S | from\_sq (1B: 0..63) \| to\_sq (1B: 0..63) \| promo (1B: 0=None, 1=Q, 2=R, 3=B, 4=N) |
| 0x22 | MOVE\_BROADCAST | S → C | from\_sq (1B) \| to\_sq (1B) \| promo (1B) \| white\_ms (4B) \| black\_ms (4B) \| flags (1B: bit0=Check, bit1=Castle, bit2=EnPassant) |
| 0x23 | MOVE\_REJECT | S → C | err\_code (1B: 1=NotTurn, 2=IllegalMove, 3=KingInCheck, 4=GameEnded) |
| 0x24 | GAME\_ACTION | C ↔ S | action (1B: 1=Resign, 2=DrawOffer, 3=DrawAccept, 4=DrawDecline) |
| 0x25 | GAME\_OVER | S → C | result (1B: 0=Draw, 1=WhiteWin, 2=BlackWin) \| reason (1B: 1=Mate, 2=Timeout, 3=Resign, 4=Stalemate, 5=Insufficient, 6=50Move, 7=3Fold, 8=Agree, 9=Disconnect) \| new\_elo (2B) |
| 0x30 | HISTORY\_LIST\_REQ | C → S | page (1B) \| page\_size (1B) |
| 0x31 | HISTORY\_LIST\_RESP | S → C | count (1B) \| lặp count lần: \[match\_id (4B) \| timestamp (8B) \| white\_name (16B) \| black\_name (16B) \| result (1B) \| reason (1B)\] |
| 0x32 | HISTORY\_DETAIL\_REQ | C → S | match\_id (4B) |
| 0x33 | HISTORY\_DETAIL\_RESP | S → C | match\_id (4B) \| move\_count (2B) \| mảng move\_count nước đi (mỗi nước 3 bytes: from, to, promo) |
| 0xFF | HEARTBEAT | C ↔ S | client\_ts (4B) (Server trả lại y nguyên để Client có thể đo Ping/RTT) |

### **3.5. Biểu đồ tuần tự (Sequence Flow) cho luồng thi đấu**

Client A (Trắng)               C Server                     Client B (Đen)  
     \|                            \|                               \|  
     \|--- 0x01 KEY\_EXCHANGE \-----\>\|                               \|  
     \|\<-- 0x01 KEY\_EXCHANGE \------\|                               \|  
     \|=== \[Bật mã hóa AES-CTR\] \===\|                               \|  
     \|--- 0x02 AUTH\_REQ \---------\>\|                               \|  
     \|\<-- 0x03 AUTH\_RESP (OK) \----\|                               \|  
     \|--- 0x10 ROOM\_CREATE \------\>\| (Tạo Room \#101)               \|  
     \|                            \|\<-- 0x11 ROOM\_JOIN (\#101) \-----\|  
     \|                            \|                               \|  
     \|\<-- 0x20 GAME\_START (W) \----\|--- 0x20 GAME\_START (B) \------\>\|  
     \|                            \| \[Bắt đầu tính giờ cho Trắng\]  \|  
     \|                            \|                               \|  
     \|--- 0x21 MOVE\_REQ (e2-\>e4)-\>\|                               \|  
     \|                            \| \[validate\_and\_apply\_move()\]   \|  
     \|                            \| \[Trừ giờ Trắng \+ cộng inc\]    \|  
     \|\<-- 0x22 MOVE\_BROADCAST \----\|--- 0x22 MOVE\_BROADCAST \------\>\|  
     \|                            \| \[Chuyển đồng hồ sang Đen\]     \|  
     \|                            \|                               \|  
     \|                            \| ... (Diễn biến ván đấu) ...   \|  
     \|                            \|                               \|  
     \|                            \| \[Đen hết giờ hoặc bị chiếu bí\]\|  
     \|                            \| \[storage\_save\_match() SQLite\] \|  
     \|\<-- 0x25 GAME\_OVER \---------\|--- 0x25 GAME\_OVER \-----------\>\|

## **4\. Đặc tả Cầu nối IPC (Electron JS ↔ C Client Sidecar)**

Vì Electron chạy ở tiến trình tách biệt với chess\_client, hai tiến trình giao tiếp cục bộ qua đường ống chuẩn của hệ điều hành (stdin và stdout) với định dạng **JSON Lines** (mỗi thông điệp là đúng 1 chuỗi JSON kết thúc bằng ký tự xuống dòng \\n, không chứa xuống dòng bên trong).

### **Quy ước luồng IPC**

> * **Chiều UI → C Sidecar (stdin của C):**  
>   {"cmd":"CONNECT","host":"192.168.1.10","port":9000}  
>   {"cmd":"AUTH","mode":"login","user":"quy\_hust","pass":"secret123"}  
>   {"cmd":"CREATE\_ROOM","base\_sec":300,"inc\_sec":2,"color":"random"}  
>   {"cmd":"JOIN\_ROOM","room\_id":101}  
>   {"cmd":"MOVE","from":12,"to":28,"promo":0}  
>   {"cmd":"ACTION","type":"resign"}  
>   {"cmd":"GET\_HISTORY","page":1}  
>   {"cmd":"GET\_MATCH\_DETAIL","match\_id":42}  
>   {"cmd":"DISCONNECT"}

> * **Chiều C Sidecar → UI (stdout của C):**  
>   {"evt":"NET\_STATUS","state":"ENCRYPTED\_READY","rtt\_ms":4}  
>   {"evt":"AUTH\_OK","user\_id":7,"elo":1200}  
>   {"evt":"GAME\_START","match\_id":42,"color":"white","opponent":"player2","base\_ms":300000,"inc\_ms":2000}  
>   {"evt":"MOVE\_APPLIED","from":12,"to":28,"promo":0,"w\_ms":298500,"b\_ms":300000,"check":false}  
>   {"evt":"MOVE\_REJECTED","reason":"King would be in check"}  
>   {"evt":"GAME\_OVER","result":"white\_win","reason":"checkmate","new\_elo":1216}  
>   {"evt":"NET\_ERROR","code":"CONNECTION\_LOST","msg":"Server closed connection"}

> * **Lưu ý kỹ thuật cho C:** Luôn gọi fflush(stdout) ngay sau mỗi lệnh fprintf(stdout, ...) và chỉ ghi log debug ra stderr để không làm hỏng luồng parse JSON phía Electron.

## **5\. Đặc tả Lõi Cờ vua & Cơ sở dữ liệu (Server-Side)**

### **5.1. Cấu trúc dữ liệu Bàn cờ (chess\_engine.h)**

Sử dụng biểu diễn mảng 1 chiều 64 phần tử (ô 0 \= a1, ô 7 \= h1, ô 56 \= a8, ô 63 \= h8):

// Mã quân cờ: Dương \= Trắng, Âm \= Đen, 0 \= Ô trống  
typedef enum {  
    EMPTY \= 0,  
    W\_PAWN \= 1, W\_KNIGHT \= 2, W\_BISHOP \= 3, W\_ROOK \= 4, W\_QUEEN \= 5, W\_KING \= 6,  
    B\_PAWN \= \-1, B\_KNIGHT \= \-2, B\_BISHOP \= \-3, B\_ROOK \= \-4, B\_QUEEN \= \-5, B\_KING \= \-6  
} Piece;

typedef struct {  
    int8\_t   squares\[64\];  
    uint8\_t  turn;               // 0 \= Trắng, 1 \= Đen  
    uint8\_t  castling\_rights;    // Bitmask: 0x01=WK, 0x02=WQ, 0x04=BK, 0x08=BQ  
    int8\_t   en\_passant\_sq;      // 0..63 nếu vừa có tốt đi 2 ô, ngược lại \= \-1  
    uint16\_t halfmove\_clock;     // Đếm nước không ăn quân/không đi tốt (luật 50 nước)  
    uint16\_t fullmove\_number;  
    uint64\_t position\_hashes\[512\]; // Zobrist hash các thế cờ để kiểm tra lặp 3 lần  
    uint16\_t hash\_count;  
} ChessBoard;

### **5.2. Ma trận kiểm duyệt luật cờ vua bắt buộc**

Hàm validate\_and\_apply\_move(ChessBoard \*b, Move m) phải kiểm tra theo đúng thứ tự:

> 1. **Kiểm tra hợp lệ cơ bản:** Ô from và to nằm trong 0..63; ô from chứa quân cùng màu với b-\>turn; ô to không chứa quân cùng màu.  
> 2. **Kiểm tra hình học & vật cản:**  
   * Tốt: đi thẳng 1 ô (ô đích trống), đi thẳng 2 ô từ hàng 2/7 (cả 2 ô trống), ăn chéo 1 ô (có quân địch hoặc trùng en\_passant\_sq), bắt buộc có promo hợp lệ khi đến hàng 8/1.  
   * Xe/Tượng/Hậu: kiểm tra đường đi thẳng/chéo không bị quân khác chắn giữa đường.  
   * Mã: kiểm tra độ lệch (dx, dy) là (1,2) hoặc (2,1) (chú ý chống wrap-around mép bàn cờ bằng tọa độ file \= sq % 8, rank \= sq / 8).  
   * Vua & Nhập thành: nếu Vua đi ngang 2 ô, kiểm tra cờ castling\_rights, các ô giữa trống, Vua hiện không bị chiếu, ô Vua đi qua và ô Vua đáp không bị khống chế bởi is\_square\_attacked().  
> 3. **Kiểm tra an toàn Vua (Pin & Check):** Tạo bản sao ChessBoard tmp \= \*b, thực hiện nước đi trên tmp, tìm vị trí Vua phe mình và gọi is\_square\_attacked(\&tmp, king\_sq, \!b-\>turn). Nếu trả về true → từ chối nước đi (MOVE\_REJECT).  
> 4. **Cập nhật trạng thái & Xét kết thúc:** Tính Zobrist Hash mới, kiểm tra chiếu bí / hết nước đi / thiếu quân / luật 50 nước / lặp 3 lần.

### **5.3. Lược đồ Cơ sở dữ liệu SQLite (storage.c)**

CREATE TABLE IF NOT EXISTS users (  
    id            INTEGER PRIMARY KEY AUTOINCREMENT,  
    username      TEXT UNIQUE NOT NULL,  
    password\_hash TEXT NOT NULL,      \-- Lưu dạng hex của SHA-256(salt \+ password)  
    elo           INTEGER DEFAULT 1200,  
    created\_at    INTEGER NOT NULL  
);

CREATE TABLE IF NOT EXISTS matches (  
    id            INTEGER PRIMARY KEY AUTOINCREMENT,  
    white\_user\_id INTEGER NOT NULL,  
    black\_user\_id INTEGER NOT NULL,  
    base\_time\_sec INTEGER NOT NULL,  
    inc\_sec       INTEGER NOT NULL,  
    result        INTEGER NOT NULL,   \-- 0: Draw, 1: WhiteWin, 2: BlackWin  
    reason        INTEGER NOT NULL,   \-- Mã lý do kết thúc  
    started\_at    INTEGER NOT NULL,  
    ended\_at      INTEGER NOT NULL,  
    moves\_blob    BLOB NOT NULL,      \-- Mảng nhị phân các nước đi (3 bytes/nước)  
    move\_count    INTEGER NOT NULL,  
    FOREIGN KEY(white\_user\_id) REFERENCES users(id),  
    FOREIGN KEY(black\_user\_id) REFERENCES users(id)  
);

## **6\. Đặc tả Giao diện Native (Electron UI) & Đóng gói triển khai**

### **6.1. Cấu trúc màn hình & Trải nghiệm Native**

> * **Cửa sổ Frameless:** Khởi tạo BrowserWindow({ width: 1200, height: 800, minWidth: 1000, minHeight: 700, frame: false, backgroundColor: '\#1e1e24' }). Tự thiết kế thanh tiêu đề (Custom Titlebar) cao 36px với thuộc tính CSS \-webkit-app-region: drag và các nút Minimize / Maximize / Close ở góc phải.  
> * **4 màn hình chính (Single Page App chuyển đổi bằng state):**  
  1. **Màn hình Kết nối & Đăng nhập:** Nhập địa chỉ IP Server (mặc định 127.0.0.1:9000 hoặc IP LAN), hiển thị đèn trạng thái kết nối \+ chỉ số Ping (ms) đo từ Heartbeat.  
  2. **Sảnh chờ (Lobby):** Danh sách phòng đang chờ (tự động cập nhật), form Tạo phòng nhanh (chọn Time Control, màu quân), nút chuyển sang Lịch sử & Cài đặt Theme.  
  3. **Bàn cờ thi đấu (Match View):** Bàn cờ 8x8 ở trung tâm (tự động xoay theo màu quân của người chơi), 2 đồng hồ đếm ngược cỡ lớn kèm tên & Elo hai bên, bảng danh sách nước đi (ký hiệu đại số như 1\. e4 e5), nút Đầu hàng / Cầu hòa, hiệu ứng âm thanh (move, capture, check, game\_end).  
  4. **Trình xem lại lịch sử (History Replay):** Danh sách các ván đã đánh; khi chọn 1 ván sẽ tải mảng nước đi từ Server và cho phép bấm \|\<, \<, \>, \>\| để đi qua từng nước cờ trên bàn cờ.  
> * **Hệ thống Theme:** Quản lý bằng CSS Custom Properties (data-theme="wood" \| "dark" \| "green" \| "neon" trên thẻ \<html\>), điều khiển màu ô sáng/tối, màu viền bàn cờ, màu highlight nước đi và bộ SVG quân cờ tương ứng.

### **6.2. Quy trình biên dịch, lưu trữ & khởi động bằng Shortcut**

> 1. **Biên dịch C Client Sidecar:**  
   * Trên Windows (MinGW-w64 / GCC): gcc \-O2 client-core/\*.c shared/\*.c \-o client-core/bin/chess\_client.exe \-lws2\_32  
   * Trên Linux: gcc \-O2 client-core/\*.c shared/\*.c \-o client-core/bin/chess\_client \-lpthread  
> 2. **Đóng gói bộ cài đặt bằng electron-builder:**  
   * File thực thi C được khai báo trong extraResources để copy nguyên vẹn vào thư mục resources/bin/ bên trong gói cài đặt (không bị nén vào app.asar vì hệ điều hành cần thực thi trực tiếp file binary).  
   * Đường dẫn gọi tiến trình C trong main.js tự động phân biệt môi trường Dev và Production:  
     const isDev \= \!app.isPackaged;  
     const binName \= process.platform \=== 'win32' ? 'chess\_client.exe' : 'chess\_client';  
     const binPath \= isDev  
       ? path.join(\_\_dirname, '..', 'client-core', 'bin', binName)  
       : path.join(process.resourcesPath, 'bin', binName);

> 3. **Bố trí trên máy người dùng cuối:**  
   * Người dùng chạy NetChess-Setup-1.0.0.exe (NSIS Installer).  
   * Chương trình tự cài vào %LOCALAPPDATA%\\Programs\\NetChess\\ và tạo **Desktop Shortcut** \+ **Start Menu Shortcut**.  
   * Khi double-click Shortcut, NetChess.exe khởi chạy cửa sổ Native và âm thầm spawn(binPath, \[\], { windowsHide: true }). Khi đóng cửa sổ, sự kiện app.on('before-quit') gửi lệnh DISCONNECT và gọi cProcess.kill() để dọn sạch tiến trình nền.

## **7\. Kế hoạch triển khai chi tiết (WBS) & Kịch bản kiểm thử**

### **7.1. Lộ trình thực hiện (5 Tuần)**

| Tuần | Giai đoạn | Công việc cụ thể (Tasks) | Tiêu chí nghiệm thu (Acceptance Criteria) |
| :---- | :---- | :---- | :---- |
| **Tuần 1** | **Tầng Mạng C & Giao thức** | \- Viết protocol.h, framing.c (send\_all, recv\_all, buffer tách gói). \- Viết crypto.c (DH \+ AES-128-CTR \+ CRC32). \- Viết khung server/main.c, net\_loop.c (poll() non-blocking) và client-core/net\_client.c. | \- 2 chương trình C chạy CLI kết nối thành công qua localhost & LAN. \- Bắt gói tin bằng Wireshark xác nhận payload đã mã hóa. \- Test gửi liên tục 500 gói tin không delay: nhận đủ 500 gói, 0 lỗi dính gói. |
| **Tuần 2** | **Lõi Cờ vua & CSDL Server** | \- Viết chess\_engine.c đủ toàn bộ luật cờ vua. \- Viết room\_manager.c (ghép phòng, quản lý đồng hồ 100ms tick). \- Viết storage.c (SQLite3 lưu user & match). | \- Chạy bộ Unit Test C vượt qua 100% các thế cờ đặc biệt (nhập thành, En Passant, phong cấp, chiếu bí, hòa cờ). \- Kiểm tra đồng hồ hết giờ tự gửi GAME\_OVER và ghi bản ghi vào file .db. |
| **Tuần 3** | **Cầu nối IPC & Khung Electron** | \- Viết client-core/ipc\_bridge.c (parse/serialize JSON Lines). \- Khởi tạo project Electron (main.js, preload.js, frameless window). \- Nối luồng Login, Lobby, Create/Join Room từ UI tới Server. | \- Mở app Electron tự động chạy ngầm chess\_client. \- 2 cửa sổ Electron đăng nhập 2 tài khoản khác nhau, tạo phòng và vào chung 1 ván đấu thành công. |
| **Tuần 4** | **UI Cờ vua, Theme & Replay** | \- Hoàn thiện board\_ui.js (kéo-thả, highlight, popup chọn quân phong cấp). \- Hoàn thiện clock\_ui.js, âm thanh, 4 bộ Theme CSS. \- Hoàn thiện màn hình history\_ui.js (Replay ván đấu). | \- Chơi trọn vẹn nhiều ván cờ thực tế giữa 2 máy tính trong mạng LAN. \- Chuyển đổi 4 theme mượt mà, khởi động lại app vẫn nhớ theme đã chọn. \- Xem lại được chính xác từng nước đi của ván vừa đánh. |
| **Tuần 5** | **Đóng gói, WebSocket (Tùy chọn) & Bảo vệ** | \- (Tùy chọn) Bổ sung Port 9001 Raw WebSocket Handshake trong C Server. \- Cấu hình electron-builder xuất bộ cài .exe / .AppImage. \- Chạy toàn bộ Test Case, chuẩn bị kịch bản demo Wireshark. | \- Cài đặt NetChess-Setup.exe lên máy Windows trắng (không có GCC/Node), chạy bằng Desktop Shortcut ổn định. \- Rút dây mạng / tắt đột ngột 1 máy: Server phát hiện sau ≤15s và xử lý kết thúc ván đúng quy định. |

### **7.2. Bộ kịch bản kiểm thử trọng tâm (Test Cases)**

| Mã TC | Hạng mục | Kịch bản kiểm thử | Kết quả kỳ vọng |
| :---- | :---- | :---- | :---- |
| **TC-01** | TCP Framing | Client gửi 2 gói MOVE\_REQ và HEARTBEAT gộp trong cùng 1 lệnh send() (giả lập dính gói) hoặc tách 1 gói thành 3 lần send() nhỏ 5 bytes (giả lập phân mảnh). | Server ghép đúng buffer, tách đủ 2 gói hợp lệ, không crash, không sai CRC32. |
| **TC-02** | Bảo mật | Dùng Wireshark capture luồng đăng nhập AUTH\_REQ và nước đi MOVE\_REQ trên port 9000\. | Không thấy plaintext username, password hay tọa độ nước đi; gửi 2 gói giống hệt nhau tạo ra 2 chuỗi byte mã hóa khác nhau. |
| **TC-03** | Toàn vẹn | Sửa 1 bit bất kỳ trong gói tin trước khi gửi lên Server. | Server kiểm tra CRC32 thất bại, từ chối gói tin và ghi log cảnh báo. |
| **TC-04** | Luật cờ vua | Thử đi quân đang bị ghim (pinned) làm lộ Vua; thử nhập thành khi ô Vua đi qua đang bị Tượng địch chiếu; thử ăn tốt qua đường sau khi đã qua 1 lượt khác. | Server đều trả về 0x23 MOVE\_REJECT, bàn cờ trên cả 2 Client giữ nguyên trạng thái cũ. |
| **TC-05** | Đồng hồ | Người chơi Trắng không đi quân cho đến khi đồng hồ về 00:00. | Server chủ động phát 0x25 GAME\_OVER (BlackWin, Timeout) dù cả 2 Client không gửi thêm gói tin nào. |
| **TC-06** | Mất kết nối | Đang giữa ván, đóng đột ngột (kill process) ứng dụng phía máy Đen. | Trong vòng tối đa 15 giây, Server phát hiện mất Heartbeat / EOF socket, gửi GAME\_OVER (WhiteWin, Disconnect) cho máy Trắng và lưu lịch sử ván đấu. |

---

