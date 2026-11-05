
import socket
import struct
import time

HOST = "127.0.0.1"
PORT = 10000

MAGIC = 0x544C
VERSION = 1
MSG_TYPE = 1

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
    data = b""

    while len(data) < size:
        chunk = sock.recv(size - len(data))

        if not chunk:
            raise RuntimeError("Server disconnected")

        data += chunk

    return data


# 1. 先创建非法客户端
invalid_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
invalid_sock.settimeout(3)
invalid_sock.connect((HOST, PORT))

# 2. 再创建正常客户端
normal_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
normal_sock.settimeout(3)
normal_sock.connect((HOST, PORT))

# 给服务端一点时间完成 accept
time.sleep(0.2)

# 3. 两个客户端分别发送数据
invalid_header = struct.pack(
    "!HBBI",
    0x1234,       # 错误 MAGIC
    VERSION,
    MSG_TYPE,
    5
)

invalid_sock.sendall(invalid_header)

payload = b"HELLO_NORMAL"
normal_sock.sendall(make_packet(payload))

# 4. 检查非法客户端被关闭
invalid_data = invalid_sock.recv(1024)

assert invalid_data == b""

print("[INVALID] Connection closed PASS")

# 5. 检查正常客户端仍然能收到完整响应
response_header = recv_exact(normal_sock, 8)

magic, version, msg_type, length = struct.unpack(
    "!HBBI", response_header
)

response_payload = recv_exact(normal_sock, length)

assert magic == MAGIC
assert version == VERSION
assert msg_type == MSG_TYPE
assert response_payload == payload

print("[NORMAL] Received:", response_payload)
print("[NORMAL] PASS")

invalid_sock.close()
normal_sock.close()

print("Invalid client isolation test PASS")

