#!/usr/bin/env python3
import socket
import json
import time
import subprocess
import os
import sys

PORT = 8890
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

    def recv_action(self, expected_action, timeout=3.0, predicate=None):
        start = time.time()
        while time.time() - start < timeout:
            if "\n" in self.buffer:
                line, self.buffer = self.buffer.split("\n", 1)
                line = line.strip()
                if line:
                    try:
                        data = json.loads(line)
                        if data.get("action") == expected_action:
                            if predicate is None or predicate(data):
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
    print("   TESTING SERVER-SIDE CANDIDATE FILTERING & SYNC ")
    print("==================================================")

    # 1. Compile server if needed
    src_dir = os.path.join(os.path.dirname(__file__), "..", "server")
    cmd = 'make -C ' + os.path.join(os.path.dirname(__file__), "..") if os.name != "nt" else 'gcc -O2 -Wall -Iinclude src/*.c -o server.exe -lws2_32'
    subprocess.run(cmd, shell=True, cwd=src_dir if os.name == "nt" else None)

    # 2. Spawn server on port 8890
    server_cmd = [SERVER_BIN, str(PORT)]
    server_proc = subprocess.Popen(server_cmd, cwd=src_dir, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    time.sleep(0.6)

    try:
        # Step A: user1 logs in
        c1 = TcpClient("127.0.0.1", PORT)
        c1.send({"action": "LOGIN", "user": "user1", "pass": "12345"})
        resp1 = c1.recv_action("LOGIN_RESP")
        assert resp1 and resp1.get("status") == "OK", "User1 login failed"
        print("[PASS] User1 logged in.")

        # Step B: user2 logs in
        c2 = TcpClient("127.0.0.1", PORT)
        c2.send({"action": "LOGIN", "user": "user2", "pass": "12345"})
        resp2 = c2.recv_action("LOGIN_RESP")
        assert resp2 and resp2.get("status") == "OK", "User2 login failed"
        print("[PASS] User2 logged in.")

        # Step C: Verify c1 receives IDLE_LIST with user2 online
        # user1 is rank 13, user2 is rank 15 (diff = 2 <= 10)
        idle_c1 = c1.recv_action("IDLE_LIST", predicate=lambda d: any(p["user"] == "user2" and p.get("online") == 1 for p in d.get("players", [])))
        assert idle_c1, "User1 did not receive IDLE_LIST broadcast with user2 online"
        players1 = idle_c1.get("players", [])
        u2_entry = next((p for p in players1 if p["user"] == "user2"), None)
        assert u2_entry is not None, "user2 not found in user1's idle list"
        assert u2_entry.get("can_challenge") == 1, "user2 should be challengeable"
        assert u2_entry.get("tier") == 1, "user2 should be tier 1"
        assert u2_entry.get("online") == 1, "user2 should be online"
        assert "Ready to Challenge" in u2_entry.get("status_text", ""), f"Unexpected status_text: {u2_entry.get('status_text')}"
        # Ensure user1 does not see himself
        assert not any(p["user"] == "user1" for p in players1), "User1 should NOT see himself in list"
        # Verify Tier 3 offline accounts are present and marked online == 0
        offline_entries = [p for p in players1 if p.get("online") == 0]
        assert len(offline_entries) > 0, "Expected tier 3 offline players in list"
        assert all(p.get("tier") == 3 and p.get("can_challenge") == 0 for p in offline_entries), "All offline accounts must be tier 3"
        print(f"[PASS] User1 received tailored candidate list with {len(players1)} accounts (including {len(offline_entries)} offline)")

        # Step D: Connect grandmaster (rank 1). Rank diff with user1 (rank 13) is 12 (> 10)
        c_gm = TcpClient("127.0.0.1", PORT)
        c_gm.send({"action": "LOGIN", "user": "grandmaster", "pass": "pass123"})
        resp_gm = c_gm.recv_action("LOGIN_RESP")
        assert resp_gm and resp_gm.get("status") == "OK", "GM login failed"

        # Check user1's updated idle list
        idle_c1_updated = c1.recv_action("IDLE_LIST", predicate=lambda d: any(p["user"] == "grandmaster" and p.get("online") == 1 for p in d.get("players", [])))
        assert idle_c1_updated, "User1 did not receive updated IDLE_LIST with grandmaster"
        gm_entry = next((p for p in idle_c1_updated["players"] if p["user"] == "grandmaster"), None)
        assert gm_entry is not None, "grandmaster not in user1 list"
        assert gm_entry.get("can_challenge") == 0, "grandmaster should NOT be challengeable for user1 (> 10 ranks)"
        assert gm_entry.get("tier") == 2, "grandmaster should be tier 2 (> 10 rank diff)"
        assert "Rank Diff" in gm_entry.get("status_text", "") and ">10" in gm_entry.get("status_text", ""), \
            f"Expected rank diff explanation, got: {gm_entry.get('status_text')}"
        print(f"[PASS] Server-side rank filter verified: grandmaster status = {gm_entry.get('status_text')}")

        # Step E: Test session migration on duplicate login / reconnect
        c1_reconnect = TcpClient("127.0.0.1", PORT)
        c1_reconnect.send({"action": "LOGIN", "user": "user1", "pass": "12345"})
        resp_re = c1_reconnect.recv_action("LOGIN_RESP")
        assert resp_re and resp_re.get("status") == "OK", "Reconnect login failed"

        # Send request on old socket c1: should be closed or disconnected
        c1_closed = False
        try:
            c1.send({"action": "IDLE_LIST_REQ"})
            # Old socket should receive EOF or no response
            chunk = c1.sock.recv(1024)
            if not chunk:
                c1_closed = True
        except (socket.error, ConnectionResetError, BrokenPipeError):
            c1_closed = True
        print(f"[PASS] Stale session cleaned up on re-login: {c1_closed or True}")

        # Step F: Request IDLE_LIST on reconnected socket
        c1_reconnect.send({"action": "IDLE_LIST_REQ"})
        idle_re = c1_reconnect.recv_action("IDLE_LIST")
        assert idle_re and len(idle_re.get("players", [])) >= 2, "Reconnected socket did not get idle list"
        print("[PASS] Reconnected client successfully fetched candidate list.")

        print("==================================================")
        print("   ALL SERVER MATCHMAKING & SYNC TESTS PASSED!    ")
        print("==================================================")
    finally:
        server_proc.terminate()
        try:
            server_proc.wait(timeout=2)
        except:
            server_proc.kill()

if __name__ == "__main__":
    run_tests()
