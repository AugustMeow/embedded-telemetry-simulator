
import socket
import struct
import hashlib
import threading
import time

HOST = "127.0.0.1"
PORT = 10000

MAGIC = 0x544C
VERSION = 1
MSG_TYPE = 1

FRAME_SIZE = 4096
TOTAL_SIZE = 48 * 1024

def make_payload(index, length):
    return bytes((index + j) % 256 for j in range(length))

def make_packet(payload):
    header = struct.pack(
        "!HBBI",
        MAGIC,
        VERSION,
        MSG_TYPE,
        len(payload)
    )
    return header + payload

def recv_exact(sock, size):
    data = bytearray()

    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            raise RuntimeError("Server disconnected")
        data.extend(chunk)

    return bytes(data)

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

# 尽量缩小客户端接收缓冲区
sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
sock.settimeout(60)

sock.connect((HOST, PORT))

frame_count = (TOTAL_SIZE + FRAME_SIZE - 1) // FRAME_SIZE

send_hash = hashlib.sha256()
recv_hash = hashlib.sha256()

def sender():
    remaining = TOTAL_SIZE

    for index in range(frame_count):
        length = min(FRAME_SIZE, remaining)

        payload = make_payload(index, length)
        sock.sendall(make_packet(payload))

        send_hash.update(payload)
        remaining -= length

print("[INFO] Start sending")

thread = threading.Thread(target=sender)
thread.start()

# 等待发送线程完成
thread.join()

print("[INFO] Send completed")
print("[INFO] Pause receiving for 3 seconds")

# 暂停读取，制造接收端背压
time.sleep(3)

print("[INFO] Start receiving")

received = 0

for index in range(frame_count):
    header = recv_exact(sock, 8)

    magic, version, msg_type, length = struct.unpack(
        "!HBBI", header
    )

    assert magic == MAGIC
    assert version == VERSION
    assert msg_type == MSG_TYPE

    payload = recv_exact(sock, length)

    expected_length = min(
        FRAME_SIZE,
        TOTAL_SIZE - index * FRAME_SIZE
    )

    assert length == expected_length

    recv_hash.update(payload)
    received += len(payload)

thread.join()

print("[SEND]", TOTAL_SIZE)
print("[RECV]", received)
print("[SEND SHA256]", send_hash.hexdigest())
print("[RECV SHA256]", recv_hash.hexdigest())

assert received == TOTAL_SIZE
assert send_hash.digest() == recv_hash.digest()

print("[PASS] Slow client test")

sock.close()

