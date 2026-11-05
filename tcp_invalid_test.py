
import socket
import struct
import time

HOST = "127.0.0.1"
PORT = 10000

MAGIC = 0x544C
VERSION = 1
MSG_TYPE = 1

def make_header(magic, version, msg_type, length):
    return struct.pack(
        "!HBBI",
        magic,
        version,
        msg_type,
        length
    )

def test_invalid(name, header):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(3)

    sock.connect((HOST, PORT))

    print(f"[{name}] Sending invalid header")

    sock.sendall(header)

    try:
        data = sock.recv(1024)

        if data == b"":
            print(f"[{name}] PASS: server closed connection")
        else:
            print(f"[{name}] FAIL: unexpected response:", data)

    except socket.timeout:
        print(f"[{name}] FAIL: server did not close connection")

    finally:
        sock.close()


# Case 1: MAGIC 错误
test_invalid(
    "INVALID_MAGIC",
    make_header(0x1234, VERSION, MSG_TYPE, 5)
)

time.sleep(0.2)

# Case 2: VERSION 错误
test_invalid(
    "INVALID_VERSION",
    make_header(MAGIC, 2, MSG_TYPE, 5)
)

time.sleep(0.2)

# Case 3: LENGTH 超过最大限制
test_invalid(
    "INVALID_LENGTH",
    make_header(MAGIC, VERSION, MSG_TYPE, 4097)
)

print("Invalid protocol test finished")

