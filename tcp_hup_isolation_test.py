
import socket
import struct
import hashlib
import time

HOST = "127.0.0.1"
PORT = 10000

MAGIC = 0x544C
VERSION = 1
MSG_TYPE = 1


def make_packet(payload):
    return struct.pack(
        "!HBBI", MAGIC, VERSION, MSG_TYPE, len(payload)
    ) + payload


def recv_exact(sock, size):
    data = bytearray()

    while len(data) < size:
        chunk = sock.recv(size - len(data))

        if not chunk:
            raise RuntimeError("Server disconnected")

        data.extend(chunk)

    return bytes(data)


def test_normal_client():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(10)
    sock.connect((HOST, PORT))

    payload = b"HELLO_NORMAL_CLIENT"

    sock.sendall(make_packet(payload))

    header = recv_exact(sock, 8)

    magic, version, msg_type, length = struct.unpack(
        "!HBBI", header
    )

    assert magic == MAGIC
    assert version == VERSION
    assert msg_type == MSG_TYPE

    response = recv_exact(sock, length)

    assert response == payload

    send_hash = hashlib.sha256(payload).hexdigest()
    recv_hash = hashlib.sha256(response).hexdigest()

    assert send_hash == recv_hash

    print("[NORMAL] Received:", response)
    print("[NORMAL] SHA256:", recv_hash)
    print("[NORMAL] PASS")

    sock.close()


def main():

    # A：异常关闭客户端
    a = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    a.connect((HOST, PORT))

    a.sendall(make_packet(b"HELLO_ABNORMAL_CLIENT"))

    print("[A] Sent data, closing connection")
    a.close()

    # 等待服务端处理 A 的断开
    time.sleep(0.2)

    # B：正常客户端
    test_normal_client()

    print("[PASS] HUP client isolation test")


if __name__ == "__main__":
    main()

