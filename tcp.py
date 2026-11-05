import socket
import struct

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

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect((HOST, PORT))

sock.sendall(packet)

response = b""

while len(response) < 8:
    data = sock.recv(8 - len(response))
    if not data:
        raise RuntimeError("Server disconnected")
    response += data

magic, version, msg_type, length = struct.unpack("!HBBI", response)

body = b""

while len(body) < length:
    data = sock.recv(length - len(body))
    if not data:
        raise RuntimeError("Server disconnected")
    body += data

print("Header:", magic, version, msg_type, length)
print("Payload:", body)

assert magic == MAGIC
assert version == VERSION
assert msg_type == MSG_TYPE
assert body == payload

print("TCP protocol echo PASS")

sock.close()
