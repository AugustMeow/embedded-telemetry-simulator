
import socket
import struct

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


def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5)
    sock.connect((HOST, PORT))

    print("[A] Connected")

    sock.sendall(make_packet(b"HELLO"))
    print("[A] Sent HELLO")

    # 只关闭发送方向
    sock.shutdown(socket.SHUT_WR)
    print("[A] Shutdown write side")

    header = recv_exact(sock, 8)

    magic, version, msg_type, length = struct.unpack(
        "!HBBI", header
    )

    assert magic == MAGIC
    assert version == VERSION
    assert msg_type == MSG_TYPE

    payload = recv_exact(sock, length)
    assert payload == b"HELLO"

    # 服务端完成发送后应该关闭连接
    assert sock.recv(1) == b""

    print("[PASS] TCP half-close test")

    sock.close()


if __name__ == "__main__":
    main()

