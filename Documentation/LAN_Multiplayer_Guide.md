# Hướng dẫn Chơi cờ qua Mạng LAN / Wi-Fi & Xử lý Đa Server

Tài liệu này giải thích chi tiết cơ chế hoạt động mạng và các bước thiết lập để chạy **Server NetChess trên Laptop A** và kết nối thi đấu từ **Client trên Laptop B** trong cùng một mạng Wi-Fi nội bộ.

---

## 1. Cơ chế Hoạt động Mạng Socket

Trong mã nguồn C của NetChess (`server/src/network.c`), socket máy chủ được khởi tạo như sau:

```c
struct sockaddr_in addr;
memset(&addr, 0, sizeof(addr));
addr.sin_family = AF_INET;
addr.sin_addr.s_addr = INADDR_ANY;  /* Lắng nghe trên mọi giao diện mạng (0.0.0.0) */
addr.sin_port = htons((uint16_t)port); /* Cổng mặc định 8888 */

bind(fd, (struct sockaddr *)&addr, sizeof(addr));
listen(fd, 16);
```

- **`INADDR_ANY` (`0.0.0.0`)**: Cho phép máy chủ chấp nhận kết nối TCP từ:
  1. Giao diện Loopback nội bộ (`127.0.0.1` / `localhost`).
  2. Giao diện mạng Wi-Fi LAN (ví dụ: `192.168.1.15`).
  3. Giao diện mạng dây Ethernet (ví dụ: `10.0.0.5`).

Do đó, bất kỳ thiết bị nào nằm chung mạng Wi-Fi với Laptop A đều có thể kết nối trực tiếp vào Server thông qua địa chỉ IP của Laptop A.

---

## 2. Các Bước Thiết lập Chơi giữa 2 Laptop qua Wi-Fi

### Bước 1: Lấy địa chỉ IP của Laptop A (Máy chạy Server)
1. Trên **Laptop A**, mở Command Prompt (`cmd`) hoặc PowerShell.
2. Gõ lệnh:
   ```cmd
   ipconfig
   ```
3. Tìm phần card mạng Wi-Fi (`Wireless LAN adapter Wi-Fi`), ghi nhớ dòng **`IPv4 Address`**.
   - *Ví dụ*: `192.168.1.15`

### Bước 2: Cho phép Cổng 8888 qua Windows Firewall trên Laptop A
Hệ điều hành Windows mặc định chặn các kết nối Inbound từ bên ngoài vào port ứng dụng lạ. Bạn chỉ cần cho phép cổng 8888 một lần duy nhất bằng lệnh PowerShell sau:
1. Mở PowerShell với quyền **Run as Administrator** trên Laptop A.
2. Dán dòng lệnh sau và nhấn Enter:
   ```powershell
   New-NetFirewallRule -DisplayName "NetChess Server" -Direction Inbound -LocalPort 8888 -Protocol TCP -Action Allow
   ```

### Bước 3: Khởi động Server trên Laptop A
Nhấp đúp chuột vào file `run_server.bat` trên Laptop A (hoặc gõ `./server 8888` trong WSL/Linux).
Màn hình console sẽ hiển thị:
```text
[+] Server initialized successfully on port 8888.
[*] Server listening on port 8888 with select() I/O multiplexing...
```

### Bước 4: Kết nối từ Laptop B
1. Chép thư mục project hoặc chạy file `run_client.bat` trên **Laptop B** (hoặc mở trực tiếp file `client/ui/index.html` bằng Microsoft Edge / Google Chrome).
2. Tại màn hình Đăng nhập của giao diện NetChess:
   - Trong ô **Địa chỉ Máy chủ (IP Server)**: Nhập IP của Laptop A kèm cổng, ví dụ:
     ```text
     192.168.1.15:8888
     ```
   - Nhấn nút **Đổi máy chủ**.
3. Góc trên bên phải giao diện sẽ chuyển sang:
   - `Đã kết nối (192.168.1.15:8888)` với chấm xanh lá cây.
4. Người chơi trên Laptop B đăng nhập tài khoản (ví dụ `user2` / `12345`), Laptop A đăng nhập tài khoản (`user1` / `12345`), danh sách người chơi rỗi sẽ lập tức đồng bộ thời gian thực!

---

## 3. Xử lý Trường hợp Mở 2 Server Cùng lúc (Port Collision)

### A. Bản chất Lập trình Mạng Socket
Theo chuẩn giao thức TCP/IP của hệ điều hành, một cặp `(IP, Port)` chỉ có thể được sở hữu bởi duy nhất **một socket lắng nghe (listening socket)** tại một thời điểm.

Nếu tiến trình Server 2 cố tình gọi hàm `bind()` vào cổng `8888` mà Server 1 đang nắm giữ, hàm `bind()` sẽ trả về mã lỗi:
- Trên Windows: `SOCKET_ERROR` với `WSAGetLastError() == WSAEADDRINUSE` (10048).
- Trên Linux/POSIX: `-1` với `errno == EADDRINUSE` (98).

### B. Cơ chế Xử lý An toàn trong NetChess
Mã nguồn NetChess (`server/src/network.c`) đã được thiết kế cơ chế bắt lỗi chuẩn mực:

```c
if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    fprintf(stderr, "[ERROR] Port %d is already in use by another server instance.\n", port);
    fprintf(stderr, "        Please terminate the existing server or specify a different port (e.g. server.exe 8889).\n");
    CLOSE_SOCK(fd);
    return INVALID_SOCK;
}
```

- Khi mở Server thứ hai, Server này sẽ **không bị crash hay treo**, mà in ngay thông báo lỗi tường minh và thoát an toàn, bảo vệ tuyệt đối Server 1 đang phục vụ các ván cờ.
- **Nếu muốn chạy song song 2 Server** (ví dụ để test 2 cụm mạng hoặc 2 phòng riêng biệt):
  Người dùng chỉ cần truyền tham số cổng khác khi khởi động:
  ```cmd
  server.exe 8889
  ```
  Khi đó Server 2 sẽ chạy độc lập trên cổng 8889 và client có thể kết nối tới `IP:8889`.
