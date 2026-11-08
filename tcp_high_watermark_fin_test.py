
import socket
import struct
import hashlib
import time

HOST = "127.0.0.1"
PORT = 10000

MAGIC = 0x544C
VERSION = 1
MSG_TYPE = 1

FRAME_COUNT = 96
FRAME_SIZE = 1024

# 暂停接收的时间
PAUSE_TIME = 2


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

    sock = socket.socket(
    socket.AF_INET,
    socket.SOCK_STREAM)

# 限制客户端 TCP 接收缓冲区
    sock.setsockopt(
    socket.SOL_SOCKET,
    socket.SO_RCVBUF,
    4096
    )

    sock.settimeout(60)

    print(
        "[INFO] Client SO_RCVBUF:",
        sock.getsockopt(
            socket.SOL_SOCKET,
            socket.SO_RCVBUF
        )
    )
    send_hash = hashlib.sha256()
    recv_hash = hashlib.sha256()

    send_count = 0
    recv_count = 0

    total_size = FRAME_COUNT * FRAME_SIZE

    try:
        # 1. 建立连接
        sock.connect((HOST, PORT))

        print("[INFO] Connected")
        print("[INFO] Frame count:", FRAME_COUNT)
        print("[INFO] Frame size:", FRAME_SIZE)
        print("[INFO] Total payload:", total_size)

        # 2. 连续发送协议帧
        for index in range(FRAME_COUNT):

            payload = make_payload(index)
            packet = make_packet(payload)

            sock.sendall(packet)

            send_hash.update(payload)
            send_count += len(payload)

        print("[SEND] Completed:", send_count)

        # 3. 客户端发送 FIN
        sock.shutdown(socket.SHUT_WR)

        print("[FIN] Client write side closed")

        # 4. 暂停接收，让服务端发送缓冲区产生积压
        print(
            f"[PAUSE] Stop receiving for {PAUSE_TIME} seconds"
        )

        time.sleep(PAUSE_TIME)

        # 5. 开始接收服务端响应
        print("[RECV] Start receiving")

        for index in range(FRAME_COUNT):

            header = recv_exact(sock, 8)

            magic, version, msg_type, length = struct.unpack(
                "!HBBI", header
            )

            assert magic == MAGIC
            assert version == VERSION
            assert msg_type == MSG_TYPE
            assert length == FRAME_SIZE

            payload = recv_exact(sock, length)

            expected = make_payload(index)

            assert payload == expected, (
                f"Payload mismatch at frame {index}"
            )

            recv_hash.update(payload)
            recv_count += len(payload)

        print("[RECV] Completed:", recv_count)

        # 6. 验证 SHA256
        send_digest = send_hash.hexdigest()
        recv_digest = recv_hash.hexdigest()

        print("[SEND] SHA256:", send_digest)
        print("[RECV] SHA256:", recv_digest)

        assert send_count == total_size
        assert recv_count == total_size

        assert send_digest == recv_digest

        print("[PASS] Data integrity verified")

        # 7. 服务端应该在发送完毕后关闭连接
        remaining = sock.recv(1)

        assert remaining == b"", (
            "Expected server to close connection"
        )

        print("[PASS] Server closed connection after draining")
        print("[PASS] High watermark FIN test")

    finally:
        sock.close()


if __name__ == "__main__":
    main()

