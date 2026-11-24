#!/usr/bin/env python3
import socket
import json
import time
import subprocess
import os
import sys

PORT = 8889
SERVER_BIN = os.path.join(os.path.dirname(__file__), "..", "server", "server.exe" if os.name == "nt" else "server")

class TcpClient:
    def __init__(self, host, port):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((host, port))
        self.sock.setblocking(False)
        self.buffer = ""

    def send(self, data):
        msg = json.dumps(data) + "\n"
        self.sock.sendall(msg.encode("utf-8"))

    def recv_action(self, expected_action, timeout=3.0):
        start = time.time()
        while time.time() - start < timeout:
            # Check if we already have a complete line
            if "\n" in self.buffer:
                line, self.buffer = self.buffer.split("\n", 1)
                line = line.strip()
                if line:
                    try:
                        data = json.loads(line)
                        if data.get("action") == expected_action:
                            return data
                    except json.JSONDecodeError:
                        pass
            try:
                chunk = self.sock.recv(2048)
                if chunk:
                    self.buffer += chunk.decode("utf-8", errors="ignore")
                else:
                    break
            except (BlockingIOError, socket.error):
                time.sleep(0.05)
        return None

    def close(self):
        try:
            self.sock.close()
        except:
            pass

def run_tests():
    print("==================================================")
    print("   RUNNING AUTOMATED NETCHESS PROTOCOL TESTS      ")
    print("==================================================")

    # 1. Compile server if not built
    if not os.path.exists(SERVER_BIN):
        print(f"[*] Compiling server binary: {SERVER_BIN}...")
        src_dir = os.path.join(os.path.dirname(__file__), "..", "server")
        if os.name == "nt":
            cmd = 'gcc -O2 -Wall -Iinclude src/*.c -o server.exe -lws2_32'
        else:
            cmd = 'gcc -O2 -Wall -Iinclude src/*.c -o server -lpthread'
        res = subprocess.run(cmd, shell=True, cwd=src_dir)
        if res.returncode != 0:
            print("[FAIL] Could not compile server binary.")
            sys.exit(1)

    # 2. Launch server process
    server_dir = os.path.join(os.path.dirname(__file__), "..", "server")
    print(f"[*] Starting server on port {PORT}...")
    server_proc = subprocess.Popen([SERVER_BIN, str(PORT)], cwd=server_dir, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(1.0)

    clientA = None
    clientB = None
    clientC = None
    try:
        # 3. Connect Client A, Client B, and Client C (grandmaster)
        clientA = TcpClient("127.0.0.1", PORT)
        clientB = TcpClient("127.0.0.1", PORT)
        clientC = TcpClient("127.0.0.1", PORT)

        # Test Login
        print("[*] Testing Login for alice, bob, and grandmaster...")
        clientA.send({"action": "LOGIN", "user": "novice_alice", "pass": "pass123"})
        respA = clientA.recv_action("LOGIN_RESP")
        assert respA and respA.get("status") == "OK", f"Alice login failed: {respA}"
        print(f"[PASS] Alice login successful (Rank: {respA.get('rank')}, Score: {respA.get('score')})")

        clientB.send({"action": "LOGIN", "user": "novice_bob", "pass": "pass123"})
        respB = clientB.recv_action("LOGIN_RESP")
        assert respB and respB.get("status") == "OK", f"Bob login failed: {respB}"
        print(f"[PASS] Bob login successful (Rank: {respB.get('rank')}, Score: {respB.get('score')})")

        clientC.send({"action": "LOGIN", "user": "grandmaster", "pass": "pass123"})
        respC = clientC.recv_action("LOGIN_RESP")
        assert respC and respC.get("status") == "OK", f"Grandmaster login failed: {respC}"
        print(f"[PASS] Grandmaster login successful (Rank: {respC.get('rank')}, Score: {respC.get('score')})")

        # 4. Test Rank Constraint (|Rank_A - Rank_B| <= 10)
        print("[*] Testing Challenge Rank Constraint (Alice Rank 13 vs Grandmaster Rank 1: diff > 10)...")
        clientA.send({"action": "CHALLENGE_REQ", "target": "grandmaster"})
        err_resp = clientA.recv_action("CHALLENGE_ERR")
        assert err_resp, "Expected CHALLENGE_ERR from server"
        assert "exceeding limit of 10" in err_resp.get("msg", ""), f"Error msg mismatch: {err_resp}"
        print(f"[PASS] Server correctly rejected challenge outside +/- 10 rank range: {err_resp.get('msg')}")

        # 5. Test Legitimate Challenge with Time Mode (Alice challenges Bob, 10 min)
        print("[*] Testing Legitimate Challenge (Alice challenges Bob, Rapid 10 min)...")
        clientA.send({"action": "CHALLENGE_REQ", "target": "novice_bob", "time_mode": 10})
        notifB = clientB.recv_action("CHALLENGE_NOTIFY")
        assert notifB and notifB.get("from") == "novice_alice", f"Bob notification mismatch: {notifB}"
        assert notifB.get("time_mode") == 10, f"Expected time_mode 10, got {notifB.get('time_mode')}"
        print(f"[PASS] Bob received incoming challenge (mode: {notifB.get('time_mode')}m) from: {notifB.get('from')}")

        # Bob accepts challenge
        print("[*] Bob accepts challenge...")
        clientB.send({"action": "CHALLENGE_REPLY", "from": "novice_alice", "accept": True})
        startA = clientA.recv_action("GAME_START")
        startB = clientB.recv_action("GAME_START")
        assert startA and startB, "Both players must receive GAME_START"
        assert {startA.get("color"), startB.get("color")} == {"white", "black"}, f"Invalid color assignment: A={startA.get('color')}, B={startB.get('color')}"
        if startA.get("color") == "white":
            white_client, black_client = clientA, clientB
            white_user, black_user = "novice_alice", "novice_bob"
        else:
            white_client, black_client = clientB, clientA
            white_user, black_user = "novice_bob", "novice_alice"
        assert startA.get("time_control") == 600, f"Expected 600s time control, got {startA.get('time_control')}"
        print(f"[PASS] Match started! (White: {white_user}, Black: {black_user}, Clock: {startA.get('time_control')}s)")

        # 5b. Test Server-Side Legal Move Generation (GET_VALID_MOVES for e2: sq 12)
        print(f"[*] Testing GET_VALID_MOVES for e2 (sq 12) by White ({white_user})...")
        white_client.send({"action": "GET_VALID_MOVES", "sq": 12})
        vm = white_client.recv_action("VALID_MOVES")
        assert vm and vm.get("from") == 12, f"Expected VALID_MOVES for sq 12, got: {vm}"
        assert 20 in vm.get("dests", []) and 28 in vm.get("dests", []), f"Missing e3/e4 in dests: {vm}"
        print(f"[PASS] C Server calculated legal moves for e2: {vm.get('dests')}")

        # 6. Test Move Exchange (White plays e2 -> e4: 12 -> 28)
        print(f"[*] Testing Move Exchange (e2 -> e4) by White ({white_user})...")
        white_client.send({"action": "MOVE_REQ", "from": 12, "to": 28, "promo": 0})
        moveA = clientA.recv_action("MOVE_BROADCAST")
        moveB = clientB.recv_action("MOVE_BROADCAST")
        assert moveA and moveB, "Both players must receive MOVE_BROADCAST"
        assert moveA.get("from") == 12 and moveA.get("to") == 28
        assert "w_time" in moveA and "b_time" in moveA, "Expected clock times in broadcast"
        assert "w_caps" in moveA and "b_caps" in moveA, "Expected captured trays in broadcast"
        assert "board" in moveA and len(moveA["board"]) == 64, "Expected authoritative 64 board squares in broadcast"
        assert moveA["board"][28] == 1, "Expected white pawn at sq 28 (e4)"
        print(f"[PASS] Move e2-e4 validated with authoritative board sync! Clocks: W={moveA.get('w_time')}s, B={moveA.get('b_time')}s")

        # 7. Test Resignation ("Xin ngừng ván hiện tại")
        print("[*] Testing Resignation (Alice resigns)...")
        clientA.send({"action": "RESIGN"})
        goA = clientA.recv_action("GAME_OVER")
        goB = clientB.recv_action("GAME_OVER")
        assert goA and goB, "Both players must receive GAME_OVER"
        assert goB.get("winner") == "novice_bob"
        assert goB.get("score_delta") == 3
        print(f"[PASS] Resignation processed correctly: Winner={goB.get('winner')}, Delta={goB.get('score_delta')}")

        # 8. Test Match Log File Transfer
        print("[*] Testing End-of-game Match Log File Transfer...")
        logA = clientA.recv_action("LOG_FILE")
        logB = clientB.recv_action("LOG_FILE")
        assert logA and logB, "Both players must receive LOG_FILE"
        assert "MATCH REPORT LOG" in logA.get("content", "")
        assert f"White Player:    {white_user}" in logA.get("content", "")
        assert f"Black Player:    {black_user}" in logA.get("content", "")
        print("[PASS] Complete match log file transmitted to both players via TCP socket!")

        # 8b. Verify random White/Black assignment across repeated matches
        seen_colors = {startA.get("color")}
        for i in range(20):
            if seen_colors == {"white", "black"}:
                break
            clientA.send({"action": "CHALLENGE_REQ", "target": "novice_bob", "time_mode": 10})
            assert clientB.recv_action("CHALLENGE_NOTIFY"), "Bob did not receive repeated challenge"
            clientB.send({"action": "CHALLENGE_REPLY", "from": "novice_alice", "accept": True})
            sA = clientA.recv_action("GAME_START")
            sB = clientB.recv_action("GAME_START")
            assert sA and sB and {sA.get("color"), sB.get("color")} == {"white", "black"}
            seen_colors.add(sA.get("color"))
            # Alternate resigner to keep ratings balanced
            resigner = clientB if (i % 2 == 0) else clientA
            resigner.send({"action": "RESIGN"})
            assert clientA.recv_action("GAME_OVER") and clientB.recv_action("GAME_OVER")
            assert clientA.recv_action("LOG_FILE") and clientB.recv_action("LOG_FILE")
        assert seen_colors == {"white", "black"}, f"Expected challenger to receive both colors randomly, only saw: {seen_colors}"
        print(f"[PASS] Verified random color assignment across matches (challenger got both white & black)")

        # 9. Test Concurrency / KICKED frame on duplicate login
        print("[*] Testing KICKED frame on duplicate login (Device 2 logs in as novice_alice)...")
        clientA_dup = TcpClient("127.0.0.1", PORT)
        clientA_dup.send({"action": "LOGIN", "user": "novice_alice", "pass": "pass123"})
        kicked_msg = clientA.recv_action("KICKED")
        assert kicked_msg, "Expected KICKED frame sent to stale session"
        assert "Account signed in from another device" in kicked_msg.get("reason", ""), f"Reason mismatch: {kicked_msg}"
        print(f"[PASS] Stale session successfully received English KICKED frame: {kicked_msg.get('reason')}")
        clientA_dup.close()

        # 10. Test LOGOUT action
        print("[*] Testing LOGOUT action (Bob logs out)...")
        clientB.send({"action": "LOGOUT"})
        logout_resp = clientB.recv_action("LOGOUT_RESP")
        assert logout_resp and logout_resp.get("status") == "OK", f"Expected LOGOUT_RESP: {logout_resp}"
        print("[PASS] User logged out successfully and received LOGOUT_RESP!")

        print("==================================================")
        print("    ALL AUTOMATED PROTOCOL TESTS PASSED 100%!     ")
        print("==================================================")
    finally:
        if clientA: clientA.close()
        if clientB: clientB.close()
        if clientC: clientC.close()
        server_proc.terminate()
        server_proc.wait()

if __name__ == "__main__":
    run_tests()
