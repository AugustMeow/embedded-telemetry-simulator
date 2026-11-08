import socket
import struct

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
    data = bytearray()

    while len(data) < size:
        chunk = sock.recv(size - len(data))

        if not chunk:
            raise RuntimeError("Server disconnected unexpectedly")

        data.extend(chunk)

    return bytes(data)


def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5)

    sock.connect((HOST, PORT))
    print("[INFO] Connected")

    # 1. 发送完整协议消息
    payload = b"HELLO"
    packet = make_packet(payload)

    sock.sendall(packet)
    print("[SEND] HELLO")

    # 2. 关闭客户端发送方向，但保留接收方向
    sock.shutdown(socket.SHUT_WR)
    #sock.close()
    print("[FIN] Client write side closed")

    # 3. 接收服务端响应
    header = recv_exact(sock, 8)

    magic, version, msg_type, length = struct.unpack(
        "!HBBI", header
    )

    assert magic == MAGIC
    assert version == VERSION
    assert msg_type == MSG_TYPE

    response = recv_exact(sock, length)

    print("[RECV]", response)

    assert response == payload

    # 4. 服务端完成响应后，应该关闭连接
    remaining = sock.recv(1)

    assert remaining == b""

    print("[PASS] TCP half-close test")

    sock.close()


if __name__ == "__main__":
    main()
