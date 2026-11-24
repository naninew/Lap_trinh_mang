# Network Programming Techniques Mapping

This document explicitly details how the NetChess system applies the concepts taught in the **IT4062 - Network Programming** curriculum.

---

## 1. Sockets & Berkeley Socket API
- **Socket Initialization**: Uses standard `socket(AF_INET, SOCK_STREAM, 0)`.
- **Address Binding & Listening**: Configures `sockaddr_in`, binds to port `8888`, and calls `listen(server_fd, SOMAXCONN)`.
- **Cross-Platform Compatibility**: Uses conditional compilation for Windows (`ws2_32.lib`, `WSAStartup`) and Linux POSIX sockets, allowing identical logic to run on both OSes.

---

## 2. I/O Multiplexing with `select()`
In accordance with Week 8 & Week 9 course topics:
- The server runs a non-blocking event-driven loop centered around `select()`.
- A master file descriptor set (`fd_set master_fds`) tracks the listening socket and all active client connections.
- Each iteration creates a copy `read_fds = master_fds` and passes it to `select(max_fd + 1, &read_fds, NULL, NULL, &timeout)`.
- When `FD_ISSET(listen_fd, &read_fds)` fires, new client connections are accepted via `accept()`.
- When `FD_ISSET(client_fd, &read_fds)` fires, client data is received and parsed without blocking other clients.

---

## 3. Advanced I/O: Non-blocking Sockets & Stream Handling
- **Non-blocking Mode**: Sockets are configured to non-blocking via `ioctlsocket(fd, FIONBIO, &mode)` (Windows) or `fcntl(fd, F_SETFL, O_NONBLOCK)` (Linux).
- **Partial I/O Handling (`send_all` / `recv_all`)**: Solves TCP stream fragmentation by looping until all bytes are transmitted or received, handling `EAGAIN` / `EWOULDBLOCK` gracefully.
- **Dynamic Stream Buffering**: Each client session maintains an input buffer (`rx_buf`, `rx_len`) to safely accumulate fragmented incoming frames before parsing.

---

## 4. Concurrency & Multithreading
- **Session Protection**: Critical shared data structures (`Account`, `Session`, `Room`) are protected to prevent race conditions.
- **Clock & Heartbeat Processing**: Integrated with non-blocking timeouts in the `select()` loop (100ms tick), providing precise timer coordination without thread bloat.

---

## 5. File Transfer Over TCP Socket
Applying concepts from Week 4 and Week 7/8 file transfer assignments:
- At the end of every match, the server generates a structured log file in `server/logs/match_<id>.log`.
- The file content is read from disk into memory buffers.
- The file payload is transferred across the socket connection to both players.
- Client saves the received file in `client/logs/` and presents it visually to the user.
