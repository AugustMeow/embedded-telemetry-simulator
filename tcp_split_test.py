
import socket
import struct
import time

HOST = "127.0.0.1"
PORT = 10000

MAGIC = 0x544C
VERSION = 1
MSG_TYPE = 1

payload = b"HELLO"

header = struct.pack(
    "!HBBI",
    MAGIC,
    VERSION,
    MSG_TYPE,
    len(payload)
)

packet = header + payload

def recv_exact(sock, size):
    data = b""

    while len(data) < size:
        chunk = sock.recv(size - len(data))

        if not chunk:
            raise RuntimeError("Server disconnected")

        data += chunk

    return data


sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect((HOST, PORT))

# 第一部分：Header 前 3 字节
sock.sendall(packet[:3])
print("[SEND 1]", packet[:3].hex(" "))

time.sleep(0.2)

# 第二部分：Header 剩余部分 + Payload 第 1 字节
sock.sendall(packet[3:9])
print("[SEND 2]", packet[3:9].hex(" "))

time.sleep(0.2)

# 第三部分：剩余 Payload
sock.sendall(packet[9:])
print("[SEND 3]", packet[9:].hex(" "))

# 接收完整响应
response_header = recv_exact(sock, 8)

magic, version, msg_type, length = struct.unpack(
    "!HBBI", response_header
)

response_payload = recv_exact(sock, length)

print("[RECV HEADER]", response_header.hex(" "))
print("[RECV PAYLOAD]", response_payload)

assert magic == MAGIC
assert version == VERSION
assert msg_type == MSG_TYPE
assert length == len(payload)
assert response_payload == payload

print("[PASS] TCP split packet test")

sock.close()

