
import socket
import struct
import hashlib
import time

HOST = "127.0.0.1"
PORT = 10000

MAGIC = 0x544C
VERSION = 1
MSG_TYPE = 1

FRAME_COUNT = 1000
FRAME_SIZE = 1024
TOTAL_SIZE = FRAME_COUNT * FRAME_SIZE


def make_payload(index):
    return bytes(
        (index + j) % 256
        for j in range(FRAME_SIZE)
    )


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
            raise RuntimeError(
                f"Server disconnected: "
                f"expected={size}, received={len(data)}"
            )

        data.extend(chunk)

    return bytes(data)


def main():

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(60)

    send_hash = hashlib.sha256()
    recv_hash = hashlib.sha256()

    send_bytes = 0
    recv_bytes = 0

    try:
        sock.connect((HOST, PORT))

        print("[INFO] Connected")
        print("[INFO] Frame count:", FRAME_COUNT)
        print("[INFO] Frame size:", FRAME_SIZE)

        # 第一阶段：连续发送 1000 帧
        for index in range(FRAME_COUNT):

            payload = make_payload(index)
            packet = make_packet(payload)

            sock.sendall(packet)

            send_hash.update(payload)
            send_bytes += len(payload)

        print("[SEND] Bytes:", send_bytes)

        # 第二阶段：接收 1000 帧响应
        for index in range(FRAME_COUNT):

            header = recv_exact(sock, 8)

            magic, version, msg_type, length = struct.unpack(
                "!HBBI", header
            )

            assert magic == MAGIC, f"Invalid magic at frame {index}"
            assert version == VERSION
            assert msg_type == MSG_TYPE
            assert length == FRAME_SIZE

            payload = recv_exact(sock, length)

            expected = make_payload(index)

            assert payload == expected, (
                f"Payload mismatch at frame {index}"
            )

            recv_hash.update(payload)
            recv_bytes += len(payload)

        print("[RECV] Bytes:", recv_bytes)

        send_digest = send_hash.hexdigest()
        recv_digest = recv_hash.hexdigest()

        print("[SEND] SHA256:", send_digest)
        print("[RECV] SHA256:", recv_digest)

        assert send_bytes == TOTAL_SIZE
        assert recv_bytes == TOTAL_SIZE
        assert send_digest == recv_digest

        print("[PASS] TCP 1000 frames test")

    finally:
        sock.close()


if __name__ == "__main__":
    main()

