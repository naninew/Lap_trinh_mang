# NetChess Deployment & User Guide

This guide details how to build, deploy, and operate the NetChess platform on both **Windows** and **Linux/WSL**.

---

## 1. Prerequisites

### Windows
- **GCC compiler**: Installed via MinGW-w64, MSYS2, or TDM-GCC.
- **Web Browser**: Microsoft Edge or Google Chrome (pre-installed on all Windows systems).
- **NO Node.js, NO npm, NO Python needed!**

### Linux / WSL (Ubuntu, Kali Linux)
- Standard development tools: `build-essential` (`gcc`, `make`).
- Standard web browser or Edge/Chrome.

---

## 2. Building the Project

### On Windows (One-Click)
Double-click `build.bat` in the project root folder, or run in Command Prompt:
```cmd
build.bat
```
This builds `server/server.exe` using GCC and links with `ws2_32`.

### On Linux / WSL (Academic Makefile)
Open terminal in the project root and run:
```bash
make clean && make all
```
This generates the `server/server` binary and the `client/client_cli/client_cli` binary.

---

## 3. Running NetChess

### Step 1: Start the Server
- **Windows**: Double-click `run_server.bat` (or run `server\server.exe 8888`).
- **Linux/WSL**: Run `./server/server 8888`.
*The server will initialize on port 8888 and wait for client connections.*

### Step 2: Launch Player 1
- **Windows**: Double-click `run_client.bat`.
  *(Launches a clean native-like window with the Lichess-style wooden chess board).*
- **Linux/WSL / Any Browser**: Open `client/ui/index.html` directly in Edge or Chrome.

### Step 3: Launch Player 2
- Repeat Step 2 in another window to simulate or connect the second player.

---

## 4. User Gameplay Walkthrough

1. **Register & Login**:
   - Window 1: Register username `alice`, password `123`, then click **Sign In**.
   - Window 2: Register username `bob`, password `123`, then click **Sign In**.
2. **View 3-Tier Lobby**:
   - Both windows display their own user profile, rating, and rank.
   - Available players are grouped into 3 tiers (sorted from highest rank to lowest within each tier):
     * **Tier 1 (Top)**: Online and idle opponents eligible to challenge (rank difference $\le 10$). The **Challenge** button is active.
     * **Tier 2 (Middle)**: Online opponents with rank difference $> 10$ or currently in game. Button is disabled with explanatory reason (e.g., `Rank Diff: 15 (>10)`).
     * **Tier 3 (Bottom)**: Registered accounts currently offline, styled with a subtle dimmed gray background.
3. **Send Challenge & Time Control**:
   - Window 1 clicks **Challenge** next to `bob`.
   - A modal allows selecting between **Rapid (10 mins)** and **Classical (60 mins)**.
   - Window 2 receives an incoming challenge notification with time mode and opponent stats.
   - Window 2 clicks **Accept**.
4. **Gameplay & Visual Indicators**:
   - Both boards transition to the match view with real-time countdown clocks.
   - White moves first. Clicking a piece queries the C server for legal moves, highlighted in semi-transparent green.
   - Previous move squares are highlighted in harmonious semi-transparent gray.
   - When a King is placed in check, the square under the checked King illuminates with a radial red glow filter.
   - Castling moves automatically shift both King and Rook synchronously across both screens via authoritative C server board state synchronization.
5. **Post-Game Review & Log**:
   - Either player can click **Resign** (loss $-3$ pts, winner $+3$ pts) or achieve checkmate.
   - A **Game Over** dialog displays the match summary and the complete match report log (`.log`).
   - Clicking **Download Log (.log)** saves the match report file locally.
   - Clicking **OK (Review Board)** closes the popup and leaves the board, clocks, and live move history open for inspection without allowing further moves.
   - Clicking **Return to Lobby** returns the player to the lobby.
