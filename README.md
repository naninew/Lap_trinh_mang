# NetChess - Online Chess Platform in C

Online multiplayer chess system implemented for **IT4062 - Network Programming (HUST)**.
Features 100% pure C networking and game logic, coupled with a zero-dependency Lichess-inspired light wooden UI.

## Features
- **User Authentication**: Register & login with persistent storage in `accounts.txt`.
- **Dynamic Leaderboard & Scoring**: Win = +3, Draw = 0, Loss = -3.
- **Matchmaking with Rank Constraint**: Players can only challenge opponents within $\pm 10$ ranks ($|Rank_A - Rank_B| \le 10$).
- **Pure C Chess Engine**: Full rule enforcement, check/checkmate/stalemate detection.
- **Resignation & Rematch**: Players can resign/forfeit (-3 pts) and immediately return to the idle pool.
- **Match Log File Transfer**: Server streams complete match reports over TCP to both clients.
- **Zero-Dependency UI**: Vanilla HTML/CSS/JS, runs instantly without npm or Node.js.

## Directory Structure
```
Lap_trinh_mang/
├── Documentation/        # Detailed technical specifications and guides
├── server/               # C Server source, headers, Makefile, and accounts.txt
├── client/
│   ├── client_cli/       # Pure C CLI socket client
│   └── ui/               # Minimalist zero-dependency Lichess-style UI
├── test/                 # Automated C engine unit tests and protocol tests
├── build.bat             # One-click Windows build script
├── run_server.bat        # One-click Windows server runner
├── run_client.bat        # One-click Windows client runner
└── Makefile              # Standard academic Makefile for Linux/WSL
```

See [DeployGuide.md](file:///d:/HUST_Study/2026.1/TH_lap_trinh_mang/Project_nhom/Lap_trinh_mang/Documentation/DeployGuide.md) for detailed deployment instructions.
