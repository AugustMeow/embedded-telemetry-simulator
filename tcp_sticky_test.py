
import socket
import struct

HOST = "127.0.0.1"
PORT = 10000

MAGIC = 0x544C
VERSION = 1
MSG_TYPE = 1

payloads = [b"HELLO", b"WORLD", b"TEST"]

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

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect((HOST, PORT))

# 一次性发送三条消息
packets = [make_packet(p) for p in payloads]
sock.sendall(b"".join(packets))

# 依次接收三条响应
for index, expected in enumerate(payloads):

    header = recv_exact(sock, 8)

    magic, version, msg_type, length = struct.unpack(
        "!HBBI", header
    )

    body = recv_exact(sock, length)

    assert magic == MAGIC
    assert version == VERSION
    assert msg_type == MSG_TYPE
    assert body == expected

    print(f"Message {index + 1}: {body}")

print("TCP sticky packet test PASS")

sock.close()

