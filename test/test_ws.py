import socket, hashlib, base64, json, time, subprocess, os

PORT = 8899

def ws_handshake(sock):
    key = base64.b64encode(b'1234567890123456').decode()
    req = (f'GET / HTTP/1.1\r\nHost: 127.0.0.1:{PORT}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n').encode()
    sock.sendall(req)
    resp = sock.recv(1024).decode()
    assert '101' in resp, f"Handshake failed: {resp}"

def ws_send(sock, obj):
    payload = json.dumps(obj).encode()
    mask = b'\x12\x34\x56\x78'
    masked = bytes([b ^ mask[i % 4] for i, b in enumerate(payload)])
    header = bytes([0x81, 0x80 | len(payload)]) + mask
    sock.sendall(header + masked)

def ws_recv(sock, timeout=2.0):
    sock.settimeout(timeout)
    hdr = sock.recv(2)
    if not hdr: return None
    plen = hdr[1] & 0x7F
    if plen == 126:
        ext = sock.recv(2)
        plen = int.from_bytes(ext, 'big')
    elif plen == 127:
        ext = sock.recv(8)
        plen = int.from_bytes(ext, 'big')
    buf = b""
    while len(buf) < plen:
        chunk = sock.recv(plen - len(buf))
        if not chunk: break
        buf += chunk
    return json.loads(buf.decode('utf-8'))

server_bin = os.path.join(os.path.dirname(__file__), "..", "server", "server")
server_proc = subprocess.Popen([server_bin, str(PORT)], cwd=os.path.join(os.path.dirname(__file__), "..", "server"))
time.sleep(0.5)

try:
    s1 = socket.socket()
    s1.connect(('127.0.0.1', PORT))
    ws_handshake(s1)
    ws_send(s1, {'action': 'LOGIN', 'user': 'user1', 'pass': '12345'})
    print('s1 login resp:', ws_recv(s1))
    print('s1 idle list:', ws_recv(s1))

    s2 = socket.socket()
    s2.connect(('127.0.0.1', PORT))
    ws_handshake(s2)
    ws_send(s2, {'action': 'LOGIN', 'user': 'user2', 'pass': '12345'})
    print('s2 login resp:', ws_recv(s2))
    print('s2 idle list:', ws_recv(s2))

    s1.settimeout(1.0)
    try:
        print('s1 received after s2 login:', ws_recv(s1))
    except Exception as e:
        print('s1 failed to receive:', e)

finally:
    server_proc.terminate()
